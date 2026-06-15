/*
 * @Description: xl9535
 * @Author: LILYGO_L
 * @Date: 2026-02-02 17:40:30
 * @LastEditTime: 2026-05-24 16:30:00
 * @License: GPL 3.0
 */
#include "lilygo_device_driver_library.h"

extern "C" void app_main(void) {
  printf("Ciallo\n");

  auto& driver = lilygo_device_driver::TDisplayP4AirDriver::GetInstance();
  driver.Init();
  auto& xl9535 = driver.chip().xl9535;

  xl9535->SetGpioMode(lilygo_device_driver::t_display_p4_air::gpio::xl9535::kLed1,
      cpp_bus_driver::Xl95x5::Mode::kOutput);
  xl9535->SetGpioMode(cpp_bus_driver::Xl95x5::Pin::kIo7,
      cpp_bus_driver::Xl95x5::Mode::kInput);

  while (1) {
    xl9535->GpioWrite(lilygo_device_driver::t_display_p4_air::gpio::xl9535::kLed1,
        cpp_bus_driver::Xl95x5::Value::kHigh);
    printf("Xl9535 io7: %d\n",
        xl9535->GpioRead(cpp_bus_driver::Xl95x5::Pin::kIo7));
    vTaskDelay(pdMS_TO_TICKS(1000));

    xl9535->GpioWrite(lilygo_device_driver::t_display_p4_air::gpio::xl9535::kLed1,
        cpp_bus_driver::Xl95x5::Value::kLow);
    printf("Xl9535 io7: %d\n",
        xl9535->GpioRead(cpp_bus_driver::Xl95x5::Pin::kIo7));
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
