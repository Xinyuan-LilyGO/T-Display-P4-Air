/*
 * @Description: rf_test
 * @Author: LILYGO_L
 * @Date: 2026-02-04 11:01:39
 * @LastEditTime: 2026-07-26 13:02:10
 * @License: GPL 3.0
 */
#include "lilygo_device_driver_library.h"

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "esp_hosted.h"
#include "esp_hosted_transport_config.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "stsw_st25rfal002_cpp_bus_driver_library.h"

namespace {

usp_cpp_bus_driver::Lr11xx* g_lr1121 = nullptr;
cpp_bus_driver::Nrf9151* g_nrf9151 = nullptr;
std::shared_ptr<cpp_bus_driver::HardwareUart> g_nrf9151_uart;
TaskHandle_t g_nrf9151_task_handle = nullptr;
TaskHandle_t g_nrf9151_button_task_handle = nullptr;
std::atomic_bool g_nrf9151_nmea_passthrough{false};

constexpr TickType_t kLr1121FrequencyHoldTicks = pdMS_TO_TICKS(500);
constexpr TickType_t kLr1121FrequencyRestTicks = pdMS_TO_TICKS(1000);
constexpr uint32_t kNrf9151CommandTimeoutMs = 5000;
constexpr size_t kNrf9151NmeaBufferLimit = 4096;
constexpr TickType_t kNrf9151LteTransmitTicks = pdMS_TO_TICKS(5000);
constexpr TickType_t kNrf9151LteRestTicks = pdMS_TO_TICKS(1000);
constexpr TickType_t kNrf9151ButtonPollTicks = pdMS_TO_TICKS(10);
constexpr TickType_t kNrf9151ButtonDebounceTicks = pdMS_TO_TICKS(300);
constexpr int kNrf9151TestButton =
    lilygo_device_driver::t_display_p4_air::gpio::button::kEsp32p4Boot;

esp_err_t SetEsp32c5ResetLevel(void* context, bool level) {
  auto* driver = static_cast<
      lilygo_device_driver::TDisplayP4AirDriver*>(context);
  return driver != nullptr && driver->SetEsp32c5PowerEnabled(level)
      ? ESP_OK
      : ESP_FAIL;
}

bool RegisterEsp32c5ResetCallback(
    lilygo_device_driver::TDisplayP4AirDriver* driver) {
  return esp_hosted_sdio_set_reset_callback(
             SetEsp32c5ResetLevel, driver) == ESP_TRANSPORT_OK;
}

enum class Nrf9151TestMode {
  kLte,
  kGnss,
};

struct Nrf9151LteTestConfig {
  const char* band_name;
  const char* tx_command;
};

constexpr Nrf9151LteTestConfig kNrf9151LteTestConfigs[] = {
    {"B28, 710.0 mhz", "AT%XRFTEST=1,1,28,7100,23"},
    {"B8, 898.0 mhz", "AT%XRFTEST=1,1,8,8980,23"},
    {"B3, 1745.0 mhz", "AT%XRFTEST=1,1,3,17450,23"},
    {"B1, 1950.0 mhz", "AT%XRFTEST=1,1,1,19500,23"},
};

/**
 * @brief 输出射频测试日志，GNSS 原始数据直通期间自动丢弃日志
 * @param format printf 格式字符串
 * @return 实际输出的字符数，静默期间返回 0
 */
int RfTestLog(const char* format, ...)
    __attribute__((format(printf, 1, 2)));
int RfTestLog(const char* format, ...) {
  if (g_nrf9151_nmea_passthrough.load(std::memory_order_acquire)) {
    return 0;
  }

  va_list args;
  va_start(args, format);
  int result = vprintf(format, args);
  va_end(args);
  return result;
}

/**
 * @brief 控制 ESP-IDF 日志输出，避免 GNSS 原始数据中混入系统日志
 * @param format vprintf 格式字符串
 * @param args 可变参数列表
 * @return 实际输出的字符数，静默期间返回 0
 */
int RfTestEspLogVprintf(const char* format, va_list args) {
  if (g_nrf9151_nmea_passthrough.load(std::memory_order_acquire)) {
    return 0;
  }
  return vprintf(format, args);
}

/**
 * @brief 设置 nRF9151 NMEA 原始数据直通状态
 * @param enabled true 表示仅输出 NMEA，false 表示恢复普通日志
 */
void SetNrf9151NmeaPassthrough(bool enabled) {
  if (enabled) {
    fflush(stdout);
  }
  g_nrf9151_nmea_passthrough.store(enabled, std::memory_order_release);
}

// 将本文件的 printf 统一接入 GNSS 静默控制。
#define printf(...) RfTestLog(__VA_ARGS__)

bool Lr1121TransmitTest(float freq, float bw, int8_t pwr) {
  const bool high_frequency = freq > 1000.0F;
  usp_cpp_bus_driver::Lr11xx::LoraConfig config = {
      .frequency_hz = static_cast<uint32_t>(freq * 1000000.0F),
      .modulation =
          {
              .sf = LR11XX_RADIO_LORA_SF12,
              .bw = high_frequency ? LR11XX_RADIO_LORA_BW_400
                                   : LR11XX_RADIO_LORA_BW_125,
              .cr = LR11XX_RADIO_LORA_CR_4_7,
              .ldro = high_frequency ? 0 : 1,
          },
      .packet =
          {
              .preamble_len_in_symb = 8,
              .header_type = LR11XX_RADIO_LORA_PKT_EXPLICIT,
              .pld_len_in_bytes = 255,
              .crc = LR11XX_RADIO_LORA_CRC_ON,
              .iq = LR11XX_RADIO_LORA_IQ_STANDARD,
          },
      .sync_word = 0x12,
      .pa =
          {
              .pa_sel = high_frequency ? LR11XX_RADIO_PA_SEL_HF
                                       : LR11XX_RADIO_PA_SEL_HP,
              .pa_reg_supply = high_frequency
                                   ? LR11XX_RADIO_PA_REG_SUPPLY_VREG
                                   : LR11XX_RADIO_PA_REG_SUPPLY_VBAT,
              .pa_duty_cycle = 0x04,
              .pa_hp_sel = 0x07,
          },
      .output_power_dbm = pwr,
      .ramp_time = LR11XX_RADIO_RAMP_48_US,
  };

  const bool result =
      g_lr1121->Invoke(lr11xx_system_calibrate_image_in_mhz,
          static_cast<uint16_t>(freq - 4.0F),
          static_cast<uint16_t>(freq + 4.0F)) == LR11XX_STATUS_OK &&
      g_lr1121->Configure(config) &&
      g_lr1121->Invoke(lr11xx_radio_set_tx_cw) == LR11XX_STATUS_OK;
  if (!result) {
    printf("lr1121 continuous wave failed at %.1f mhz\n", freq);
  } else {
    printf("lr1121 continuous wave: %.1f mhz, %.2f khz, %d dbm\n", freq, bw,
        pwr);
  }

  // 每个频点保持 500 ms，便于频谱仪稳定捕获并读取功率。
  vTaskDelay(kLr1121FrequencyHoldTicks);
  if (g_lr1121->Invoke(
          lr11xx_system_set_standby, LR11XX_SYSTEM_STANDBY_CFG_RC) !=
      LR11XX_STATUS_OK) {
    printf("lr1121 standby failed at %.1f mhz\n", freq);
  }
  // 关闭发射后等待 1 秒，避免切换到下一频点时仍处于发射状态。
  vTaskDelay(kLr1121FrequencyRestTicks);
  return result;
}

void Lr1121RfTask(void* arg) {
  printf("Lr1121RfTask start\n");

  while (1) {
    Lr1121TransmitTest(1900.0, 406.25, 13);
    Lr1121TransmitTest(2200.0, 406.25, 13);
    Lr1121TransmitTest(2400.0, 406.25, 13);
    Lr1121TransmitTest(2500.0, 406.25, 13);
    Lr1121TransmitTest(433.0, 125.0, 22);
    Lr1121TransmitTest(868.0, 125.0, 22);
    Lr1121TransmitTest(915.0, 125.0, 22);
  }
}

void St25r3916RfTask(void* arg) {
  printf("St25r3916RfTask start\n");

  ReturnCode status =
      rfalSetMode(RFAL_MODE_POLL_NFCA, RFAL_BR_106, RFAL_BR_106);
  if (status == RFAL_ERR_NONE) {
    // RFO 电阻设为 0，使用 ST25R3916 的最大射频驱动能力。
    status = rfalChipSetRFO(0);
  }
  if (status == RFAL_ERR_NONE) {
    status = rfalFieldOnAndStartGT();
  }
  if (status != RFAL_ERR_NONE) {
    printf("st25r3916 13.56 mhz field start failed (error code: %d)\n",
        status);
    vTaskDelete(nullptr);
    return;
  }

  printf("st25r3916 continuous field: 13.56 mhz, rfo resistance 0\n");
  while (1) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

bool CheckEspWifiResult(esp_err_t result, const char* operation) {
  if (result == ESP_OK) {
    return true;
  }
  printf("esp32c5 %s failed: %s\n", operation, esp_err_to_name(result));
  return false;
}

void Esp32c5RfTask(void* arg) {
  printf("Esp32c5RfTask (softap beacon power test) start\n");

  uint8_t cycle = 0;

  while (1) {
    cycle = (cycle + 1) % 2;

    if (!CheckEspWifiResult(esp_wifi_stop(), "stop wifi")) {
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }
    vTaskDelay(pdMS_TO_TICKS(200));

    if (!CheckEspWifiResult(esp_wifi_start(), "start wifi")) {
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    bool configured = false;
    if (cycle == 0) {
      printf("mode: 2.4ghz (ch 6)\n");
      configured = CheckEspWifiResult(
                       esp_wifi_set_band_mode(WIFI_BAND_MODE_2G_ONLY),
                       "set 2.4ghz band") &&
          CheckEspWifiResult(
              esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11B),
              "set 802.11b protocol") &&
          CheckEspWifiResult(
              esp_wifi_set_channel(6, WIFI_SECOND_CHAN_NONE), "set channel 6");
    } else {
      printf("mode: 5ghz (ch 36)\n");
      configured = CheckEspWifiResult(
                       esp_wifi_set_band_mode(WIFI_BAND_MODE_5G_ONLY),
                       "set 5ghz band") &&
          CheckEspWifiResult(
              esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11A),
              "set 802.11a protocol") &&
          CheckEspWifiResult(esp_wifi_set_channel(
                                 36, WIFI_SECOND_CHAN_NONE),
              "set channel 36");
    }

    configured = configured && CheckEspWifiResult(
        esp_wifi_set_max_tx_power(80), "set maximum tx power");
    if (!configured) {
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    // SoftAP 信标不依赖 STA 关联和 IP 路由，可持续在当前信道发射。
    printf("esp32c5 %s softap beacon test for 5s\n",
        cycle == 0 ? "2.4ghz" : "5ghz");
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}

void WifiInitAp() {
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_ap();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

  wifi_config_t ap_config = {};
  static constexpr char kSoftApSsid[] = "T-Display-P4-Air-RF";
  std::memcpy(ap_config.ap.ssid, kSoftApSsid, sizeof(kSoftApSsid) - 1);
  ap_config.ap.ssid_len = sizeof(kSoftApSsid) - 1;
  ap_config.ap.channel = 6;
  ap_config.ap.authmode = WIFI_AUTH_OPEN;
  ap_config.ap.max_connection = 1;
  ap_config.ap.beacon_interval = 100;
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));

  const wifi_country_t country = {
      .cc = "CN",
      .schan = 1,
      .nchan = 13,
      .max_tx_power = 80,
      .policy = WIFI_COUNTRY_POLICY_AUTO,
  };
  ESP_ERROR_CHECK(esp_wifi_set_country(&country));
  ESP_ERROR_CHECK(esp_wifi_start());
}

bool SendNrf9151Command(const char* command) {
  std::string response;
  cpp_bus_driver::Nrf9151::CommandResult result = g_nrf9151->SendCommand(
      command, &response, kNrf9151CommandTimeoutMs);
  printf("nrf9151 command: [%s], result: %s, response: [%s]\n", command,
      cpp_bus_driver::Nrf9151::CommandResultToString(result), response.c_str());

  // %XRFTEST 的功率响应使用 q4 dBm，实际 dBm 值等于响应值除以 16。
  const char* tx_power_prefix = "%XRFTEST: ";
  size_t tx_power_pos = response.find(tx_power_prefix);
  if (std::string(command).rfind("AT%XRFTEST=1,1,", 0) == 0 &&
      tx_power_pos != std::string::npos) {
    const char* power_text = response.c_str() + tx_power_pos +
        std::strlen(tx_power_prefix);
    char* end = nullptr;
    long power_q4 = std::strtol(power_text, &end, 10);
    if (end != power_text) {
      printf("nrf9151 tx measured power: %ld / 16 = %.3f dbm\n", power_q4,
          static_cast<double>(power_q4) / 16.0);
    }
  }

  return result == cpp_bus_driver::Nrf9151::CommandResult::kOk;
}

/**
 * @brief 重组 UART 分片并将完整 NMEA 语句原样输出到主串口
 * @param pending_data 用于保存尚未接收完整的 UART 数据
 */
void ForwardNrf9151NmeaOutput(std::string* pending_data) {
  const size_t read_length = g_nrf9151_uart->GetRxBufferLength();
  if (read_length == 0) {
    vTaskDelay(pdMS_TO_TICKS(20));
    return;
  }

  std::vector<uint8_t> buffer(read_length);
  const int32_t bytes_read = g_nrf9151_uart->Read(
      buffer.data(), static_cast<uint32_t>(buffer.size()));
  if (bytes_read <= 0) {
    vTaskDelay(pdMS_TO_TICKS(20));
    return;
  }

  pending_data->append(reinterpret_cast<const char*>(buffer.data()),
      static_cast<size_t>(bytes_read));
  while (true) {
    const size_t line_end = pending_data->find('\n');
    if (line_end == std::string::npos) {
      break;
    }

    const size_t line_length = line_end + 1;
    if (!pending_data->empty() && pending_data->front() == '$') {
      fwrite(pending_data->data(), 1, line_length, stdout);
      fflush(stdout);
    }
    pending_data->erase(0, line_length);
  }

  if (pending_data->size() > kNrf9151NmeaBufferLimit) {
    const size_t nmea_start = pending_data->rfind('$');
    if (nmea_start == std::string::npos) {
      pending_data->clear();
    } else {
      pending_data->erase(0, nmea_start);
    }
  }
}

void Nrf9151ButtonTask(void* arg) {
  pinMode(kNrf9151TestButton, INPUT_PULLUP);

  while (true) {
    if (digitalRead(kNrf9151TestButton) != LOW) {
      vTaskDelay(kNrf9151ButtonPollTicks);
      continue;
    }

    // 按下后等待 300 ms，过滤机械按键抖动。
    vTaskDelay(kNrf9151ButtonDebounceTicks);
    if (digitalRead(kNrf9151TestButton) != LOW) {
      continue;
    }

    // 任务通知会累积，主任务即使正在等待 AT 响应也不会漏掉按键。
    if (g_nrf9151_task_handle != nullptr) {
      xTaskNotifyGive(g_nrf9151_task_handle);
    }

    // 等待按键释放，确保一次按下只产生一次切换事件。
    while (digitalRead(kNrf9151TestButton) == LOW) {
      vTaskDelay(kNrf9151ButtonPollTicks);
    }
    vTaskDelay(kNrf9151ButtonDebounceTicks);
  }
}

// 获取并清除已保存的按键切换事件。
bool IsNrf9151ModeSwitchRequested() {
  return ulTaskNotifyTake(pdTRUE, 0) > 0;
}

// 停止按键任务并清除接收任务句柄，避免向已删除的任务发送通知。
void StopNrf9151ButtonTask() {
  g_nrf9151_task_handle = nullptr;
  if (g_nrf9151_button_task_handle != nullptr) {
    vTaskDelete(g_nrf9151_button_task_handle);
    g_nrf9151_button_task_handle = nullptr;
  }
}

bool WaitForNrf9151ModeSwitch(TickType_t wait_ticks) {
  TickType_t start_ticks = xTaskGetTickCount();
  while ((xTaskGetTickCount() - start_ticks) < wait_ticks) {
    if (IsNrf9151ModeSwitchRequested()) {
      return true;
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
  return false;
}

bool RunNrf9151LteTransmitTests() {
  for (const Nrf9151LteTestConfig& test : kNrf9151LteTestConfigs) {
    if (IsNrf9151ModeSwitchRequested()) {
      return true;
    }

    printf("nrf9151 lte test start: %s, requested power: 23 dbm\n",
        test.band_name);
    if (SendNrf9151Command(test.tx_command)) {
      if (WaitForNrf9151ModeSwitch(kNrf9151LteTransmitTicks)) {
        SendNrf9151Command("AT%XRFTEST=1,0");
        return true;
      }
    }

    SendNrf9151Command("AT%XRFTEST=1,0");
    // 每个 LTE 频段关闭发射后等待 1 秒，再切换到下一频段。
    if (WaitForNrf9151ModeSwitch(kNrf9151LteRestTicks)) {
      return true;
    }
  }
  return false;
}

void Nrf9151AtTask(void* arg) {
  printf("Nrf9151AtTask started\n");
  g_nrf9151_task_handle = xTaskGetCurrentTaskHandle();
  pinMode(kNrf9151TestButton, INPUT_PULLUP);
  if (xTaskCreate(Nrf9151ButtonTask, "Nrf9151ButtonTask", 2 * 1024,
          nullptr, 4, &g_nrf9151_button_task_handle) != pdPASS) {
    printf("nrf9151 button task create failed\n");
    vTaskDelete(nullptr);
    return;
  }
  vTaskDelay(pdMS_TO_TICKS(1000));

  if (!SendNrf9151Command("AT+CFUN=0") ||
      !SendNrf9151Command("AT%XSYSTEMMODE=1,0,0,0")) {
    printf("nrf9151 failed to enter lte test mode\n");
    StopNrf9151ButtonTask();
    vTaskDelete(nullptr);
    return;
  }

  Nrf9151TestMode test_mode = Nrf9151TestMode::kLte;
  printf("nrf9151 test mode: lte, press boot button to switch to gnss\n");

  while (1) {
    if (test_mode == Nrf9151TestMode::kLte) {
      if (RunNrf9151LteTransmitTests()) {
        test_mode = Nrf9151TestMode::kGnss;
        printf("nrf9151 test mode switched to gnss\n");
      }
      continue;
    }

    // 切换到仅 GNSS 模式，按下按键后停止 GNSS 并返回 LTE 测试。
    if (!SendNrf9151Command("AT+CFUN=0") ||
        !SendNrf9151Command("AT%XSYSTEMMODE=0,0,1,0") ||
        !SendNrf9151Command("AT+CFUN=31") ||
        !SendNrf9151Command("AT#XGNSSNMEA=1") ||
        !SendNrf9151Command("AT#XGNSS=1,0,1")) {
      printf("nrf9151 gnss test start failed\n");
      SendNrf9151Command("AT#XGNSSNMEA=0");
      SendNrf9151Command("AT#XGNSS=0");
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    printf("nrf9151 test mode: gnss, press boot button to switch to lte\n");
    std::string pending_nmea_data;
    SetNrf9151NmeaPassthrough(true);
    while (!IsNrf9151ModeSwitchRequested()) {
      ForwardNrf9151NmeaOutput(&pending_nmea_data);
    }

    SendNrf9151Command("AT#XGNSSNMEA=0");
    SendNrf9151Command("AT#XGNSS=0");
    SetNrf9151NmeaPassthrough(false);
    SendNrf9151Command("AT+CFUN=0");
    SendNrf9151Command("AT%XSYSTEMMODE=1,0,0,0");
    test_mode = Nrf9151TestMode::kLte;
    printf("nrf9151 test mode switched to lte\n");
  }
}

void ConfigLr1121(lilygo_device_driver::TDisplayP4AirDriver& driver) {
  if (!driver.IsLr1121Ready() || driver.chip().lr1121 == nullptr) {
    printf("lr1121 init failed\n");
    return;
  }

  g_lr1121 = driver.chip().lr1121.get();
}

}  // namespace

extern "C" void app_main(void) {
  esp_log_set_vprintf(RfTestEspLogVprintf);
  printf("Ciallo\n");

  auto& driver = lilygo_device_driver::TDisplayP4AirDriver::GetInstance();
  if (!driver.InitMinimal() || !driver.InitXl9535()) {
    printf("RF control hardware initialization failed\n");
    return;
  }

  driver.InitLr1121();
  ConfigLr1121(driver);
  const bool st25r3916_ready =
      driver.InitSt25r3916() && driver.IsSt25r3916Ready();
  const bool nrf9151_ready =
      driver.InitNrf9151() && driver.IsNrf9151Ready();
  if (nrf9151_ready) {
    g_nrf9151 = driver.chip().nrf9151.get();
    g_nrf9151_uart = driver.bus().nrf9151_uart_bus;
  } else {
    printf("nrf9151 init failed\n");
  }

  bool esp32c5_ready = driver.SetEsp32c5PowerEnabled(true);
  if (!esp32c5_ready) {
    printf("esp32c5 power enable failed\n");
  } else if (!RegisterEsp32c5ResetCallback(&driver)) {
    printf("register esp32c5 reset callback failed\n");
    esp32c5_ready = false;
  } else {
    const esp_err_t result = esp_hosted_init();
    if (result != ESP_OK) {
      printf("esp_hosted_init failed: %s\n", esp_err_to_name(result));
      esp32c5_ready = false;
    }
  }
  if (esp32c5_ready) {
    WifiInitAp();
  }

  if (g_lr1121 != nullptr) {
    xTaskCreate(Lr1121RfTask, "Lr1121RfTask", 4 * 1024, NULL, 3, NULL);
  }
  if (st25r3916_ready) {
    xTaskCreate(
        St25r3916RfTask, "St25r3916RfTask", 4 * 1024, NULL, 3, NULL);
  } else {
    printf("st25r3916 init failed\n");
  }
  if (esp32c5_ready) {
    xTaskCreate(Esp32c5RfTask, "Esp32c5RfTask", 4 * 1024, NULL, 3, NULL);
  }
  if (g_nrf9151 != nullptr && g_nrf9151_uart != nullptr) {
    xTaskCreate(Nrf9151AtTask, "Nrf9151AtTask", 8 * 1024, NULL, 3, NULL);
  }
}
