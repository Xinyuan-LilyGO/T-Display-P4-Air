/*
 * @Description: iic_scan
 * @Author: LILYGO_L
 * @Date: 2025-06-13 12:06:14
 * @LastEditTime: 2026-05-24 16:30:00
 * @License: GPL 3.0
 */
#include "lilygo_device_driver_library.h"

#include <memory>
#include <vector>

void IicScan(const char* name,
    const std::shared_ptr<cpp_bus_driver::HardwareI2c1>& bus) {
  std::vector<uint8_t> address;
  if (!bus->Scan7bitAddress(&address)) {
    printf("%s scan failed\n", name);
    return;
  }

  for (size_t i = 0; i < address.size(); i++) {
    printf("%s discovered iic devices[%zu]: %#x\n", name, i, address[i]);
  }
}

extern "C" void app_main(void) {
  printf("Ciallo\n");

  auto& driver = lilygo_device_driver::TDisplayP4AirDriver::GetInstance();
  driver.Init();

  while (1) {
    IicScan("port1", driver.bus().xl9535_i2c_bus);
    IicScan("port2", driver.bus().axp517_i2c_bus);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
