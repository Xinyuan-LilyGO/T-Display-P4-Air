/*
 * @Description: es8389_sd_mp3
 * @Author: LILYGO_L
 * @Date: 2026-02-25 09:09:45
 * @LastEditTime: 2026-05-24 16:30:00
 * @License: GPL 3.0
 */
#include "lilygo_device_driver_library.h"
#include "mp3_driver.h"

namespace {

namespace device = lilygo_device_driver::t_display_p4_air::device;

constexpr const char* kMp3FilePath = "/sdcard/music.mp3";
constexpr int kAudioBufferDurationMs = 20;
constexpr size_t kAudioBufferFrameCount =
    device::es8389::kSampleRate * kAudioBufferDurationMs / 1000;
constexpr size_t kAudioBufferSampleCount =
    kAudioBufferFrameCount * device::es8389::kChannel;

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

  if (!driver.InitSdmmc(device::sd::kBasePath, SDMMC_FREQ_52M)) {
    printf("InitSdmmc failed\n");
  }

  uint32_t audio_sample_rate = device::es8389::kSampleRate;
  esp_codec_dev_handle_t input_codec_dev = driver.es8389_input_codec_dev();
  esp_codec_dev_handle_t output_codec_dev = driver.es8389_output_codec_dev();
  if ((input_codec_dev == nullptr) || (output_codec_dev == nullptr) ||
      (!driver.status().es8389.init_flag)) {
    printf("Es8389 init failed\n");
    return;
  }

  auto buffer = std::make_unique<int16_t[]>(kAudioBufferSampleCount);

  while (1) {
    if (BootKeyPressed()) {
      vTaskDelay(pdMS_TO_TICKS(300));

      printf("play_mp3 start\n");
      private_library::mp3::PlayFile(
          kMp3FilePath, output_codec_dev, &audio_sample_rate);
      printf("play_mp3 finish\n");
    }

    int ret = esp_codec_dev_read(input_codec_dev, buffer.get(),
        kAudioBufferSampleCount * sizeof(int16_t));
    if (ret != ESP_CODEC_DEV_OK) {
      printf("esp_codec_dev_read failed (error code: %#X)\n", ret);
      continue;
    }

    ret = esp_codec_dev_write(output_codec_dev, buffer.get(),
        kAudioBufferSampleCount * sizeof(int16_t));
    if (ret != ESP_CODEC_DEV_OK) {
      printf("esp_codec_dev_write failed (error code: %#X)\n", ret);
    }
  }
}
