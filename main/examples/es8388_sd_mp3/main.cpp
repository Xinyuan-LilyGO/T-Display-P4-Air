/*
 * @Description: es8388_sd_mp3
 * @Author: LILYGO_L
 * @Date: 2026-02-25 09:09:45
 * @LastEditTime: 2026-05-24 16:30:00
 * @License: GPL 3.0
 */
#include "mp3_driver.h"

namespace {

constexpr const char* kMp3FilePath = "/sdcard/music.mp3";

bool InitBootKey() {
  static cpp_bus_driver::Tool tool;
  return tool.SetGpioMode(
      lilygo_device_driver::t_display_p4_air::gpio::button::kEsp32p4Boot,
      cpp_bus_driver::Tool::GpioMode::kInput,
      cpp_bus_driver::Tool::GpioStatus::kPullup);
}

bool BootKeyPressed() {
  static cpp_bus_driver::Tool tool;
  return !tool.GpioRead(
      lilygo_device_driver::t_display_p4_air::gpio::button::kEsp32p4Boot);
}

}  // namespace

extern "C" void app_main(void) {
  printf("Ciallo\n");

  auto& driver = lilygo_device_driver::TDisplayP4AirDriver::GetInstance();
  driver.Init();
  InitBootKey();

  if (!driver.InitSdmmc(
          lilygo_device_driver::t_display_p4_air::device::sd::kBasePath, SDMMC_FREQ_52M)) {
    printf("InitSdmmc failed\n");
  }

  uint32_t audio_sample_rate =
      lilygo_device_driver::t_display_p4_air::device::es8388::kSampleRate;
  esp_codec_dev_handle_t codec_dev =
      private_library::es8388::InitCodec(
          driver, audio_sample_rate, 60, 20);

  while (1) {
    if (BootKeyPressed()) {
      vTaskDelay(pdMS_TO_TICKS(300));

      printf("play_mp3 start\n");
      private_library::mp3::PlayFile(
          kMp3FilePath, codec_dev, &audio_sample_rate);
      printf("play_mp3 finish\n");
    }

    auto buffer = std::make_unique<int16_t[]>(1024);
    esp_codec_dev_read(codec_dev, buffer.get(), 1024 * sizeof(int16_t));
    esp_codec_dev_write(codec_dev, buffer.get(), 1024 * sizeof(int16_t));

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
