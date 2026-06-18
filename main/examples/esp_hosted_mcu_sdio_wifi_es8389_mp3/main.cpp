/*
 * @Description: esp_hosted_mcu_sdio_wifi_es8389_mp3
 * @Author: LILYGO_L
 * @Date: 2026-02-26 10:39:24
 * @LastEditTime: 2026-06-18 15:32:08
 * @License: GPL 3.0
 */
#include "lilygo_device_driver_library.h"
#include "mp3_driver.h"

#include <algorithm>
#include <cstring>
#include <memory>

#include "esp_crt_bundle.h"
#include "esp_event.h"
#include "esp_hosted.h"
#include "esp_http_client.h"
#include "esp_wifi.h"

namespace {

constexpr char kWifiSsid[] = "LilyGo-AABB-5G";
constexpr char kWifiPassword[] = "xinyuandianzi";
constexpr char kDownloadUrl[] =
    "https://s9.imslp.org/files/imglnks/usimg/b/ba/"
    "IMSLP479619-PMLP02312-Nocturne,_Op.9_No.2_in_E-flat_major_Aya_"
    "Higuchi.mp3";

bool g_wifi_connected_flag = false;
esp_codec_dev_handle_t g_codec_dev = nullptr;

uint32_t GetSystemTimeMs() {
  return static_cast<uint32_t>(esp_timer_get_time() / 1000);
}

bool SkipHttpBytes(esp_http_client_handle_t client, uint32_t bytes_to_skip) {
  auto skip_buffer = std::make_unique<uint8_t[]>(4096);
  uint32_t remaining = bytes_to_skip;
  while (remaining > 0) {
    size_t chunk = std::min<size_t>(4096, remaining);
    int read_len =
        esp_http_client_read(client, (char*)skip_buffer.get(), chunk);
    if (read_len <= 0) {
      printf("skip failed, read_len=%d, remaining=%lu\n", read_len,
          static_cast<unsigned long>(remaining));
      return false;
    }
    remaining -= read_len;
  }
  return true;
}

size_t ReadPrefixAndSkipId3v2(
    esp_http_client_handle_t client, uint8_t* prefix_buffer) {
  uint8_t header[10] = {0};
  int read_len = esp_http_client_read(client, (char*)header, sizeof(header));
  if (read_len <= 0) {
    return 0;
  }

  if (read_len != sizeof(header) || memcmp(header, "ID3", 3) != 0) {
    memcpy(prefix_buffer, header, read_len);
    return read_len;
  }

  uint8_t major_version = header[3];
  uint8_t flags = header[5];
  uint32_t tag_size = private_library::mp3::SynchsafeToUint32(
      header + 6);
  printf("id3v2.%d tag detected, flags: 0x%02x, size: %lu bytes\n",
      major_version, flags, static_cast<unsigned long>(tag_size));

  if (SkipHttpBytes(client, tag_size)) {
    printf("skipped id3v2 bytes: %lu\n",
        static_cast<unsigned long>(tag_size + sizeof(header)));
  }
  return 0;
}

void HttpStreamPlayTask(void* pv_parameters) {
  printf("starting stream playback: %s\n", kDownloadUrl);

  esp_http_client_config_t config = {
      .url = kDownloadUrl,
      .timeout_ms = 15000,
      .crt_bundle_attach = esp_crt_bundle_attach,
  };
  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (esp_http_client_open(client, 0) != ESP_OK) {
    printf("esp_http_client_open failed\n");
    esp_http_client_cleanup(client);
    vTaskDelete(NULL);
    return;
  }

  int content_length = esp_http_client_fetch_headers(client);
  printf("stream start total size: %d bytes\n", content_length);

  esp_audio_dec_register_default();
  esp_audio_dec_cfg_t dec_cfg = {
      .type = ESP_AUDIO_TYPE_MP3,
  };
  esp_audio_dec_handle_t decoder = nullptr;
  if (esp_audio_dec_open(&dec_cfg, &decoder) != ESP_AUDIO_ERR_OK) {
    printf("esp_audio_dec_open failed\n");
    esp_audio_dec_unregister_default();
    esp_http_client_cleanup(client);
    vTaskDelete(NULL);
    return;
  }

  constexpr size_t kReadBufferSize = 4 * 1024;
  constexpr size_t kPcmBufferSize = 8 * 1024;
  auto read_buffer = std::make_unique<uint8_t[]>(kReadBufferSize);
  auto pcm_buffer = std::make_unique<uint8_t[]>(kPcmBufferSize);

  size_t remain_bytes = ReadPrefixAndSkipId3v2(client, read_buffer.get());
  int total_download = 0;
  uint32_t start_time = GetSystemTimeMs();
  uint32_t last_print_time = start_time;

  while (1) {
    int read_len = esp_http_client_read(client,
        reinterpret_cast<char*>(read_buffer.get() + remain_bytes),
        kReadBufferSize - remain_bytes);
    if (read_len < 0) {
      printf("http read error\n");
      break;
    }

    size_t total_in_buffer = remain_bytes + read_len;
    total_download += read_len;
    if (total_in_buffer == 0) {
      break;
    }

    esp_audio_dec_in_raw_t in_raw = {
        .buffer = read_buffer.get(),
        .len = total_in_buffer,
    };
    esp_audio_dec_out_frame_t out_frame = {
        .buffer = pcm_buffer.get(),
        .len = kPcmBufferSize,
    };

    while (in_raw.len > 0) {
      esp_audio_err_t result =
          esp_audio_dec_process(decoder, &in_raw, &out_frame);
      if (result != ESP_AUDIO_ERR_OK) {
        if (result == ESP_AUDIO_ERR_FAIL) {
          printf("esp_audio_dec_process failed\n");
        }
        break;
      }

      if (out_frame.decoded_size > 0) {
        esp_codec_dev_write(
            g_codec_dev, out_frame.buffer, out_frame.decoded_size);
      }

      if (in_raw.consumed == 0) {
        break;
      }
      in_raw.buffer += in_raw.consumed;
      in_raw.len -= in_raw.consumed;
    }

    remain_bytes = in_raw.len;
    if (remain_bytes > 0) {
      memmove(read_buffer.get(), in_raw.buffer, remain_bytes);
    }

    uint32_t now = GetSystemTimeMs();
    if (now - last_print_time >= 1000) {
      float duration = (now - start_time) / 1000.0f;
      float speed = (total_download / 1024.0f) / duration;
      float progress =
          content_length > 0 ? (total_download * 100.0f / content_length) : 0;
      printf("[music] progress: %.2f%% | speed: %.2f kb/s | total size: %.1f kb\n",
          progress, speed, total_download / 1024.0f);
      last_print_time = now;
    }

    if (read_len == 0 && remain_bytes == total_in_buffer) {
      break;
    }
  }

  printf("playback finished. cleaning up...\n");
  esp_audio_dec_close(decoder);
  esp_audio_dec_unregister_default();
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

  wifi_config_t wifi_config = {};
  std::strncpy(reinterpret_cast<char*>(wifi_config.sta.ssid), kWifiSsid,
      sizeof(wifi_config.sta.ssid) - 1);
  std::strncpy(reinterpret_cast<char*>(wifi_config.sta.password),
      kWifiPassword, sizeof(wifi_config.sta.password) - 1);
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());
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

  g_codec_dev = driver.es8389_output_codec_dev();
  if ((g_codec_dev == nullptr) || (!driver.status().es8389.init_flag)) {
    printf("Es8389 init failed\n");
    return;
  }
  xTaskCreate(HttpStreamPlayTask, "stream_task", 10 * 1024, NULL, 5, NULL);

  while (1) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
