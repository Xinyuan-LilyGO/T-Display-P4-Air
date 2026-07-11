/*
 * @Description: nrf9151_uart
 * @Author: LILYGO_L
 * @Date: 2026-03-04 11:26:40
 * @LastEditTime: 2026-07-11 14:39:16
 * @License: GPL 3.0
 */
#include <memory>
#include <string>

#include "lilygo_device_driver_library.h"

namespace {

constexpr uart_port_t kPcUartPort = UART_NUM_0;
constexpr int kPcUartBufferSize = 2 * 1024;
constexpr int kUartReadTimeoutMs = 20;
constexpr int kLoopDelayMs = 10;
constexpr uint32_t kAtCommandTimeoutMs = 5000;
constexpr size_t kMaxAtCommandLength = 1024;

void ExecuteAtCommand(
    cpp_bus_driver::Nrf9151 &nrf9151, const std::string &command) {
  std::string response;
  const auto result =
      nrf9151.SendCommand(command.c_str(), &response, kAtCommandTimeoutMs);

  printf("nrf9151 AT command: [%s]\n", command.c_str());
  printf("nrf9151 AT result: %s\n",
      cpp_bus_driver::Nrf9151::CommandResultToString(result));
  printf("nrf9151 AT response: [%s]\n", response.c_str());
}

}  // namespace

extern "C" void app_main(void) {
  printf("Ciallo\n");

  auto &driver = lilygo_device_driver::TDisplayP4AirDriver::GetInstance();
  driver.Init();

  if (!driver.status().nrf9151.init_flag) {
    printf("Nrf9151 init failed\n");
    return;
  }

  auto &nrf9151 = driver.chip().nrf9151;
  auto &nrf9151_uart = driver.bus().nrf9151_uart_bus;
  if ((nrf9151 == nullptr) || (nrf9151_uart == nullptr)) {
    printf("Nrf9151 driver or uart bus is null\n");
    return;
  }

  uart_driver_install(kPcUartPort, kPcUartBufferSize, 0, 0, NULL, 0);
  uart_flush_input(kPcUartPort);

  std::string command_buffer;
  while (1) {
    size_t pc_uart_length = 0;
    uart_get_buffered_data_len(kPcUartPort, &pc_uart_length);
    if (pc_uart_length > 0) {
      auto buffer = std::make_unique<uint8_t[]>(pc_uart_length);
      const int32_t read_length = uart_read_bytes(kPcUartPort, buffer.get(),
          pc_uart_length, pdMS_TO_TICKS(kUartReadTimeoutMs));
      if (read_length > 0) {
        command_buffer.append(reinterpret_cast<char *>(buffer.get()),
            static_cast<size_t>(read_length));
      }

      if (command_buffer.size() > kMaxAtCommandLength) {
        printf("AT command is too long, buffer cleared\n");
        command_buffer.clear();
      }

      while (true) {
        const size_t line_end = command_buffer.find_first_of("\r\n");
        if (line_end == std::string::npos) {
          break;
        }

        const std::string command = command_buffer.substr(0, line_end);
        size_t next = line_end;
        while ((next < command_buffer.size()) &&
               ((command_buffer[next] == '\r') ||
                   (command_buffer[next] == '\n'))) {
          next++;
        }
        command_buffer.erase(0, next);

        if (!command.empty()) {
          ExecuteAtCommand(*nrf9151, command);
        }
      }
    }

    size_t nrf9151_uart_length = nrf9151_uart->GetRxBufferLength();
    if (nrf9151_uart_length > 0) {
      auto buffer = std::make_unique<uint8_t[]>(nrf9151_uart_length);
      const int32_t read_length = nrf9151_uart->Read(
          buffer.get(), static_cast<uint32_t>(nrf9151_uart_length));
      if (read_length > 0) {
        const int printable_length = static_cast<int>(read_length);
        printf("nrf9151 URC(size: %d): [%.*s]\n", printable_length,
            printable_length,
            reinterpret_cast<char *>(buffer.get()));
      }
    }

    vTaskDelay(pdMS_TO_TICKS(kLoopDelayMs));
  }
}
