/*
 * @Description: radiolib_lr1121_send_receive
 * @Author: LILYGO_L
 * @Date: 2026-02-28 16:21:31
 * @LastEditTime: 2026-05-24 16:30:00
 * @License: GPL 3.0
 */
#include "lilygo_device_driver_library.h"

#include <memory>

uint8_t g_send_package[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
volatile bool g_lr1121_irq_flag = false;

extern "C" void app_main(void) {
  printf("Ciallo\n");

  auto& driver = lilygo_device_driver::TDisplayP4AirDriver::GetInstance();
  driver.Init();
  auto esp32p4 = std::make_unique<cpp_bus_driver::Tool>();

  esp32p4->SetGpioMode(
      lilygo_device_driver::t_display_p4_air::gpio::button::kEsp32p4Boot,
      cpp_bus_driver::Tool::GpioMode::kInput,
      cpp_bus_driver::Tool::GpioStatus::kPullup);

  if (!driver.status().lr1121.init_flag || driver.chip().lr1121 == nullptr) {
    printf("InitLr1121 failed\n");
    return;
  }
  auto* lr1121 = driver.chip().lr1121;

  lr1121->setIrqAction([]() { g_lr1121_irq_flag = true; });
  lr1121->startReceive();

  while (1) {
    if (!esp32p4->GpioRead(
            lilygo_device_driver::t_display_p4_air::gpio::button::kEsp32p4Boot)) {
      vTaskDelay(pdMS_TO_TICKS(300));

      printf("Lr1121 send package\n");
      int16_t status = lr1121->transmit(g_send_package, sizeof(g_send_package));
      if (status != RADIOLIB_ERR_NONE) {
        printf("transmit failed (error code: %d)\n", status);
      }

      status = lr1121->startReceive();
      if (status != RADIOLIB_ERR_NONE) {
        printf("startReceive failed (error code: %d)\n", status);
      }

      g_lr1121_irq_flag = false;
    }

    if (g_lr1121_irq_flag) {
      uint8_t receive_package[255] = {};
      if (lr1121->readData(receive_package, sizeof(g_send_package)) ==
          RADIOLIB_ERR_NONE) {
        printf("Lr1121 rssi: %.2f dbm, snr: %.2f db\n", lr1121->getRSSI(),
            lr1121->getSNR());

        for (uint8_t i = 0; i < sizeof(g_send_package); i++) {
          printf("get Lr1121 data[%d]: %d\n", i, receive_package[i]);
        }
      }

      g_lr1121_irq_flag = false;
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
