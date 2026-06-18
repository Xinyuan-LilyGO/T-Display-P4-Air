/*
 * @Description: esp_hosted_mcu_sdio_wifi
 * @Author: LILYGO_L
 * @Date: 2025-09-30 14:21:42
 * @LastEditTime: 2026-06-18 15:11:36
 * @License: GPL 3.0
 */
#include "lilygo_device_driver_library.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <unordered_set>
#include <vector>

#include "esp_event.h"
#include "esp_hosted.h"
#include "esp_wifi.h"

namespace {

constexpr char kWifiSsid[] = "LilyGo-AABB-5G";
constexpr char kWifiPassword[] = "xinyuandianzi";

bool g_wifi_scan_success_flag = false;
bool g_wifi_connected_flag = false;

void WifiEventHandler(
    void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    printf("wifi sta start\n");
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t* event = static_cast<ip_event_got_ip_t*>(event_data);
    printf("got ip: " IPSTR "\n", IP2STR(&event->ip_info.ip));
    g_wifi_connected_flag = true;
  } else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_DISCONNECTED) {
    printf("wifi disconnected, retrying...\n");
    g_wifi_connected_flag = false;
    esp_wifi_connect();
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

  wifi_config_t wifi_config = {};
  std::strncpy(reinterpret_cast<char*>(wifi_config.sta.ssid), kWifiSsid,
      sizeof(wifi_config.sta.ssid) - 1);
  std::strncpy(reinterpret_cast<char*>(wifi_config.sta.password),
      kWifiPassword, sizeof(wifi_config.sta.password) - 1);
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_connect());
}

}  // namespace

extern "C" void app_main(void) {
  printf("Ciallo\n");

  auto& driver = lilygo_device_driver::TDisplayP4AirDriver::GetInstance();
  driver.Init();

  // driver.SetUartTarget(
  //     lilygo_device_driver::TDisplayP4AirDriver::UartTarget::kEsp32c5);
  // driver.EnterEsp32c5DownloadMode();
  // while (1) {
  //   vTaskDelay(pdMS_TO_TICKS(1000));
  // }

  esp_err_t result = esp_hosted_init();
  if (result != ESP_OK) {
    printf("esp_hosted_init failed: %s\n", esp_err_to_name(result));
  }

  InitWifiSta();
  while (!g_wifi_connected_flag) {
    vTaskDelay(pdMS_TO_TICKS(500));
  }

  while (1) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
