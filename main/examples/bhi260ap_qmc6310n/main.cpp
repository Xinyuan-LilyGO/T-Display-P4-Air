/*
 * @Description: bhi260ap_qmc6310n
 * @Author: LILYGO_L
 * @Date: 2026-02-27 10:56:03
 * @LastEditTime: 2026-06-16
 * @License: GPL 3.0
 */
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <memory>

#include "lilygo_device_driver_library.h"
#include "SensorBHI260AP.hpp"
#include "SensorQMC6310.hpp"

#define BOSCH_APP30_SHUTTLE_BHI260_FW
#include "BoschFirmware.h"

constexpr int kQmc6310nCalibrationTimeMs = 20000;

float g_accel_data[3] = {0.0f, 0.0f, 0.0f};
float g_mag_data[3] = {0.0f, 0.0f, 0.0f};

bool g_accel_data_ready_flag = false;
bool g_mag_data_ready_flag = false;
int64_t g_cycle_time_ms = 0;

auto g_bhi260ap = std::make_unique<SensorBHI260AP>();
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

void CalculateOrientation(
    const float accel[3], const float mag[3], float* pitch, float* roll,
    float* yaw) {
  const float accel_z = -accel[2];

  *pitch =
      atan2f(-accel[0], sqrtf(accel[1] * accel[1] + accel_z * accel_z)) *
      180.0f / M_PI;
  *roll = atan2f(accel[1], accel_z) * 180.0f / M_PI;

  const float pitch_rad = *pitch * M_PI / 180.0f;
  const float roll_rad = *roll * M_PI / 180.0f;
  const float mx_h = mag[0] * cosf(pitch_rad) + mag[2] * sinf(pitch_rad);
  const float my_h = mag[0] * sinf(roll_rad) * sinf(pitch_rad) -
                     mag[2] * sinf(roll_rad) * cosf(pitch_rad) +
                     mag[1] * cosf(roll_rad);

  *yaw = NormalizeDegree360(atan2f(my_h, mx_h) * 180.0f / M_PI);
}

bool InitBhi260ap(void) {
  printf("InitBhi260ap\n");

  g_bhi260ap->setFirmware(
      bosch_firmware_image, bosch_firmware_size, bosch_firmware_type, true);
  g_bhi260ap->setBootFromFlash(bosch_firmware_type);

  auto& driver = lilygo_device_driver::TDisplayP4AirDriver::GetInstance();
  i2c_master_bus_handle_t i2c_handle = driver.bus().axp517_i2c_bus->bus_handle();

  if (!g_bhi260ap->begin(i2c_handle,
          lilygo_device_driver::t_display_p4_air::device::bhi260ap::kI2cAddress)) {
    printf("Bhi260ap begin failed (error code: %s)\n",
        g_bhi260ap->getError());
    return false;
  }

  printf("Bhi260ap begin success\n");
  BoschSensorInfo info = g_bhi260ap->getSensorInfo();
  info.printInfo();

  if (!g_bhi260ap->configure(BoschSensorID::ACCEL_PASSTHROUGH, 100.0, 0)) {
    printf("Bhi260ap accel configure failed (error code: %s)\n",
        g_bhi260ap->getError());
    return false;
  }

  if (!g_bhi260ap->onResultEvent(BoschSensorID::ACCEL_PASSTHROUGH,
          [](uint8_t sensor_id, const uint8_t* data_ptr, uint32_t len,
              uint64_t* timestamp, void* user_data) {
            struct bhy2_data_xyz data;
            const float scaling_factor = g_bhi260ap->getScaling(sensor_id);
            bhy2_parse_xyz(const_cast<uint8_t*>(data_ptr), &data);

            g_accel_data[0] = data.x * scaling_factor;
            g_accel_data[1] = data.y * scaling_factor;
            g_accel_data[2] = data.z * scaling_factor;
            g_accel_data_ready_flag = true;
          })) {
    printf("Bhi260ap accel callback register failed (error code: %s)\n",
        g_bhi260ap->getError());
    return false;
  }

  printf("InitBhi260ap success\n");
  return true;
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
  const bool bhi260ap_ready = InitBhi260ap();
  bool qmc6310n_ready = InitQmc6310n();
  if (qmc6310n_ready) {
    qmc6310n_ready = CalibrateQmc6310n();
  }

  while (1) {
    if (bhi260ap_ready) {
      g_bhi260ap->update();
    }

    if (qmc6310n_ready && g_qmc6310n->isDataReady()) {
      MagnetometerData mag_data;
      if (g_qmc6310n->readData(mag_data)) {
        g_mag_data[0] = mag_data.magnetic_field.x;
        g_mag_data[1] = mag_data.magnetic_field.y;
        g_mag_data[2] = mag_data.magnetic_field.z;
        g_mag_data_ready_flag = true;
      }
    }

    const int64_t now_ms = esp_timer_get_time() / 1000;
    if (now_ms > g_cycle_time_ms) {
      if (g_accel_data_ready_flag && g_mag_data_ready_flag) {
        float pitch = 0.0f;
        float roll = 0.0f;
        float yaw = 0.0f;
        CalculateOrientation(g_accel_data, g_mag_data, &pitch, &roll, &yaw);
        printf("Orientation: pitch=%.2f, roll=%.2f, yaw=%.2f\n", pitch, roll,
            yaw);

        g_accel_data_ready_flag = false;
        g_mag_data_ready_flag = false;
      }

      g_cycle_time_ms = now_ms + 1000;
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
