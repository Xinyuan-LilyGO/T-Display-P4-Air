/*
 * @Description: radiolib_lr1121_send_receive
 * @Author: LILYGO_L
 * @Date: 2026-02-28 16:21:31
 * @LastEditTime: 2026-05-24 16:30:00
 * @License: GPL 3.0
 */
#include "../common/t_display_p4_air_example.h"

#include <memory>

uint8_t g_send_package[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
volatile bool g_lr1121_irq_flag = false;

extern "C" void app_main(void) {
  printf("Ciallo\n");

  auto& driver = t_display_p4_air_example::InitDriver();
  auto esp32p4 = std::make_unique<cpp_bus_driver::Tool>();

  esp32p4->SetGpioMode(t_display_p4_air_example::gpio::button::kEsp32p4Boot,
      cpp_bus_driver::Tool::GpioMode::kInput,
      cpp_bus_driver::Tool::GpioStatus::kPullup);

  if (!driver.InitLr1121()) {
    printf("InitLr1121 failed\n");
    return;
  }
  auto* lr1121 = driver.chip().lr1121;

  const uint32_t rf_switch_dio_pins[] = {
      RADIOLIB_LR11X0_DIO5, RADIOLIB_LR11X0_DIO6, RADIOLIB_NC, RADIOLIB_NC,
      RADIOLIB_NC,
  };

  const Module::RfSwitchMode_t rf_switch_table[] = {
      {LR11x0::MODE_STBY, {0, 0}},
      {LR11x0::MODE_RX, {0, 1}},
      {LR11x0::MODE_TX, {0, 0}},
      {LR11x0::MODE_TX_HP, {1, 0}},
      {LR11x0::MODE_TX_HF, {0, 0}},
      {LR11x0::MODE_GNSS, {0, 0}},
      {LR11x0::MODE_WIFI, {0, 0}},
      END_OF_MODE_TABLE,
  };

  lr1121->setRfSwitchTable(rf_switch_dio_pins, rf_switch_table);
  lr1121->setIrqAction([]() { g_lr1121_irq_flag = true; });
  lr1121->startReceive();

  while (1) {
    if (!esp32p4->GpioRead(
            t_display_p4_air_example::gpio::button::kEsp32p4Boot)) {
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
