/*
 * @Description: nrf9151_uart
 * @Author: LILYGO_L
 * @Date: 2026-03-04 11:26:40
 * @LastEditTime: 2026-05-24 16:30:00
 * @License: GPL 3.0
 */
#include "lilygo_device_driver_library.h"

#include <memory>

extern "C" void app_main(void) {
  printf("Ciallo\n");

  auto& driver = lilygo_device_driver::TDisplayP4AirDriver::GetInstance();
  driver.Init();
  driver.InitNrf9151();

  auto& nrf9151_uart = driver.bus().nrf9151_uart_bus;
  uart_driver_install(UART_NUM_0, 2 * 1024, 0, 0, NULL, 0);
  uart_flush_input(UART_NUM_0);

  while (1) {
    size_t pc_uart_length = 0;
    uart_get_buffered_data_len(UART_NUM_0, &pc_uart_length);
    if (pc_uart_length > 0) {

      auto buffer = std::make_unique<uint8_t[]>(pc_uart_length + 1);
      uart_read_bytes(
          UART_NUM_0, buffer.get(), pc_uart_length, pdMS_TO_TICKS(20));
      nrf9151_uart->Write(buffer.get(), pc_uart_length);
      printf("nrf9151 uart send(size: %zu): [%s]\n", pc_uart_length,
          reinterpret_cast<char*>(buffer.get()));
    }

    size_t nrf9151_uart_length = nrf9151_uart->GetRxBufferLength();
    if (nrf9151_uart_length > 0) {
      auto buffer = std::make_unique<uint8_t[]>(nrf9151_uart_length + 1);
      nrf9151_uart->Read(buffer.get(), nrf9151_uart_length);
      printf("nrf9151 uart receive(size: %zu): [%s]\n", nrf9151_uart_length,
          reinterpret_cast<char*>(buffer.get()));
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
