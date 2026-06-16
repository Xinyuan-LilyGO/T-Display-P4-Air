/*
 * @Description: esp_hosted_mcu_sdio_wifi_http_download_file
 * @Author: LILYGO_L
 * @Date: 2026-02-09 16:54:46
 * @LastEditTime: 2026-05-24 16:30:00
 * @License: GPL 3.0
 */
#include "lilygo_device_driver_library.h"

#include <memory>

#include "esp_crt_bundle.h"
#include "esp_event.h"
#include "esp_hosted.h"
#include "esp_http_client.h"
#include "esp_wifi.h"

#define WIFI_SSID "xinyuandianzi_AX_Wi-Fi5"
#define WIFI_PASSWORD "AA15994823428"
#define DOWNLOAD_URL "https://dldir1.qq.com/qqfile/qq/PCQQ9.7.17/QQ9.7.17.29225.exe"

namespace {

bool g_wifi_connected_flag = false;

void ResetEsp32c5(lilygo_device_driver::TDisplayP4AirDriver& driver) {
  auto& xl9535 = driver.chip().xl9535;
  xl9535->GpioWrite(
      lilygo_device_driver::t_display_p4_air::gpio::xl9535::kEsp32c5En,
      1);
  vTaskDelay(pdMS_TO_TICKS(100));
  xl9535->GpioWrite(
      lilygo_device_driver::t_display_p4_air::gpio::xl9535::kEsp32c5En,
      0);
  vTaskDelay(pdMS_TO_TICKS(100));
  xl9535->GpioWrite(
      lilygo_device_driver::t_display_p4_air::gpio::xl9535::kEsp32c5En,
      1);
  vTaskDelay(pdMS_TO_TICKS(100));
}

uint32_t GetSystemTimeMs() {
  return static_cast<uint32_t>(esp_timer_get_time() / 1000);
}

void DownloadTask(void* pv_parameters) {
  printf("starting download from: %s\n", DOWNLOAD_URL);

  esp_http_client_config_t config = {
      .url = DOWNLOAD_URL,
      .timeout_ms = 10000,
      .crt_bundle_attach = esp_crt_bundle_attach,
  };
  esp_http_client_handle_t client = esp_http_client_init(&config);

  esp_err_t result = esp_http_client_open(client, 0);
  if (result != ESP_OK) {
    printf("failed to open http connection: %s\n", esp_err_to_name(result));
    esp_http_client_cleanup(client);
    vTaskDelete(NULL);
    return;
  }

  int content_length = esp_http_client_fetch_headers(client);
  int total_read_len = 0;
  auto buffer = std::make_unique<char[]>(4096);

  uint32_t start_time = GetSystemTimeMs();
  uint32_t last_calc_time = start_time;
  int last_read_len = 0;

  printf("download started. total size: %d bytes\n", content_length);
  printf("--------------------------------------------------\n");

  while (total_read_len < content_length || content_length == -1) {
    int read_len = esp_http_client_read(client, buffer.get(), 4096);
    if (read_len <= 0) {
      printf("download finished or connection closed\n");
      break;
    }

    total_read_len += read_len;
    uint32_t current_time = GetSystemTimeMs();
    if (current_time - last_calc_time >= 1000) {
      float duration = (current_time - start_time) / 1000.0f;
      float speed_instant =
          (total_read_len - last_read_len) / 1024.0f /
          ((current_time - last_calc_time) / 1000.0f);
      float speed_avg = (total_read_len / 1024.0f) / duration;

      printf("progress: %.2f kb | speed: %.2f kb/s (avg: %.2f kb/s)\n",
          total_read_len / 1024.0f, speed_instant, speed_avg);

      last_calc_time = current_time;
      last_read_len = total_read_len;
    }
  }

  uint32_t end_time = GetSystemTimeMs();
  float total_duration = (end_time - start_time) / 1000.0f;
  printf("--------------------------------------------------\n");
  printf("download finished! total: %d bytes in %.2f seconds\n", total_read_len,
      total_duration);
  if (total_duration > 0.0f) {
    printf("final average speed: %.2f kb/s\n",
        (total_read_len / 1024.0f) / total_duration);
  }

  esp_http_client_cleanup(client);
  vTaskDelete(NULL);
}

void WifiEventHandler(
    void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t* event = static_cast<ip_event_got_ip_t*>(event_data);
    printf("got ip: " IPSTR "\n", IP2STR(&event->ip_info.ip));
    g_wifi_connected_flag = true;
  } else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_DISCONNECTED) {
    printf("wifi disconnected, retrying...\n");
    g_wifi_connected_flag = false;
    esp_wifi_connect();
  }
}

void InitWifiSta() {
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      WIFI_EVENT, ESP_EVENT_ANY_ID, &WifiEventHandler, NULL, NULL));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      IP_EVENT, IP_EVENT_STA_GOT_IP, &WifiEventHandler, NULL, NULL));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

  wifi_config_t wifi_config = {
      .sta =
          {
              .ssid = WIFI_SSID,
              .password = WIFI_PASSWORD,
          },
  };
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());
}

}  // namespace

extern "C" void app_main(void) {
  printf("Ciallo\n");

  auto& driver = lilygo_device_driver::TDisplayP4AirDriver::GetInstance();
  driver.Init();
  ResetEsp32c5(driver);

  esp_err_t result = esp_hosted_init();
  if (result != ESP_OK) {
    printf("esp_hosted_init failed: %s\n", esp_err_to_name(result));
  }

  InitWifiSta();
  while (!g_wifi_connected_flag) {
    vTaskDelay(pdMS_TO_TICKS(500));
  }

  xTaskCreate(DownloadTask, "download_task", 8192, NULL, 5, NULL);

  while (1) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
