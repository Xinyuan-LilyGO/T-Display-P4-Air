/*
 * @Description: T-Display-P4-Air example helpers
 * @Author: LILYGO_L
 * @Date: 2026-05-24 16:30:00
 * @LastEditTime: 2026-05-24 16:30:00
 * @License: GPL 3.0
 */

#pragma once

#include "lilygo_device_driver_library.h"

namespace t_display_p4_air_example {

using Driver = lilygo_device_driver::TDisplayP4AirDriver;
namespace board = lilygo_device_driver::t_display_p4_air;
namespace gpio = board::gpio;
namespace device = board::device;

inline Driver& InitDriver(Driver::InitMode mode = Driver::InitMode::kSync) {
  auto& driver = Driver::GetInstance();
  driver.Init(mode);
  return driver;
}

inline constexpr int kScreenWidth = device::hi8561::kScreenWidth;
inline constexpr int kScreenHeight = device::hi8561::kScreenHeight;
inline constexpr int kScreenBitsPerPixel = device::screen::kBitsPerPixel;
inline constexpr int kCameraBufferCount = device::camera::kBufferCount;
inline constexpr int kScreenRotationDirection =
    device::screen::kRotationDirection;

}  // namespace t_display_p4_air_example
