/*
 * @Description: qmc6310n
 * @Author: LILYGO_L
 * @Date: 2026-02-27 09:48:55
 * @LastEditTime: 2026-05-24 16:30:00
 * @License: GPL 3.0
 */
#include "lilygo_device_driver_library.h"

#include <memory>

#include "SensorQMC6310.hpp"

float g_mag_data[3] = {0.0f, 0.0f, 0.0f};  // x, y, z, unit: uT
bool g_mag_data_ready_flag = false;
int64_t g_cycle_time_ms = 0;

auto g_qmc6310n = std::make_unique<SensorQMC6310>();

bool InitQmc6310n(void) {
  printf("InitQmc6310n\n");

  Wire.set_freq(1000000);
  if (!g_qmc6310n->begin(Wire,
          lilygo_device_driver::t_display_p4_air::device::qmc6310n::kI2cAddress,
          lilygo_device_driver::t_display_p4_air::gpio::qmc6310n::kSda,
          lilygo_device_driver::t_display_p4_air::gpio::qmc6310n::kScl)) {
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

  auto& driver = lilygo_device_driver::TDisplayP4AirDriver::GetInstance();
  driver.Init();
  Wire.set_bus_handle(driver.bus().xl9535_i2c_bus->bus_handle());
  InitQmc6310n();

  while (1) {
    if (g_qmc6310n->isDataReady()) {
      g_qmc6310n->readData();
      g_mag_data[0] = g_qmc6310n->getX();
      g_mag_data[1] = g_qmc6310n->getY();
      g_mag_data[2] = g_qmc6310n->getZ();
      g_mag_data_ready_flag = true;
    }

    const int64_t now_ms = esp_timer_get_time() / 1000;
    if (now_ms > g_cycle_time_ms) {
      if (g_mag_data_ready_flag) {
        printf("Mag: X: %8.2f  Y: %8.2f  Z: %8.2f uT\n", g_mag_data[0],
            g_mag_data[1], g_mag_data[2]);
        g_mag_data_ready_flag = false;
      }

      g_cycle_time_ms = now_ms + 1000;
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
