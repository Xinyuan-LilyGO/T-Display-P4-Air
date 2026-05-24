/*
 * @Description: es8388
 * @Author: LILYGO_L
 * @Date: 2026-03-03 13:51:54
 * @LastEditTime: 2026-05-24 16:30:00
 * @License: GPL 3.0
 */
#include "../common/es8388_example.h"

#include "New Notification 010_c2_b16_s44100.h"

extern "C" void app_main(void) {
  printf("Ciallo\n");

  auto& driver = t_display_p4_air_example::InitDriver();
  t_display_p4_air_example::es8388::InitBootKey();

  esp_codec_dev_handle_t codec_dev =
      t_display_p4_air_example::es8388::InitCodec(
          driver, t_display_p4_air_example::device::es8388::kSampleRate, 100,
          20);

  while (1) {
    if (t_display_p4_air_example::es8388::BootKeyPressed()) {
      vTaskDelay(pdMS_TO_TICKS(300));

      printf("play_audio start\n");
      esp_codec_dev_write(codec_dev, (uint8_t*)c2_b16_s44100,
          sizeof(c2_b16_s44100));
      printf("play_audio finish\n");
    }

    auto buffer = std::make_unique<int16_t[]>(1024);
    esp_codec_dev_read(codec_dev, buffer.get(), 1024 * sizeof(int16_t));
    esp_codec_dev_write(codec_dev, buffer.get(), 1024 * sizeof(int16_t));

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
