/*
 * @Description: sgm38121
 * @Author: LILYGO_L
 * @Date: 2026-02-27 17:50:32
 * @LastEditTime: 2026-05-24 16:30:00
 * @License: GPL 3.0
 */
#include "lilygo_device_driver_library.h"

extern "C" void app_main(void) {
  printf("Ciallo\n");

  auto& driver = lilygo_device_driver::TDisplayP4AirDriver::GetInstance();
  driver.Init();
  auto& sgm38121 = driver.chip().sgm38121;

  sgm38121->SetOutputVoltage(
      cpp_bus_driver::Sgm38121::Channel::kAvdd1, 1800);
  sgm38121->SetOutputVoltage(
      cpp_bus_driver::Sgm38121::Channel::kAvdd2, 2800);

  while (1) {
    sgm38121->SetChannelStatus(cpp_bus_driver::Sgm38121::Channel::kAvdd1,
        cpp_bus_driver::Sgm38121::Status::kOn);
    sgm38121->SetChannelStatus(cpp_bus_driver::Sgm38121::Channel::kAvdd2,
        cpp_bus_driver::Sgm38121::Status::kOn);
    vTaskDelay(pdMS_TO_TICKS(1000));

    sgm38121->SetChannelStatus(cpp_bus_driver::Sgm38121::Channel::kAvdd1,
        cpp_bus_driver::Sgm38121::Status::kOff);
    sgm38121->SetChannelStatus(cpp_bus_driver::Sgm38121::Channel::kAvdd2,
        cpp_bus_driver::Sgm38121::Status::kOff);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
