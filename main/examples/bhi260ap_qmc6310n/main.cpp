/*
 * @Description: bhi260ap_qmc6310n
 * @Author: LILYGO_L
 * @Date: 2026-02-27 10:56:03
 * @LastEditTime: 2026-05-24 16:30:00
 * @License: GPL 3.0
 */
#include <cmath>
#include <memory>

#include "../common/t_display_p4_air_example.h"
#include "SensorBHI260AP.hpp"
#include "SensorQMC6310.hpp"

#define BOSCH_APP30_SHUTTLE_BHI260_FW
#include "BoschFirmware.h"

float g_accel_data[3] = {0.0f, 0.0f, 0.0f};  // x, y, z
float g_mag_data[3] = {0.0f, 0.0f, 0.0f};    // x, y, z，单位 uT

bool g_accel_data_ready_flag = false;
bool g_mag_data_ready_flag = false;
int64_t g_cycle_time_ms = 0;

auto g_bhi260ap = std::make_unique<SensorBHI260AP>();
auto g_qmc6310n = std::make_unique<SensorQMC6310>();

void CalculateOrientation(
    const float accel[3], const float mag[3], float* pitch, float* roll,
    float* yaw) {
  *pitch =
      atan2f(-accel[0], sqrtf(accel[1] * accel[1] + accel[2] * accel[2])) *
      180.0f / M_PI;
  *roll = atan2f(accel[1], accel[2]) * 180.0f / M_PI;

  const float pitch_rad = *pitch * M_PI / 180.0f;
  const float roll_rad = *roll * M_PI / 180.0f;
  const float mx_h = mag[0] * cosf(pitch_rad) + mag[2] * sinf(pitch_rad);
  const float my_h = mag[0] * sinf(roll_rad) * sinf(pitch_rad) -
                     mag[1] * cosf(roll_rad) +
                     mag[2] * sinf(roll_rad) * cosf(pitch_rad);

  *yaw = atan2f(my_h, mx_h) * 180.0f / M_PI;
  if (*yaw < 0) {
    *yaw += 360.0f;
  }
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
}

bool InitQmc6310n(void) {
  printf("InitQmc6310n\n");

  Wire.set_freq(1000000);
  if (!g_qmc6310n->begin(Wire,
          t_display_p4_air_example::device::qmc6310n::kI2cAddress,
          t_display_p4_air_example::gpio::qmc6310n::kSda,
          t_display_p4_air_example::gpio::qmc6310n::kScl)) {
    printf("Qmc6310n begin failed\n");
    return false;
  }

  printf("Qmc6310n device id: %#X\n", g_qmc6310n->getChipID());
  g_qmc6310n->configMagnetometer(SensorQMC6310::MODE_CONTINUOUS,
      SensorQMC6310::RANGE_8G, SensorQMC6310::DATARATE_200HZ,
      SensorQMC6310::OSR_1, SensorQMC6310::DSR_1);
  printf("InitQmc6310n success\n");
  return true;
}

extern "C" void app_main(void) {
  printf("Ciallo\n");

  auto& driver = t_display_p4_air_example::InitDriver();
  Wire1.set_bus_handle(driver.bus().axp517_i2c_bus->bus_handle());
  Wire.set_bus_handle(driver.bus().xl9535_i2c_bus->bus_handle());
  InitBhi260ap();
  InitQmc6310n();

  while (1) {
    g_bhi260ap->update();

    if (g_qmc6310n->isDataReady()) {
      g_qmc6310n->readData();
      g_mag_data[0] = g_qmc6310n->getX();
      g_mag_data[1] = g_qmc6310n->getY();
      g_mag_data[2] = g_qmc6310n->getZ();
      g_mag_data_ready_flag = true;
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
