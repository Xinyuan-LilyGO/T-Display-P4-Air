/*
 * @Description: aw86224
 * @Author: LILYGO_L
 * @Date: 2026-02-04 17:12:29
 * @LastEditTime: 2026-06-15 15:41:48
 * @License: GPL 3.0
 */
#include "lilygo_device_driver_library.h"

namespace {

using Driver = lilygo_device_driver::TDisplayP4AirDriver;

static constexpr uint8_t kGainLevels[] = {
    16, 32, 48, 64, 80, 96, 112, 128,
    144, 160, 176, 192, 208, 224, 240, 255};
static constexpr uint8_t kLoopCount = 1;
static constexpr uint32_t kPlayMs = 220;
static constexpr uint32_t kStopMs = 180;
static constexpr uint32_t kRetryDelayMs = 1000;
static constexpr uint32_t kCycleDelayMs = 1500;

}  // namespace

extern "C" void app_main(void) {
  printf("Ciallo\n");

  auto& driver = Driver::GetInstance();
  driver.Init(Driver::InitMode::kSync);

  auto& aw86224 = driver.chip().aw86224;
  if (aw86224 == nullptr || !driver.status().aw86224.init_flag) {
    printf("Aw86224 init failed\n");
    return;
  }

  while (1) {
    const auto& info = driver.status().aw86224.ram_waveform_info;
    if (info.data == nullptr || info.waveform_count == 0) {
      printf("Aw86224 RAM waveform init failed\n");
      vTaskDelay(pdMS_TO_TICKS(kRetryDelayMs));
      continue;
    }

    printf(
        "Aw86224 selected library: %s, sequences: %u, rated f0: %uHz\n",
        info.name, static_cast<unsigned int>(info.waveform_count),
        static_cast<unsigned int>(info.rated_f0_hz));
    printf("Aw86224 gain test levels:");
    for (uint8_t gain : kGainLevels) {
      printf(" %u", static_cast<unsigned int>(gain));
    }
    printf("\n");

    printf("Aw86224 GetInputVoltage: %.06f v\n", aw86224->GetInputVoltage());

    for (uint8_t gain : kGainLevels) {
      printf("Aw86224 gain level: %u\n", static_cast<unsigned int>(gain));

      for (uint8_t sequence = 1; sequence <= info.waveform_count; sequence++) {
        printf("Play %s sequence %u gain %u\n", info.name,
            static_cast<unsigned int>(sequence),
            static_cast<unsigned int>(gain));
        aw86224->PlayRamWaveform(sequence, kLoopCount, gain);
        vTaskDelay(pdMS_TO_TICKS(kPlayMs));
        aw86224->StopRamPlaybackWaveform();
        vTaskDelay(pdMS_TO_TICKS(kStopMs));
      }
    }

    vTaskDelay(pdMS_TO_TICKS(kCycleDelayMs));
  }
}
