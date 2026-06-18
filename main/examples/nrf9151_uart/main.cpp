/*
 * @Description: nrf9151_uart
 * @Author: LILYGO_L
 * @Date: 2026-03-04 11:26:40
 * @LastEditTime: 2026-06-18 16:04:08
 * @License: GPL 3.0
 */
#include "lilygo_device_driver_library.h"

#include <memory>

namespace {

constexpr uart_port_t kPcUartPort = UART_NUM_0;
constexpr int kPcUartBufferSize = 2 * 1024;
constexpr int kUartReadTimeoutMs = 20;
constexpr int kLoopDelayMs = 10;

}  // namespace

extern "C" void app_main(void) {
  printf("Ciallo\n");

  auto& driver = lilygo_device_driver::TDisplayP4AirDriver::GetInstance();
  driver.Init();
  
  if (!driver.status().nrf9151.init_flag) {
    printf("Nrf9151 init failed\n");
    return;
  }

  auto& nrf9151_uart = driver.bus().nrf9151_uart_bus;
  if (nrf9151_uart == nullptr) {
    printf("Nrf9151 uart bus is null\n");
    return;
  }

  uart_driver_install(kPcUartPort, kPcUartBufferSize, 0, 0, NULL, 0);
  uart_flush_input(kPcUartPort);

  while (1) {
    size_t pc_uart_length = 0;
    uart_get_buffered_data_len(kPcUartPort, &pc_uart_length);
    if (pc_uart_length > 0) {
      auto buffer = std::make_unique<uint8_t[]>(pc_uart_length + 1);
      uart_read_bytes(kPcUartPort, buffer.get(), pc_uart_length,
          pdMS_TO_TICKS(kUartReadTimeoutMs));
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

    vTaskDelay(pdMS_TO_TICKS(kLoopDelayMs));
  }
}
