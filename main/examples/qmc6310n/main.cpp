/*
 * @Description: qmc6310n
 * @Author: LILYGO_L
 * @Date: 2026-02-27 10:56:03
 * @LastEditTime: 2026-07-28
 * @License: GPL 3.0
 */
#include <cmath>
#include <cstdint>

#include "bhi2xy_sensorapi_cpp_bus_driver_library.h"
#include "bhy2_parse.h"
#include "lilygo_device_driver_library.h"

constexpr float kAccelerometerScale = 1.0f / 4096.0f;

float g_accel_data[3] = {0.0f, 0.0f, 0.0f};
float g_mag_data[3] = {0.0f, 0.0f, 0.0f};

bool g_accel_data_ready_flag = false;
bool g_mag_data_ready_flag = false;
int64_t g_cycle_time_ms = 0;

bhi2xy_sensorapi_cpp_bus_driver::Bhi2xy* g_bhi260ap = nullptr;
SensorQMC6310* g_qmc6310n = nullptr;

void ParseAcceleration(
    const struct bhy2_fifo_parse_data_info* callback_info, void*) {
  if (callback_info == nullptr || callback_info->data_ptr == nullptr ||
      callback_info->data_size < 6) {
    return;
  }

  struct bhy2_data_xyz data{};
  bhy2_parse_xyz(callback_info->data_ptr, &data);
  g_accel_data[0] = data.x * kAccelerometerScale;
  g_accel_data[1] = data.y * kAccelerometerScale;
  g_accel_data[2] = data.z * kAccelerometerScale;
  g_accel_data_ready_flag = true;
}

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

bool ConfigureBhi260ap(void) {
  printf("ConfigureBhi260ap\n");

  auto& driver = lilygo_device_driver::TDisplayP4AirDriver::GetInstance();
  if (!driver.IsBhi260apReady()) {
    const auto* bhi260ap = driver.chip().bhi260ap.get();
    printf("BHI260AP is not ready (error code: %d)\n",
        bhi260ap == nullptr ? BHY2_E_NULL_PTR : bhi260ap->last_error());
    return false;
  }
  g_bhi260ap = driver.chip().bhi260ap.get();

  if (!g_bhi260ap->RegisterFifoCallback(
          BHY2_SENSOR_ID_ACC_PASS, ParseAcceleration)) {
    printf("BHI260AP callback registration failed (error code: %d)\n",
        g_bhi260ap->last_error());
    return false;
  }
  if (!g_bhi260ap->ProcessFifo() ||
      !g_bhi260ap->UpdateVirtualSensorList()) {
    printf("BHI260AP sensor list update failed (error code: %d)\n",
        g_bhi260ap->last_error());
    return false;
  }
  if (!g_bhi260ap->ConfigureSensor(
          BHY2_SENSOR_ID_ACC_PASS, 100.0f, 0)) {
    printf("BHI260AP accelerometer configuration failed (error code: %d)\n",
        g_bhi260ap->last_error());
    return false;
  }

  printf("ConfigureBhi260ap success\n");
  return true;
}

bool GetQmc6310n(void) {
  auto& driver = lilygo_device_driver::TDisplayP4AirDriver::GetInstance();
  if (!driver.IsQmc6310nReady()) {
    printf("Qmc6310n is not ready\n");
    return false;
  }
  g_qmc6310n = driver.chip().qmc6310n.get();
  printf("Qmc6310n device id: %#X\n", g_qmc6310n->getChipID());
  return true;
}

extern "C" void app_main(void) {
  printf("Ciallo\n");

  auto& driver = lilygo_device_driver::TDisplayP4AirDriver::GetInstance();
  driver.Init();
  const bool bhi260ap_ready = ConfigureBhi260ap();
  const bool qmc6310n_ready = GetQmc6310n();

  while (1) {
    if (bhi260ap_ready && !g_bhi260ap->ProcessFifo()) {
      printf("BHI260AP FIFO processing failed (error code: %d)\n",
          g_bhi260ap->last_error());
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
