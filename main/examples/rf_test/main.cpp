/*
 * @Description: rf_test
 * @Author: LILYGO_L
 * @Date: 2026-02-04 11:01:39
 * @LastEditTime: 2026-06-22 11:13:13
 * @License: GPL 3.0
 */
#include "lilygo_device_driver_library.h"

#include <cstring>
#include <string>
#include <vector>

#include "esp_hosted.h"
#include "esp_wifi.h"
#include "lwip/sockets.h"
#include "st25r3916_driver.h"

namespace {

LR1121* g_lr1121 = nullptr;
std::shared_ptr<cpp_bus_driver::HardwareUart> g_nrf9151_uart;

int16_t Lr1121TransmitTest(float freq, float bw, int8_t pwr) {
  int16_t status = g_lr1121->setFrequency(freq);
  if (status != RADIOLIB_ERR_NONE) {
    printf("lr1121 setFrequency failed at %.1f mhz (error code: %d)\n", freq,
        status);
    return status;
  }

  status = g_lr1121->setBandwidth(bw, freq > 1000.0);
  if (status != RADIOLIB_ERR_NONE) {
    printf("lr1121 setBandwidth failed at %.1f mhz (error code: %d)\n", freq,
        status);
    return status;
  }

  status = g_lr1121->setOutputPower(pwr);
  if (status != RADIOLIB_ERR_NONE) {
    printf("lr1121 setOutputPower failed at %.1f mhz (error code: %d)\n",
        freq, status);
    return status;
  }

  status = g_lr1121->startTransmit("ciallo", 0);
  if (status != RADIOLIB_ERR_NONE) {
    printf("lr1121 startTransmit failed at %.1f mhz (error code: %#X)\n",
        freq, status);
  } else {
    printf("lr1121 startTransmit success at %.1f mhz\n", freq);
  }

  vTaskDelay(pdMS_TO_TICKS(100));
  g_lr1121->standby();
  return status;
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

  while (1) {
    St25r3916_Loop();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void Esp32c5RfTask(void* arg) {
  printf("Esp32c5RfTask (udp max power test) start\n");

  uint8_t cycle = 0;
  char dummy_data[1024];
  memset(dummy_data, 0xA5, sizeof(dummy_data));

  while (1) {
    cycle = (cycle + 1) % 2;

    esp_wifi_stop();
    vTaskDelay(pdMS_TO_TICKS(200));

    if (cycle == 0) {
      printf("mode: 2.4ghz (ch 6)\n");
      esp_wifi_set_band_mode(WIFI_BAND_MODE_2G_ONLY);
      esp_wifi_start();
      esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B);
      esp_wifi_set_channel(6, WIFI_SECOND_CHAN_NONE);
    } else {
      printf("mode: 5ghz (ch 36)\n");
      esp_wifi_set_band_mode(WIFI_BAND_MODE_5G_ONLY);
      esp_wifi_start();
      esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11A);
      esp_wifi_set_channel(36, WIFI_SECOND_CHAN_NONE);
    }

    esp_wifi_set_max_tx_power(80);

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
      printf("socket failed\n");
      continue;
    }

    struct sockaddr_in dest_addr = {};
    dest_addr.sin_addr.s_addr = inet_addr("255.255.255.255");
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(12345);

    printf("esp32c5 starting tx stress test for 5s\n");
    uint32_t start_time = xTaskGetTickCount();
    while (xTaskGetTickCount() - start_time < pdMS_TO_TICKS(5000)) {
      sendto(sock, dummy_data, sizeof(dummy_data), 0,
          reinterpret_cast<struct sockaddr*>(&dest_addr), sizeof(dest_addr));
      vTaskDelay(pdMS_TO_TICKS(10));
    }

    close(sock);
    printf("esp32c5 %s test finish\n", cycle == 0 ? "2.4ghz" : "5ghz");
  }
}

void WifiInitSta() {
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

  wifi_config_t empty_conf = {};
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &empty_conf));

  const wifi_country_t country = {
      .cc = "CN",
      .schan = 1,
      .nchan = 13,
      .max_tx_power = 80,
      .policy = WIFI_COUNTRY_POLICY_AUTO,
  };
  esp_wifi_set_country(&country);
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_start());
}

