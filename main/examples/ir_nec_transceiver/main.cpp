/*
 * @Description: ir_nec_transceiver
 * @Author: LILYGO_L
 * @Date: 2026-03-03 10:33:23
 * @LastEditTime: 2026-05-24 16:30:00
 * @License: GPL 3.0
 */
#include "../common/t_display_p4_air_example.h"

#include "driver/rmt_rx.h"
#include "driver/rmt_tx.h"
#include "esp_err.h"
#include "ir_nec_encoder.h"

namespace {

constexpr uint32_t kNecLeadingCodeDuration0 = 9000;
constexpr uint32_t kNecLeadingCodeDuration1 = 4500;
constexpr uint32_t kNecPayloadZeroDuration0 = 560;
constexpr uint32_t kNecPayloadZeroDuration1 = 560;
constexpr uint32_t kNecPayloadOneDuration0 = 560;
constexpr uint32_t kNecPayloadOneDuration1 = 1690;
constexpr uint32_t kNecRepeatCodeDuration0 = 9000;
constexpr uint32_t kNecRepeatCodeDuration1 = 2250;

rmt_channel_handle_t g_rx_channel = nullptr;
rmt_channel_handle_t g_tx_channel = nullptr;
QueueHandle_t g_receive_queue = nullptr;
rmt_symbol_word_t g_raw_symbols[64] = {};
rmt_encoder_handle_t g_nec_encoder = nullptr;

uint16_t g_nec_code_address = 0;
uint16_t g_nec_code_command = 0;

bool RmtRxDoneCallback(rmt_channel_handle_t channel,
    const rmt_rx_done_event_data_t* edata, void* user_data) {
  BaseType_t high_task_wakeup = pdFALSE;
  QueueHandle_t queue = static_cast<QueueHandle_t>(user_data);
  xQueueSendFromISR(queue, edata, &high_task_wakeup);
  return high_task_wakeup == pdTRUE;
}

bool NecCheckInRange(uint32_t signal_duration, uint32_t spec_duration) {
  return signal_duration <
             (spec_duration +
                 t_display_p4_air_example::device::infrared::kNecDecodeMargin) &&
         signal_duration >
             (spec_duration -
                 t_display_p4_air_example::device::infrared::kNecDecodeMargin);
}

bool NecParseLogic0(rmt_symbol_word_t* rmt_nec_symbols) {
  return NecCheckInRange(
             rmt_nec_symbols->duration0, kNecPayloadZeroDuration0) &&
         NecCheckInRange(
             rmt_nec_symbols->duration1, kNecPayloadZeroDuration1);
}

bool NecParseLogic1(rmt_symbol_word_t* rmt_nec_symbols) {
  return NecCheckInRange(rmt_nec_symbols->duration0, kNecPayloadOneDuration0) &&
         NecCheckInRange(rmt_nec_symbols->duration1, kNecPayloadOneDuration1);
}

bool NecParseFrame(rmt_symbol_word_t* rmt_nec_symbols) {
  rmt_symbol_word_t* current = rmt_nec_symbols;
  uint16_t address = 0;
  uint16_t command = 0;

  bool valid_leading_code =
      NecCheckInRange(current->duration0, kNecLeadingCodeDuration0) &&
      NecCheckInRange(current->duration1, kNecLeadingCodeDuration1);
  if (!valid_leading_code) {
    return false;
  }
  current++;

  for (int i = 0; i < 16; i++) {
    if (NecParseLogic1(current)) {
      address |= (1 << i);
    } else if (!NecParseLogic0(current)) {
      return false;
    }
    current++;
  }

  for (int i = 0; i < 16; i++) {
    if (NecParseLogic1(current)) {
      command |= (1 << i);
    } else if (!NecParseLogic0(current)) {
      return false;
    }
    current++;
  }

  g_nec_code_address = address;
  g_nec_code_command = command;
  return true;
}

bool NecParseFrameRepeat(rmt_symbol_word_t* rmt_nec_symbols) {
  return NecCheckInRange(
             rmt_nec_symbols->duration0, kNecRepeatCodeDuration0) &&
         NecCheckInRange(rmt_nec_symbols->duration1, kNecRepeatCodeDuration1);
}

void ParseNecFrame(rmt_symbol_word_t* rmt_nec_symbols, size_t symbol_num) {
  printf("----------nec frame start----------\n");
  for (size_t i = 0; i < symbol_num; i++) {
    printf("{%d:%d},{%d:%d}\n", rmt_nec_symbols[i].level0,
        rmt_nec_symbols[i].duration0, rmt_nec_symbols[i].level1,
        rmt_nec_symbols[i].duration1);
  }
  printf("----------nec frame end----------\n");

  printf("nec parse: ");
  switch (symbol_num) {
    case 34:
      if (NecParseFrame(rmt_nec_symbols)) {
        printf("address=%#X, command=%#X\n", g_nec_code_address,
            g_nec_code_command);
      } else {
        printf("invalid normal frame\n");
      }
      break;
    case 2:
      if (NecParseFrameRepeat(rmt_nec_symbols)) {
        printf("address=%#X, command=%#X, repeat\n", g_nec_code_address,
            g_nec_code_command);
      } else {
        printf("invalid repeat frame\n");
      }
      break;
    default:
      printf("unknown nec frame (symbol count: %zu)\n", symbol_num);
      break;
  }
}

rmt_receive_config_t CreateReceiveConfig() {
  return {
      .signal_range_min_ns = 1250,
      .signal_range_max_ns = 12000000,
  };
}

bool StartReceive() {
  auto receive_config = CreateReceiveConfig();
  esp_err_t result =
      rmt_receive(g_rx_channel, g_raw_symbols, sizeof(g_raw_symbols),
          &receive_config);
  if (result != ESP_OK) {
    printf("rmt_receive failed (error code: %#X)\n", result);
    return false;
  }
  return true;
}

bool RmtInit() {
  printf("create rmt rx channel\n");
  rmt_rx_channel_config_t rx_channel_cfg = {
      .gpio_num =
          static_cast<gpio_num_t>(t_display_p4_air_example::gpio::infrared::kRx),
      .clk_src = RMT_CLK_SRC_DEFAULT,
      .resolution_hz =
          t_display_p4_air_example::device::infrared::kResolutionHz,
      .mem_block_symbols = 64,
  };
  esp_err_t result = rmt_new_rx_channel(&rx_channel_cfg, &g_rx_channel);
  if (result != ESP_OK) {
    printf("rmt_new_rx_channel failed (error code: %#X)\n", result);
    return false;
  }

  g_receive_queue = xQueueCreate(1, sizeof(rmt_rx_done_event_data_t));
  if (g_receive_queue == nullptr) {
    printf("create receive queue failed\n");
    return false;
  }

  rmt_rx_event_callbacks_t cbs = {
      .on_recv_done = RmtRxDoneCallback,
  };
  result = rmt_rx_register_event_callbacks(
      g_rx_channel, &cbs, g_receive_queue);
  if (result != ESP_OK) {
    printf("rmt_rx_register_event_callbacks failed (error code: %#X)\n",
        result);
    return false;
  }

  printf("create rmt tx channel\n");
  rmt_tx_channel_config_t tx_channel_cfg = {
      .gpio_num =
          static_cast<gpio_num_t>(t_display_p4_air_example::gpio::infrared::kTx),
      .clk_src = RMT_CLK_SRC_DEFAULT,
      .resolution_hz =
          t_display_p4_air_example::device::infrared::kResolutionHz,
      .mem_block_symbols = 64,
      .trans_queue_depth = 4,
  };
  result = rmt_new_tx_channel(&tx_channel_cfg, &g_tx_channel);
  if (result != ESP_OK) {
    printf("rmt_new_tx_channel failed (error code: %#X)\n", result);
    return false;
  }

  rmt_carrier_config_t carrier_cfg = {
      .frequency_hz = 38000,
      .duty_cycle = 0.33f,
  };
  result = rmt_apply_carrier(g_tx_channel, &carrier_cfg);
  if (result != ESP_OK) {
    printf("rmt_apply_carrier failed (error code: %#X)\n", result);
    return false;
  }

  ir_nec_encoder_config_t nec_encoder_cfg = {
      .resolution = t_display_p4_air_example::device::infrared::kResolutionHz,
  };
  result = rmt_new_ir_nec_encoder(&nec_encoder_cfg, &g_nec_encoder);
  if (result != ESP_OK) {
    printf("rmt_new_ir_nec_encoder failed (error code: %#X)\n", result);
    return false;
  }

  result = rmt_enable(g_tx_channel);
  if (result != ESP_OK) {
    printf("rmt tx enable failed (error code: %#X)\n", result);
    return false;
  }
  result = rmt_enable(g_rx_channel);
  if (result != ESP_OK) {
    printf("rmt rx enable failed (error code: %#X)\n", result);
    return false;
  }

  return StartReceive();
}

}  // namespace

