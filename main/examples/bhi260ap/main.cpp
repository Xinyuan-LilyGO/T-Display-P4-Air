/*
 * @Description: 使用 BHI2xy SensorAPI 桥接库读取 BHI260AP 四元数
 * @Author: LILYGO_L
 * @Date: 2026-07-15 23:20:00
 * @LastEditTime: 2026-07-27 14:12:28
 * @License: GPL 3.0
 */
#include <cmath>
#include <cstdio>

#include "bhi2xy_sensorapi_cpp_bus_driver_library.h"
#include "bhy2_parse.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lilygo_device_driver_library.h"

namespace {

using bhi2xy_sensorapi_cpp_bus_driver::Bhi2xy;

struct QuaternionData {
  float x = 0.0f;                 // X轴分量
  float y = 0.0f;                 // Y轴分量
  float z = 0.0f;                 // Z轴分量
  float w = 0.0f;                 // 标量分量
  float accuracy_degrees = 0.0f;  // 估算精度
  bool ready = false;             // 新数据标志
};

struct EulerAngles {
  float roll = 0.0f;   // 横滚角
  float pitch = 0.0f;  // 俯仰角
  float yaw = 0.0f;    // 偏航角（不使用磁力计，仅表示相对方向）
};

constexpr float kQuaternionScale = 16384.0f;
constexpr float kRadiansToDegrees = 57.2957795f;
constexpr float kSampleRateHz = 100.0f;
constexpr uint32_t kReportLatencyMs = 0;
constexpr uint32_t kLogIntervalMs = 500;

Bhi2xy* g_bhi260ap = nullptr;
QuaternionData g_quaternion;

/**
 * @brief 将四元数转换为便于阅读的欧拉角
 */
EulerAngles QuaternionToEuler(const QuaternionData& quaternion) {
  EulerAngles angles;

  const float sin_roll =
      2.0f * (quaternion.w * quaternion.x + quaternion.y * quaternion.z);
  const float cos_roll = 1.0f -
      2.0f * (quaternion.x * quaternion.x + quaternion.y * quaternion.y);
  angles.roll = std::atan2(sin_roll, cos_roll) * kRadiansToDegrees;

  const float sin_pitch =
      2.0f * (quaternion.w * quaternion.y - quaternion.z * quaternion.x);
  if (std::fabs(sin_pitch) >= 1.0f) {
    angles.pitch = std::copysign(90.0f, sin_pitch);
  } else {
    angles.pitch = std::asin(sin_pitch) * kRadiansToDegrees;
  }

  const float sin_yaw =
      2.0f * (quaternion.w * quaternion.z + quaternion.x * quaternion.y);
  const float cos_yaw = 1.0f -
      2.0f * (quaternion.y * quaternion.y + quaternion.z * quaternion.z);
  angles.yaw = std::atan2(sin_yaw, cos_yaw) * kRadiansToDegrees;

  return angles;
}

/**
 * @brief 解析游戏旋转矢量FIFO中的四元数数据
 * @param callback_info Bosch官方API提供的FIFO数据描述
 */
void ParseQuaternion(
    const struct bhy2_fifo_parse_data_info* callback_info, void*) {
  if (callback_info == nullptr || callback_info->data_ptr == nullptr ||
      callback_info->data_size != 11) {
    return;
  }

  struct bhy2_data_quaternion data{};
  bhy2_parse_quaternion(callback_info->data_ptr, &data);
  // Bosch四元数采用Q14定点格式
  g_quaternion.x = data.x / kQuaternionScale;
  g_quaternion.y = data.y / kQuaternionScale;
  g_quaternion.z = data.z / kQuaternionScale;
  g_quaternion.w = data.w / kQuaternionScale;
  g_quaternion.accuracy_degrees =
      data.accuracy / kQuaternionScale * kRadiansToDegrees;
  g_quaternion.ready = true;
}

/**
 * @brief 为已由板级驱动初始化的BHI260AP开启游戏旋转矢量虚拟传感器
 * @return 配置成功返回 true，失败返回 false
 */
bool ConfigureBhi260ap() {
  auto& driver = lilygo_device_driver::TDisplayP4AirDriver::GetInstance();
  if (!driver.IsBhi260apReady()) {
    const auto* bhi260ap = driver.chip().bhi260ap.get();
    printf("BHI260AP is not ready (error code: %d)\n",
        bhi260ap == nullptr ? BHY2_E_NULL_PTR : bhi260ap->last_error());
    return false;
  }
  g_bhi260ap = driver.chip().bhi260ap.get();
  printf("BHI260AP kernel version: %u\n",
      static_cast<unsigned>(g_bhi260ap->kernel_version()));

  // 固件启动后先注册回调，再处理初始化事件并更新虚拟传感器列表
  if (!g_bhi260ap->RegisterFifoCallback(
          BHY2_SENSOR_ID_GAMERV, ParseQuaternion)) {
    printf("BHI260AP callback registration failed (error code: %d)\n",
        g_bhi260ap->last_error());
    return false;
  }
  if (!g_bhi260ap->ProcessFifo() || !g_bhi260ap->UpdateVirtualSensorList()) {
    printf("BHI260AP sensor list update failed (error code: %d)\n",
        g_bhi260ap->last_error());
    return false;
  }
  if (!g_bhi260ap->ConfigureSensor(
          BHY2_SENSOR_ID_GAMERV, kSampleRateHz, kReportLatencyMs)) {
    printf("BHI260AP sensor configuration failed (error code: %d)\n",
        g_bhi260ap->last_error());
    return false;
  }

  printf("BHI260AP game rotation vector enabled at %.2f Hz\n", kSampleRateHz);
  return true;
}

}  // namespace

extern "C" void app_main() {
  printf("Ciallo\n");

  auto& driver = lilygo_device_driver::TDisplayP4AirDriver::GetInstance();
  driver.Init();
  const bool bhi260ap_ready = ConfigureBhi260ap();
  TickType_t last_log_tick = xTaskGetTickCount();

  while (true) {
    // 持续轮询FIFO以避免数据堆积，但仅按设定间隔输出最新姿态
    if (bhi260ap_ready && !g_bhi260ap->ProcessFifo()) {
      printf("BHI260AP FIFO processing failed (error code: %d)\n",
          g_bhi260ap->last_error());
    }

    const TickType_t now = xTaskGetTickCount();
    if (g_quaternion.ready &&
        now - last_log_tick >= pdMS_TO_TICKS(kLogIntervalMs)) {
      const EulerAngles angles = QuaternionToEuler(g_quaternion);
      printf(
          "Orientation: roll=%7.2f deg, pitch=%7.2f deg, "
          "yaw=%7.2f deg (relative)\n",
          angles.roll, angles.pitch, angles.yaw);
      g_quaternion.ready = false;
      last_log_tick = now;
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
