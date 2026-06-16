/*
 * @Description: qmc6310n
 * @Author: LILYGO_L
 * @Date: 2026-02-27 09:48:55
 * @LastEditTime: 2026-06-16
 * @License: GPL 3.0
 */
#include "lilygo_device_driver_library.h"

#include <algorithm>
#include <cstdint>
#include <memory>

#include "SensorQMC6310.hpp"

constexpr int kQmc6310nCalibrationTimeMs = 20000;

float g_mag_data[3] = {0.0f, 0.0f, 0.0f};
float g_heading_degree = 0.0f;
bool g_mag_data_ready_flag = false;
int64_t g_cycle_time_ms = 0;

auto g_qmc6310n = std::make_unique<SensorQMC6310>();

float NormalizeDegree360(float degree) {
  while (degree < 0.0f) {
    degree += 360.0f;
  }
  while (degree >= 360.0f) {
    degree -= 360.0f;
  }
  return degree;
}

const char* GetCompassDirection(float degree) {
  static const char* kCompassDirections[] = {
      "N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
      "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"};
  const int index = static_cast<int>((NormalizeDegree360(degree) + 11.25f) /
                                    22.5f) %
                    16;
  return kCompassDirections[index];
}

bool InitQmc6310n(void) {
  printf("InitQmc6310n\n");

  auto& driver = lilygo_device_driver::TDisplayP4AirDriver::GetInstance();
  i2c_master_bus_handle_t i2c_handle = driver.bus().xl9535_i2c_bus->bus_handle();

  if (!g_qmc6310n->begin(i2c_handle,
          lilygo_device_driver::t_display_p4_air::device::qmc6310n::kI2cAddress)) {
    printf("Qmc6310n begin failed\n");
    return false;
  }

  printf("Qmc6310n device id: %#X\n", g_qmc6310n->getChipID());
  g_qmc6310n->setOffset(0, 0, 0);

  if (!g_qmc6310n->configMagnetometer(
          OperationMode::CONTINUOUS_MEASUREMENT, MagFullScaleRange::FS_8G,
          200.0f, MagOverSampleRatio::OSR_1, MagDownSampleRatio::DSR_1)) {
    printf("Qmc6310n config magnetometer failed\n");
    return false;
  }

  printf("InitQmc6310n success\n");
  return true;
}

bool CalibrateQmc6310n(void) {
  printf("Qmc6310n calibration start, rotate the board slowly on all axes for "
         "%d seconds\n",
      kQmc6310nCalibrationTimeMs / 1000);

  RawVector raw_min = {INT16_MAX, INT16_MAX, INT16_MAX};
  RawVector raw_max = {INT16_MIN, INT16_MIN, INT16_MIN};
  int sample_count = 0;
  int64_t next_log_time_ms = 0;
  const int64_t calibration_end_time_ms =
      esp_timer_get_time() / 1000 + kQmc6310nCalibrationTimeMs;

  while ((esp_timer_get_time() / 1000) < calibration_end_time_ms) {
    if (g_qmc6310n->isDataReady()) {
      MagnetometerData mag_data;
      if (g_qmc6310n->readData(mag_data)) {
        raw_min.x = std::min(raw_min.x, mag_data.raw.x);
        raw_min.y = std::min(raw_min.y, mag_data.raw.y);
        raw_min.z = std::min(raw_min.z, mag_data.raw.z);
        raw_max.x = std::max(raw_max.x, mag_data.raw.x);
        raw_max.y = std::max(raw_max.y, mag_data.raw.y);
        raw_max.z = std::max(raw_max.z, mag_data.raw.z);
        ++sample_count;
      }
    }

    const int64_t now_ms = esp_timer_get_time() / 1000;
    if (now_ms >= next_log_time_ms) {
      printf("Qmc6310n calibrating... %lld s left, samples=%d\n",
          (calibration_end_time_ms - now_ms + 999) / 1000, sample_count);
      next_log_time_ms = now_ms + 1000;
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }

  if (sample_count <= 0) {
    printf("Qmc6310n calibration failed: no samples\n");
    return false;
  }

  const int16_t offset_x =
      static_cast<int16_t>((static_cast<int32_t>(raw_max.x) + raw_min.x) / 2);
  const int16_t offset_y =
      static_cast<int16_t>((static_cast<int32_t>(raw_max.y) + raw_min.y) / 2);
  const int16_t offset_z =
      static_cast<int16_t>((static_cast<int32_t>(raw_max.z) + raw_min.z) / 2);
  g_qmc6310n->setOffset(offset_x, offset_y, offset_z);

  printf("Qmc6310n calibration done: offset X=%d Y=%d Z=%d, raw min "
         "X=%d Y=%d Z=%d, raw max X=%d Y=%d Z=%d\n",
      offset_x, offset_y, offset_z, raw_min.x, raw_min.y, raw_min.z,
      raw_max.x, raw_max.y, raw_max.z);

  return true;
}

extern "C" void app_main(void) {
  printf("Ciallo\n");

  auto& driver = lilygo_device_driver::TDisplayP4AirDriver::GetInstance();
  driver.Init();
  bool qmc6310n_ready = InitQmc6310n();
  if (qmc6310n_ready) {
    qmc6310n_ready = CalibrateQmc6310n();
  }

  while (1) {
    if (qmc6310n_ready && g_qmc6310n->isDataReady()) {
      MagnetometerData mag_data;
      if (g_qmc6310n->readData(mag_data)) {
        g_mag_data[0] = mag_data.magnetic_field.x;
        g_mag_data[1] = mag_data.magnetic_field.y;
        g_mag_data[2] = mag_data.magnetic_field.z;
        g_heading_degree = NormalizeDegree360(mag_data.heading_degrees);
        g_mag_data_ready_flag = true;
      }
    }

    const int64_t now_ms = esp_timer_get_time() / 1000;
    if (now_ms > g_cycle_time_ms) {
      if (g_mag_data_ready_flag) {
        printf("Compass: heading=%6.2f deg  direction=%s  Mag: X=%8.2f  "
               "Y=%8.2f  Z=%8.2f G\n",
            g_heading_degree, GetCompassDirection(g_heading_degree),
            g_mag_data[0], g_mag_data[1], g_mag_data[2]);
        g_mag_data_ready_flag = false;
      }

      g_cycle_time_ms = now_ms + 1000;
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
