/*
 * @Description: ESP-Hosted example helpers
 * @Author: LILYGO_L
 * @Date: 2026-05-24 16:30:00
 * @LastEditTime: 2026-05-24 16:30:00
 * @License: GPL 3.0
 */

#pragma once

#include "t_display_p4_air_example.h"

namespace t_display_p4_air_example {
namespace esp_hosted {

inline void ResetEsp32c5(Driver& driver) {
  auto& xl9535 = driver.chip().xl9535;
  xl9535->GpioWrite(
      gpio::xl9535::kEsp32c5En, cpp_bus_driver::Xl95x5::Value::kHigh);
  vTaskDelay(pdMS_TO_TICKS(100));
  xl9535->GpioWrite(
      gpio::xl9535::kEsp32c5En, cpp_bus_driver::Xl95x5::Value::kLow);
  vTaskDelay(pdMS_TO_TICKS(100));
  xl9535->GpioWrite(
      gpio::xl9535::kEsp32c5En, cpp_bus_driver::Xl95x5::Value::kHigh);
  vTaskDelay(pdMS_TO_TICKS(100));
}

}  // namespace esp_hosted
}  // namespace t_display_p4_air_example
