/*
 * @Description: es8389
 * @Author: LILYGO_L
 * @Date: 2026-03-03 13:51:54
 * @LastEditTime: 2026-06-18 09:17:52
 * @License: GPL 3.0
 */
#include "lilygo_device_driver_library.h"
#include "new_notification_010_c2_b16_s44100.h"

namespace {

constexpr size_t kAudioBufferSampleCount = 1024;

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

      printf("play_audio start\n");
      int ret = esp_codec_dev_write(output_codec_dev,
                                    const_cast<uint16_t*>(c2_b16_s44100),
                                    sizeof(c2_b16_s44100));
      if (ret != ESP_CODEC_DEV_OK) {
        printf("esp_codec_dev_write failed (error code: %#X)\n", ret);
      }
      printf("play_audio finish\n");
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
