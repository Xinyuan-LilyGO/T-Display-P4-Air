/*
 * @Description: bhi260ap
 * @Author: LILYGO_L
 * @Date: 2026-02-27 09:07:56
 * @LastEditTime: 2026-05-24 14:58:02
 * @License: GPL 3.0
 */
#include <cmath>
#include <memory>

#include "../common/t_display_p4_air_example.h"
#include "SensorBHI260AP.hpp"

#define BOSCH_APP30_SHUTTLE_BHI260_FW
#include "BoschFirmware.h"

float g_accel_data[3] = {0.0f, 0.0f, 0.0f};  // x, y, z
float g_gyro_data[3] = {0.0f, 0.0f, 0.0f};   // x, y, z

bool g_accel_data_ready_flag = false;
bool g_gyro_data_ready_flag = false;
int64_t g_cycle_time_ms = 0;

auto g_bhi260ap = std::make_unique<SensorBHI260AP>();

void CalculateOrientation(const float accel[3], const float gyro[3],
    float* pitch, float* roll, float* yaw) {
  *pitch =
      atan2f(-accel[0], sqrtf(accel[1] * accel[1] + accel[2] * accel[2])) *
      180.0f / M_PI;
  *roll = atan2f(accel[1], accel[2]) * 180.0f / M_PI;

  static float last_yaw = 0.0f;
  static int64_t last_time_ms = 0;
  const int64_t now_ms = esp_timer_get_time() / 1000;
  const float dt =
      (last_time_ms == 0) ? 0.01f : (now_ms - last_time_ms) / 1000.0f;
  last_time_ms = now_ms;

  last_yaw += gyro[2] * dt;
  *yaw = last_yaw;
}

void InitBhi260ap(void) {
  printf("InitBhi260ap\n");

  g_bhi260ap->setFirmware(
      bosch_firmware_image, bosch_firmware_size, bosch_firmware_type, true);
  g_bhi260ap->setBootFromFlash(bosch_firmware_type);

  Wire1.set_freq(1000000);
  if (!g_bhi260ap->begin(Wire1,
          t_display_p4_air_example::device::bhi260ap::kI2cAddress,
          t_display_p4_air_example::gpio::bhi260ap::kSda,
          t_display_p4_air_example::gpio::bhi260ap::kScl)) {
    printf("Bhi260ap begin failed (error code: %s)\n",
        g_bhi260ap->getError());
    return;
  }

  printf("Bhi260ap begin success\n");
  BoschSensorInfo info = g_bhi260ap->getSensorInfo();
  info.printInfo();

  g_bhi260ap->configure(SensorBHI260AP::ACCEL_PASSTHROUGH, 100.0, 0);
  g_bhi260ap->configure(SensorBHI260AP::GYRO_PASSTHROUGH, 100.0, 0);

  g_bhi260ap->onResultEvent(SensorBHI260AP::ACCEL_PASSTHROUGH,
      [](uint8_t sensor_id, uint8_t* data_ptr, uint32_t len,
          uint64_t* timestamp, void* user_data) {
        struct bhy2_data_xyz data;
        const float scaling_factor = g_bhi260ap->getScaling(sensor_id);
        bhy2_parse_xyz(data_ptr, &data);

        g_accel_data[0] = data.x * scaling_factor;
        g_accel_data[1] = data.y * scaling_factor;
        g_accel_data[2] = data.z * scaling_factor;
        g_accel_data_ready_flag = true;
      });

  g_bhi260ap->onResultEvent(SensorBHI260AP::GYRO_PASSTHROUGH,
      [](uint8_t sensor_id, uint8_t* data_ptr, uint32_t len,
          uint64_t* timestamp, void* user_data) {
        struct bhy2_data_xyz data;
        const float scaling_factor = g_bhi260ap->getScaling(sensor_id);
        bhy2_parse_xyz(data_ptr, &data);

        g_gyro_data[0] = data.x * scaling_factor;
        g_gyro_data[1] = data.y * scaling_factor;
        g_gyro_data[2] = data.z * scaling_factor;
        g_gyro_data_ready_flag = true;
      });
}

extern "C" void app_main(void) {
  printf("Ciallo\n");

  auto& driver = t_display_p4_air_example::InitDriver();
  Wire1.set_bus_handle(driver.bus().axp517_i2c_bus->bus_handle());
  InitBhi260ap();

  while (1) {
    g_bhi260ap->update();

    const int64_t now_ms = esp_timer_get_time() / 1000;
    if (now_ms > g_cycle_time_ms) {
      if (g_accel_data_ready_flag && g_gyro_data_ready_flag) {
        float pitch = 0.0f;
        float roll = 0.0f;
        float yaw = 0.0f;
        CalculateOrientation(g_accel_data, g_gyro_data, &pitch, &roll, &yaw);
        printf("Orientation: pitch=%.2f, roll=%.2f, yaw=%.2f\n", pitch, roll,
            yaw);

        g_accel_data_ready_flag = false;
        g_gyro_data_ready_flag = false;
      }

      g_cycle_time_ms = now_ms + 1000;
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
