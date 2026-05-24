/*
 * @Description: st25r3916
 * @Author: LILYGO_L
 * @Date: 2026-03-02 13:59:47
 * @LastEditTime: 2026-05-24 16:30:00
 * @License: GPL 3.0
 */
#include "../common/t_display_p4_air_example.h"

#include "st25r3916_driver.h"

extern "C" void app_main(void) {
  printf("Ciallo\n");

  auto& driver = t_display_p4_air_example::InitDriver();
  Wire.set_bus_handle(driver.bus().xl9535_i2c_bus->bus_handle());

  St25r3916_Init();

  while (1) {
    St25r3916_Loop();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
