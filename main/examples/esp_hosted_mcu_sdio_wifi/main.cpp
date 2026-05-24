/*
 * @Description: esp_hosted_mcu_sdio_wifi
 * @Author: LILYGO_L
 * @Date: 2025-09-30 14:21:42
 * @LastEditTime: 2026-05-24 16:30:00
 * @License: GPL 3.0
 */
#include "../common/t_display_p4_air_example.h"

#include <algorithm>
#include <string>
#include <unordered_set>
#include <vector>

#include "esp_hosted.h"
#include "esp_wifi_remote.h"

extern "C" {
#include "esp_hosted_transport_config.h"
}

#define WIFI_SSID "xinyuandianzi_AX_Wi-Fi5"
#define WIFI_PASSWORD "AA15994823428"

namespace {

typedef enum {
  TRANSPORT_CONFIG_STATUS_UNINITIALIZED = 0,
  TRANSPORT_CONFIG_STATUS_CONFIGURED,
  TRANSPORT_CONFIG_STATUS_ACTIVE,
  TRANSPORT_CONFIG_STATUS_ERROR
} transport_config_status_t;

typedef struct {
  transport_config_status_t status;
  esp_hosted_transport_err_t last_error;
} transport_config_context_t;

transport_config_context_t g_transport_context = {
    .status = TRANSPORT_CONFIG_STATUS_UNINITIALIZED,
    .last_error = ESP_TRANSPORT_OK,
};

bool g_wifi_scan_success_flag = false;

void ResetEsp32c5(t_display_p4_air_example::Driver& driver) {
  auto& xl9535 = driver.chip().xl9535;
  xl9535->GpioWrite(t_display_p4_air_example::gpio::xl9535::kEsp32c5En,
      cpp_bus_driver::Xl95x5::Value::kHigh);
  vTaskDelay(pdMS_TO_TICKS(100));
  xl9535->GpioWrite(t_display_p4_air_example::gpio::xl9535::kEsp32c5En,
      cpp_bus_driver::Xl95x5::Value::kLow);
  vTaskDelay(pdMS_TO_TICKS(100));
  xl9535->GpioWrite(t_display_p4_air_example::gpio::xl9535::kEsp32c5En,
      cpp_bus_driver::Xl95x5::Value::kHigh);
  vTaskDelay(pdMS_TO_TICKS(100));
}

esp_err_t ValidateTransportConfiguration() {
  printf("validating transport configuration...\n");

  struct esp_hosted_transport_config* config = nullptr;
  esp_hosted_transport_err_t result = esp_hosted_transport_get_config(&config);
  if (result != ESP_TRANSPORT_OK || config == nullptr) {
    printf("configuration validation failed: cannot retrieve config\n");
    return ESP_FAIL;
  }

  if (config->transport_in_use < 1 || config->transport_in_use > 4) {
    printf("invalid transport type: %d\n", config->transport_in_use);
    return ESP_FAIL;
  }

  printf("transport configuration validation passed\n");
  return ESP_OK;
}

void DisplayTransportConfiguration() {
  printf("retrieving current transport configuration...\n");

  struct esp_hosted_transport_config* config = nullptr;
  esp_hosted_transport_err_t result = esp_hosted_transport_get_config(&config);
  if (result != ESP_TRANSPORT_OK || config == nullptr) {
    printf("failed to retrieve transport configuration: error %d\n", result);
    return;
  }

  printf("current transport configuration:\n");
  printf("  transport type: %d\n", config->transport_in_use);

  switch (config->transport_in_use) {
    case 1:
      printf("  sdio configuration:\n");
      printf("    clock frequency: %lu khz\n", config->u.sdio.clock_freq_khz);
      printf("    bus width: %d bits\n", config->u.sdio.bus_width);
      printf("    queue sizes - tx: %d, rx: %d\n",
          config->u.sdio.tx_queue_size, config->u.sdio.rx_queue_size);
      break;
    case 2:
      printf("  spi half-duplex configuration:\n");
      printf("    clock frequency: %lu mhz\n", config->u.spi_hd.clk_mhz);
      printf("    data lines: %d\n", config->u.spi_hd.num_data_lines);
      printf("    checksum: %s\n",
          config->u.spi_hd.checksum_enable ? "enabled" : "disabled");
      break;
    case 3:
      printf("  spi full-duplex configuration:\n");
      printf("    clock frequency: %lu mhz\n", config->u.spi.clk_mhz);
      printf("    spi mode: %d\n", config->u.spi.mode);
      printf("    queue sizes - tx: %d, rx: %d\n", config->u.spi.tx_queue_size,
          config->u.spi.rx_queue_size);
      break;
    case 4:
      printf("  uart configuration:\n");
      printf("    baud rate: %lu bps\n", config->u.uart.baud_rate);
      printf("    uart port: %d\n", config->u.uart.port);
      printf("    checksum: %s\n",
          config->u.uart.checksum_enable ? "enabled" : "disabled");
      break;
    default:
      printf("  unknown transport type: %d\n", config->transport_in_use);
      break;
  }
}

esp_err_t InitializeEspHostedSimple() {
  printf("initializing esp-hosted...\n");

  esp_err_t result = esp_hosted_init();
  if (result == ESP_OK) {
    printf("esp-hosted initialized successfully\n");
    g_transport_context.status = TRANSPORT_CONFIG_STATUS_ACTIVE;
  } else {
    printf("esp-hosted initialization failed: %s\n", esp_err_to_name(result));
    g_transport_context.status = TRANSPORT_CONFIG_STATUS_ERROR;
    printf("check transport configuration and slave device connection\n");
  }

  return result;
}

void WifiEventHandler(
    void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    printf("wifi sta start\n");
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t* event = static_cast<ip_event_got_ip_t*>(event_data);
    printf("got ip: " IPSTR "\n", IP2STR(&event->ip_info.ip));
    printf("wifi connect success\n");
  } else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_DISCONNECTED) {
    printf("wifi connect fail\n");
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE) {
    uint16_t ap_count = 0;
    std::vector<wifi_ap_record_t> ap_info;

    if (esp_wifi_scan_get_ap_num(&ap_count) != ESP_OK) {
      printf("failed to get ap count\n");
      return;
    }
    if (ap_count > 64) {
      ap_count = 64;
    }
    ap_info.resize(ap_count);

    if (esp_wifi_scan_get_ap_records(&ap_count, ap_info.data()) != ESP_OK) {
      printf("failed to get ap records\n");
      return;
    }
    esp_wifi_clear_ap_list();

    std::sort(ap_info.begin(), ap_info.end(),
        [](const wifi_ap_record_t& a, const wifi_ap_record_t& b) {
          return a.rssi > b.rssi;
        });

    std::unordered_set<std::string> seen_ssids;
    std::vector<wifi_ap_record_t> unique_ap_info;
    for (const auto& info : ap_info) {
      std::string ssid_str(reinterpret_cast<const char*>(info.ssid));
      if (!ssid_str.empty() && seen_ssids.find(ssid_str) == seen_ssids.end()) {
        seen_ssids.emplace(ssid_str);
        unique_ap_info.emplace_back(info);
      }
    }

    printf("-------------------------------------------------------------\n");
    printf("| %-32s | %4s | %7s | %17s |\n", "ssid", "rssi", "channel",
        "mac address");
    printf("-------------------------------------------------------------\n");
    for (const auto& info : unique_ap_info) {
      const char* band = (info.primary <= 14) ? "2.4ghz" : "5ghz";
      printf("| %-32s | %4d dbm | %3d (%s) | %02x:%02x:%02x:%02x:%02x:%02x |\n",
          info.ssid, info.rssi, info.primary, band, info.bssid[0],
          info.bssid[1], info.bssid[2], info.bssid[3], info.bssid[4],
          info.bssid[5]);
    }
    printf("-------------------------------------------------------------\n");

    g_wifi_scan_success_flag = true;
  } else {
    printf("event %s %ld\n", event_base, event_id);
  }
}

