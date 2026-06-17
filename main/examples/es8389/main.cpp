/*
 * @Description: es8389
 * @Author: LILYGO_L
 * @Date: 2026-03-03 13:51:54
 * @LastEditTime: 2026-06-17 10:55:00
 * @License: GPL 3.0
 */
#include "es8389_driver.h"

#include "new_notification_010_c2_b16_s44100.h"

namespace {

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

  esp_codec_dev_handle_t codec_dev = private_library::es8389::InitCodec(
      driver,
      lilygo_device_driver::t_display_p4_air::device::es8389::kSampleRate, 100,
      20);

  while (1) {
    if (BootKeyPressed()) {
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
