/*
 * @Description: lr1121_send_receive
 * @Author: LILYGO_L
 * @Date: 2026-02-28 16:21:31
 * @LastEditTime: 2026-07-28 00:00:00
 * @License: GPL 3.0
 */
#include "lilygo_device_driver_library.h"

#include <algorithm>
#include <array>
#include <memory>

namespace {

constexpr std::array<uint8_t, 9> kSendPackage = {
    1, 2, 3, 4, 5, 6, 7, 8, 9};
constexpr uint32_t kRadioIrqMask =
    LR11XX_SYSTEM_IRQ_TX_DONE | LR11XX_SYSTEM_IRQ_RX_DONE |
    LR11XX_SYSTEM_IRQ_HEADER_ERROR | LR11XX_SYSTEM_IRQ_CRC_ERROR |
    LR11XX_SYSTEM_IRQ_TIMEOUT;

lr11xx_radio_pkt_params_lora_t MakePacketConfig(uint8_t payload_length) {
  return {
      .preamble_len_in_symb = 8,
      .header_type = LR11XX_RADIO_LORA_PKT_EXPLICIT,
      .pld_len_in_bytes = payload_length,
      .crc = LR11XX_RADIO_LORA_CRC_ON,
      .iq = LR11XX_RADIO_LORA_IQ_STANDARD,
  };
}

bool SetPayloadLength(
    usp_cpp_bus_driver::Lr11xx* lr1121, uint8_t payload_length) {
  const lr11xx_radio_pkt_params_lora_t packet_config =
      MakePacketConfig(payload_length);
  return lr1121->Invoke(lr11xx_radio_set_lora_pkt_params, &packet_config) ==
         LR11XX_STATUS_OK;
}

bool ReadAndClearIrq(usp_cpp_bus_driver::Lr11xx* lr1121,
    lr11xx_system_irq_mask_t* irq_status) {
  return lr1121->Invoke(
             lr11xx_system_get_and_clear_irq_status, irq_status) ==
         LR11XX_STATUS_OK;
}

bool WaitForIrq(cpp_bus_driver::Tool* tool,
    usp_cpp_bus_driver::Lr11xx* lr1121,
    lr11xx_system_irq_mask_t* irq_status, uint32_t timeout_ms) {
  const int64_t deadline_ms = tool->GetSystemTimeMs() + timeout_ms;
  while (tool->GetSystemTimeMs() < deadline_ms) {
    if (tool->GpioRead(
            lilygo_device_driver::t_display_p4_air::gpio::lr1121::kInt)) {
      return ReadAndClearIrq(lr1121, irq_status);
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
  return false;
}

bool StartReceive(usp_cpp_bus_driver::Lr11xx* lr1121) {
  return SetPayloadLength(lr1121, 255) &&
         lr1121->Invoke(
             lr11xx_radio_set_rx_with_timeout_in_rtc_step, 0xFFFFFFU) ==
             LR11XX_STATUS_OK;
}

}  // namespace

extern "C" void app_main(void) {
  printf("Ciallo\n");

  auto& driver = lilygo_device_driver::TDisplayP4AirDriver::GetInstance();
  if (!driver.Init()) {
    printf("TDisplayP4AirDriver init failed\n");
    return;
  }
  auto tool = std::make_unique<cpp_bus_driver::Tool>();

  tool->SetGpioMode(
      lilygo_device_driver::t_display_p4_air::gpio::button::kEsp32p4Boot,
      cpp_bus_driver::Tool::GpioMode::kInput,
      cpp_bus_driver::Tool::GpioStatus::kPullup);

  if (!driver.status().lr1121.init_flag || driver.chip().lr1121 == nullptr) {
    printf("InitLr1121 failed\n");
    return;
  }
  auto* lr1121 = driver.chip().lr1121.get();

  if (lr1121->Invoke(lr11xx_system_clear_irq_status,
          LR11XX_SYSTEM_IRQ_ALL_MASK) != LR11XX_STATUS_OK ||
      lr1121->Invoke(lr11xx_system_set_dio_irq_params, kRadioIrqMask,
          LR11XX_SYSTEM_IRQ_NONE) != LR11XX_STATUS_OK ||
      !StartReceive(lr1121)) {
    printf("StartReceive failed\n");
    return;
  }

  while (true) {
    if (!tool->GpioRead(
            lilygo_device_driver::t_display_p4_air::gpio::button::
                kEsp32p4Boot)) {
      vTaskDelay(pdMS_TO_TICKS(300));

      printf("Lr1121 send package\n");
      lr11xx_system_irq_mask_t irq_status = LR11XX_SYSTEM_IRQ_NONE;
      const bool transmit_started =
          lr1121->Invoke(lr11xx_system_clear_irq_status,
              LR11XX_SYSTEM_IRQ_ALL_MASK) == LR11XX_STATUS_OK &&
          SetPayloadLength(
              lr1121, static_cast<uint8_t>(kSendPackage.size())) &&
          lr1121->WriteBuffer(kSendPackage.data(), kSendPackage.size()) &&
          lr1121->StartTransmit(5000);
      if (!transmit_started ||
          !WaitForIrq(tool.get(), lr1121, &irq_status, 6000) ||
          (irq_status & LR11XX_SYSTEM_IRQ_TX_DONE) == 0) {
        printf("transmit failed (irq: 0x%08lx)\n",
            static_cast<unsigned long>(irq_status));
      }

      if (!StartReceive(lr1121)) {
        printf("StartReceive failed\n");
        return;
      }
    }

    if (tool->GpioRead(
            lilygo_device_driver::t_display_p4_air::gpio::lr1121::kInt)) {
      lr11xx_system_irq_mask_t irq_status = LR11XX_SYSTEM_IRQ_NONE;
      if (!ReadAndClearIrq(lr1121, &irq_status)) {
        printf("read irq failed\n");
      } else if ((irq_status & LR11XX_SYSTEM_IRQ_RX_DONE) != 0 &&
                 (irq_status & (LR11XX_SYSTEM_IRQ_HEADER_ERROR |
                                   LR11XX_SYSTEM_IRQ_CRC_ERROR)) == 0) {
        lr11xx_radio_rx_buffer_status_t buffer_status = {};
        lr11xx_radio_pkt_status_lora_t packet_status = {};
        std::array<uint8_t, 255> receive_package = {};
        if (lr1121->Invoke(
                lr11xx_radio_get_rx_buffer_status, &buffer_status) ==
                LR11XX_STATUS_OK &&
            buffer_status.pld_len_in_bytes > 0 &&
            lr1121->ReadBuffer(buffer_status.buffer_start_pointer,
                receive_package.data(), buffer_status.pld_len_in_bytes) &&
            lr1121->Invoke(
                lr11xx_radio_get_lora_pkt_status, &packet_status) ==
                LR11XX_STATUS_OK) {
          printf("Lr1121 rssi: %d dbm, snr: %d db\n",
              packet_status.rssi_pkt_in_dbm, packet_status.snr_pkt_in_db);

          const size_t print_length =
              std::min(kSendPackage.size(),
                  static_cast<size_t>(buffer_status.pld_len_in_bytes));
          for (size_t i = 0; i < print_length; ++i) {
            printf("get Lr1121 data[%u]: %u\n", static_cast<unsigned>(i),
                static_cast<unsigned>(receive_package[i]));
          }
        }
      }

      if (!StartReceive(lr1121)) {
        printf("StartReceive failed\n");
        return;
      }
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