std::string SendAtCommand(const std::string& cmd, int timeout_ms = 3000) {
  std::string full_cmd = cmd + "\r\n";
  g_nrf9151_uart->ClearRxBufferData();
  g_nrf9151_uart->Write(
      reinterpret_cast<const uint8_t*>(full_cmd.c_str()), full_cmd.length());

  std::string response;
  uint32_t start = xTaskGetTickCount();
  while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(timeout_ms)) {
    size_t length = g_nrf9151_uart->GetRxBufferLength();
    if (length > 0) {
      std::vector<uint8_t> buffer(length + 1, 0);
      g_nrf9151_uart->Read(buffer.data(), length);
      response.append(reinterpret_cast<char*>(buffer.data()), length);
      if (response.find("OK") != std::string::npos ||
          response.find("ERROR") != std::string::npos) {
        break;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  while (!response.empty() &&
         (response.back() == '\r' || response.back() == '\n')) {
    response.pop_back();
  }
  return response;
}

bool CheckSimCard() {
  std::string response = SendAtCommand("AT+CPIN?");
  return response.find("+CPIN: READY") != std::string::npos;
}

bool IsNetworkRegistered() {
  std::string response = SendAtCommand("AT+CEREG?");
  size_t pos = response.find("+CEREG:");
  if (pos == std::string::npos) {
    return false;
  }

  pos = response.find(',', pos);
  if (pos == std::string::npos || pos + 1 >= response.size()) {
    return false;
  }

  char stat = response[pos + 1];
  return stat == '2' || stat == '5';
}

void Nrf9151AtTask(void* arg) {
  printf("Nrf9151AtTask started\n");
  vTaskDelay(pdMS_TO_TICKS(1000));

  printf("configuring system mode...\n");
  std::string response = SendAtCommand("AT%XSYSTEMMODE=1,1,1,1");
  printf("response: %s\n", response.c_str());

  printf("setting CFUN=1...\n");
  response = SendAtCommand("AT+CFUN=1");
  printf("response: %s\n", response.c_str());
  vTaskDelay(pdMS_TO_TICKS(500));

  printf("checking sim card...\n");
  printf("sim card %s\n", CheckSimCard() ? "ready" : "not ready");

  printf("enabling CEREG URC...\n");
  response = SendAtCommand("AT+CEREG=2");
  printf("response: %s\n", response.c_str());

  printf("waiting for network registration...\n");
  bool registered = false;
  for (int i = 0; i < 24; i++) {
    if (IsNetworkRegistered()) {
      registered = true;
      printf("network registered successfully\n");
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
  if (!registered) {
    printf("network registration timeout\n");
  }

  printf("enabling GNSS...\n");
  response = SendAtCommand("AT#XGNSS=1,0,0,0");
  printf("response: %s\n", response.c_str());
  vTaskDelay(pdMS_TO_TICKS(1000));

  while (1) {
    response = SendAtCommand("AT+CCLK?");
    if (response.find("+CCLK:") != std::string::npos) {
      printf("current time: %s\n", response.c_str());
    } else {
      printf("time query failed: %s\n", response.c_str());
    }
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}

void ConfigLr1121(lilygo_device_driver::TDisplayP4AirDriver& driver) {
  if (!driver.status().lr1121.init_flag || driver.chip().lr1121 == nullptr) {
    printf("lr1121 init failed\n");
    return;
  }

  g_lr1121 = driver.chip().lr1121;
}

}  // namespace

extern "C" void app_main(void) {
  printf("Ciallo\n");

  auto& driver = lilygo_device_driver::TDisplayP4AirDriver::GetInstance();
  driver.Init();

  ConfigLr1121(driver);
  g_nrf9151_uart = driver.bus().nrf9151_uart_bus;

  Wire.set_bus_handle(driver.bus().xl9535_i2c_bus->bus_handle());
  St25r3916_Init();

  esp_err_t result = esp_hosted_init();
  if (result != ESP_OK) {
    printf("esp_hosted_init failed: %s\n", esp_err_to_name(result));
  }
  WifiInitSta();

  if (g_lr1121 != nullptr) {
    xTaskCreate(Lr1121RfTask, "Lr1121RfTask", 4 * 1024, NULL, 3, NULL);
  }
  xTaskCreate(St25r3916RfTask, "St25r3916RfTask", 4 * 1024, NULL, 3, NULL);
  xTaskCreate(Esp32c5RfTask, "Esp32c5RfTask", 4 * 1024, NULL, 3, NULL);
  xTaskCreate(Nrf9151AtTask, "Nrf9151AtTask", 8 * 1024, NULL, 3, NULL);
}
