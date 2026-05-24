/*
 * @Description: es8388_sd_mp3
 * @Author: LILYGO_L
 * @Date: 2026-02-25 09:09:45
 * @LastEditTime: 2026-05-24 16:30:00
 * @License: GPL 3.0
 */
#include "../common/mp3_example.h"

namespace {

constexpr const char* kMp3FilePath = "/sdcard/music.mp3";

}  // namespace

extern "C" void app_main(void) {
  printf("Ciallo\n");

  auto& driver = t_display_p4_air_example::InitDriver();
  t_display_p4_air_example::es8388::InitBootKey();

  if (!driver.InitSdmmc(
          t_display_p4_air_example::device::sd::kBasePath, SDMMC_FREQ_52M)) {
    printf("InitSdmmc failed\n");
  }

  uint32_t audio_sample_rate =
      t_display_p4_air_example::device::es8388::kSampleRate;
  esp_codec_dev_handle_t codec_dev =
      t_display_p4_air_example::es8388::InitCodec(
          driver, audio_sample_rate, 60, 20);

  while (1) {
    if (t_display_p4_air_example::es8388::BootKeyPressed()) {
      vTaskDelay(pdMS_TO_TICKS(300));

      printf("play_mp3 start\n");
      t_display_p4_air_example::mp3::PlayFile(
          kMp3FilePath, codec_dev, &audio_sample_rate);
      printf("play_mp3 finish\n");
    }

    auto buffer = std::make_unique<int16_t[]>(1024);
    esp_codec_dev_read(codec_dev, buffer.get(), 1024 * sizeof(int16_t));
    esp_codec_dev_write(codec_dev, buffer.get(), 1024 * sizeof(int16_t));

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