extern "C" void app_main(void) {
  printf("Ciallo\n");

  t_display_p4_air_example::InitDriver();

  cpp_bus_driver::Tool tool;
  tool.SetGpioMode(t_display_p4_air_example::gpio::button::kEsp32p4Boot,
      cpp_bus_driver::Tool::GpioMode::kInput,
      cpp_bus_driver::Tool::GpioStatus::kPullup);

  if (!RmtInit()) {
    printf("RmtInit failed\n");
  }

  rmt_rx_done_event_data_t rx_data = {};
  while (1) {
    if (xQueueReceive(g_receive_queue, &rx_data, pdMS_TO_TICKS(10)) ==
        pdPASS) {
      ParseNecFrame(rx_data.received_symbols, rx_data.num_symbols);
      StartReceive();
    }

    if (!tool.GpioRead(t_display_p4_air_example::gpio::button::kEsp32p4Boot)) {
      vTaskDelay(pdMS_TO_TICKS(300));

      printf("rmt send start\n");
      const ir_nec_scan_code_t test_code = {
          .address = 0x0440,
          .command = 0x3003,
      };
      rmt_transmit_config_t transmit_config = {
          .loop_count = 0,
      };
      esp_err_t result = rmt_transmit(g_tx_channel, g_nec_encoder, &test_code,
          sizeof(test_code), &transmit_config);
      if (result != ESP_OK) {
        printf("rmt_transmit failed (error code: %#X)\n", result);
      }
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