void InitWifiSta() {
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_t* sta_netif = esp_netif_create_default_wifi_sta();
  assert(sta_netif);

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      WIFI_EVENT, ESP_EVENT_ANY_ID, &WifiEventHandler, NULL, NULL));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      IP_EVENT, IP_EVENT_STA_GOT_IP, &WifiEventHandler, NULL, NULL));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_start());

  while (1) {
    esp_wifi_scan_start(NULL, true);
    vTaskDelay(pdMS_TO_TICKS(1500));
    if (g_wifi_scan_success_flag) {
      g_wifi_scan_success_flag = false;
      printf("wifi scan success\n");
      break;
    }
  }

  wifi_config_t wifi_config = {
      .sta =
          {
              .ssid = WIFI_SSID,
              .password = WIFI_PASSWORD,
          },
  };
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  esp_wifi_connect();
}

}  // namespace

extern "C" void app_main(void) {
  printf("Ciallo\n");

  auto& driver = t_display_p4_air_example::InitDriver();
  ResetEsp32c5(driver);

  printf("esp-hosted transport configuration example\n");
  printf("========================================\n");

  if (ValidateTransportConfiguration() != ESP_OK) {
    printf("transport configuration validation failed\n");
    return;
  }

  DisplayTransportConfiguration();
  printf("========================================\n");

  if (InitializeEspHostedSimple() == ESP_OK) {
    printf("wifi and bluetooth functionality is now available\n");
    DisplayTransportConfiguration();
    printf("========================================\n");
  } else {
    printf("esp-hosted initialization failed\n");
  }

  printf("application running - transport status: %s\n",
      g_transport_context.status == TRANSPORT_CONFIG_STATUS_ACTIVE ? "active"
                                                                   : "error");

  InitWifiSta();

  while (1) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
