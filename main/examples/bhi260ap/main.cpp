/*
 * @Description: bhi260ap
 * @Author: LILYGO_L
 * @Date: 2026-02-27 09:07:56
 * @LastEditTime: 2026-06-16
 * @License: GPL 3.0
 */
#include <cmath>
#include <memory>

#include "lilygo_device_driver_library.h"
#include "SensorBHI260AP.hpp"

#define BOSCH_APP30_SHUTTLE_BHI260_FW
#include "BoschFirmware.h"

// 加速度计数据缓存：x, y, z
float g_accel_data[3] = {0.0f, 0.0f, 0.0f};
// 陀螺仪数据缓存：x, y, z
float g_gyro_data[3] = {0.0f, 0.0f, 0.0f};

bool g_accel_data_ready_flag = false;
bool g_gyro_data_ready_flag = false;
int64_t g_cycle_time_ms = 0;

auto g_bhi260ap = std::make_unique<SensorBHI260AP>();

/**
 * @brief 根据加速度和陀螺仪数据计算姿态角
 * @param accel  加速度数据 [x, y, z]
 * @param gyro   陀螺仪数据 [x, y, z]
 * @param pitch  俯仰角（度）输出
 * @param roll   横滚角（度）输出
 * @param yaw    偏航角（度）输出（通过陀螺仪积分）
 */
void CalculateOrientation(const float accel[3], const float gyro[3],
    float* pitch, float* roll, float* yaw) {
  *pitch =
      atan2f(-accel[0], sqrtf(accel[1] * accel[1] + accel[2] * accel[2])) *
      180.0f / M_PI;
  *roll = atan2f(accel[1], accel[2]) * 180.0f / M_PI;

  // 通过陀螺仪 Z 轴积分计算偏航角
  static float last_yaw = 0.0f;
  static int64_t last_time_ms = 0;
  const int64_t now_ms = esp_timer_get_time() / 1000;
  const float dt =
      (last_time_ms == 0) ? 0.01f : (now_ms - last_time_ms) / 1000.0f;
  last_time_ms = now_ms;

  last_yaw += gyro[2] * dt;
  *yaw = last_yaw;
}

/**
 * @brief 初始化 BHI260AP 传感器
 */
void InitBhi260ap(void) {
  printf("InitBhi260ap\n");

  // bosch_firmware_type 为 0 表示加载到 RAM
  g_bhi260ap->setFirmware(
      bosch_firmware_image, bosch_firmware_size, bosch_firmware_type, true);
  g_bhi260ap->setBootFromFlash(bosch_firmware_type);

  // 获取 I2C 总线句柄
  auto& driver = lilygo_device_driver::TDisplayP4AirDriver::GetInstance();
  i2c_master_bus_handle_t i2c_handle = driver.bus().axp517_i2c_bus->bus_handle();

  // 初始化传感器
  if (!g_bhi260ap->begin(i2c_handle,
          lilygo_device_driver::t_display_p4_air::device::bhi260ap::kI2cAddress)) {
    printf("Bhi260ap begin failed (error code: %s)\n",
        g_bhi260ap->getError());
    return;
  }

  printf("Bhi260ap begin success\n");
  BoschSensorInfo info = g_bhi260ap->getSensorInfo();
  info.printInfo();

  // 配置加速度计和陀螺仪直通模式，采样率 100Hz
  g_bhi260ap->configure(BoschSensorID::ACCEL_PASSTHROUGH, 100.0, 0);
  g_bhi260ap->configure(BoschSensorID::GYRO_PASSTHROUGH, 100.0, 0);

  // 注册加速度数据回调
  g_bhi260ap->onResultEvent(BoschSensorID::ACCEL_PASSTHROUGH,
      [](uint8_t sensor_id, const uint8_t* data_ptr, uint32_t len,
          uint64_t* timestamp, void* user_data) {
        struct bhy2_data_xyz data;
        const float scaling_factor = g_bhi260ap->getScaling(sensor_id);
        bhy2_parse_xyz(const_cast<uint8_t*>(data_ptr), &data);

        g_accel_data[0] = data.x * scaling_factor;
        g_accel_data[1] = data.y * scaling_factor;
        g_accel_data[2] = data.z * scaling_factor;
        g_accel_data_ready_flag = true;
      });

  // 注册陀螺仪数据回调
  g_bhi260ap->onResultEvent(BoschSensorID::GYRO_PASSTHROUGH,
      [](uint8_t sensor_id, const uint8_t* data_ptr, uint32_t len,
          uint64_t* timestamp, void* user_data) {
        struct bhy2_data_xyz data;
        const float scaling_factor = g_bhi260ap->getScaling(sensor_id);
        bhy2_parse_xyz(const_cast<uint8_t*>(data_ptr), &data);

        g_gyro_data[0] = data.x * scaling_factor;
        g_gyro_data[1] = data.y * scaling_factor;
        g_gyro_data[2] = data.z * scaling_factor;
        g_gyro_data_ready_flag = true;
      });
}

extern "C" void app_main(void) {
  printf("Ciallo\n");

  auto& driver = lilygo_device_driver::TDisplayP4AirDriver::GetInstance();
  driver.Init();
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
