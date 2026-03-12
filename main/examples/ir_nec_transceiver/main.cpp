/*
 * @Description: None
 * @Author: LILYGO_L
 * @Date: 2026-03-03 10:33:23
 * @LastEditTime: 2026-03-12 17:20:40
 * @License: GPL 3.0
 */
#include "lilygo_device_driver_library.h"
#include "cpp_bus_driver_library.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_rx.h"
#include "ir_nec_encoder.h"
#include "esp_err.h"

// NEC timing spec
#define NEC_LEADING_CODE_DURATION_0 9000
#define NEC_LEADING_CODE_DURATION_1 4500
#define NEC_PAYLOAD_ZERO_DURATION_0 560
#define NEC_PAYLOAD_ZERO_DURATION_1 560
#define NEC_PAYLOAD_ONE_DURATION_0 560
#define NEC_PAYLOAD_ONE_DURATION_1 1690
#define NEC_REPEAT_CODE_DURATION_0 9000
#define NEC_REPEAT_CODE_DURATION_1 2250

rmt_channel_handle_t rx_channel = NULL;
rmt_channel_handle_t tx_channel = NULL;

QueueHandle_t receive_queue;

// 准备接收缓冲
rmt_symbol_word_t raw_symbols[64];
rmt_encoder_handle_t nec_encoder = NULL;

// NEC 解码结果
uint16_t s_nec_code_address = 0;
uint16_t s_nec_code_command = 0;

auto Axp517_Iic_Bus = std::make_shared<Cpp_Bus_Driver::Hardware_Iic_1>(AXP517_SDA, AXP517_SCL, I2C_NUM_0);
auto Xl9555_Iic_Bus = std::make_shared<Cpp_Bus_Driver::Hardware_Iic_1>(XL9555_SDA, XL9555_SCL, I2C_NUM_1);

auto Axp517 = std::make_unique<Cpp_Bus_Driver::Axp517>(Axp517_Iic_Bus, AXP517_IIC_ADDRESS);
auto Xl9555 = std::make_unique<Cpp_Bus_Driver::Xl95x5>(Xl9555_Iic_Bus, XL9555_IIC_ADDRESS);
auto Esp32p4 = std::make_unique<Cpp_Bus_Driver::Tool>();

bool Rmt_Rx_Done_Callback(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t *edata, void *user_data)
{
    BaseType_t high_task_wakeup = pdFALSE;
    QueueHandle_t receive_queue = (QueueHandle_t)user_data;

    // send the received rmt symbols to the parser task
    xQueueSendFromISR(receive_queue, edata, &high_task_wakeup);

    return high_task_wakeup == pdTRUE;
}

// 检查 duration 是否在预期范围内（± margin）
static inline bool nec_check_in_range(uint32_t signal_duration, uint32_t spec_duration)
{
    return (signal_duration < (spec_duration + INFRARED_NEC_DECODE_MARGIN)) &&
           (signal_duration > (spec_duration - INFRARED_NEC_DECODE_MARGIN));
}

// 判断是否为逻辑 0
bool nec_parse_logic0(rmt_symbol_word_t *rmt_nec_symbols)
{
    return nec_check_in_range(rmt_nec_symbols->duration0, NEC_PAYLOAD_ZERO_DURATION_0) &&
           nec_check_in_range(rmt_nec_symbols->duration1, NEC_PAYLOAD_ZERO_DURATION_1);
}

// 判断是否为逻辑 1
bool nec_parse_logic1(rmt_symbol_word_t *rmt_nec_symbols)
{
    return nec_check_in_range(rmt_nec_symbols->duration0, NEC_PAYLOAD_ONE_DURATION_0) &&
           nec_check_in_range(rmt_nec_symbols->duration1, NEC_PAYLOAD_ONE_DURATION_1);
}

// 解析标准 NEC 帧（地址 + 命令）
bool nec_parse_frame(rmt_symbol_word_t *rmt_nec_symbols)
{
    rmt_symbol_word_t *cur = rmt_nec_symbols;
    uint16_t address = 0;
    uint16_t command = 0;

    // 检查 leading code
    bool valid_leading_code = nec_check_in_range(cur->duration0, NEC_LEADING_CODE_DURATION_0) &&
                              nec_check_in_range(cur->duration1, NEC_LEADING_CODE_DURATION_1);
    if (!valid_leading_code)
    {
        return false;
    }
    cur++;

    // 解析 16 bit address
    for (int i = 0; i < 16; i++)
    {
        if (nec_parse_logic1(cur))
        {
            address |= (1 << i);
        }
        else if (nec_parse_logic0(cur))
        {
            // address bit already 0, no need to clear
        }
        else
        {
            return false;
        }
        cur++;
    }

    // 解析 16 bit command
    for (int i = 0; i < 16; i++)
    {
        if (nec_parse_logic1(cur))
        {
            command |= (1 << i);
        }
        else if (nec_parse_logic0(cur))
        {
            // command bit already 0
        }
        else
        {
            return false;
        }
        cur++;
    }

    // 保存结果
    s_nec_code_address = address;
    s_nec_code_command = command;
    return true;
}

// 检查是否为 repeat 码
bool nec_parse_frame_repeat(rmt_symbol_word_t *rmt_nec_symbols)
{
    return nec_check_in_range(rmt_nec_symbols->duration0, NEC_REPEAT_CODE_DURATION_0) &&
           nec_check_in_range(rmt_nec_symbols->duration1, NEC_REPEAT_CODE_DURATION_1);
}

void parse_nec_frame(rmt_symbol_word_t *rmt_nec_symbols, size_t symbol_num)
{
    printf("----------nec frame start----------\n");

    // 打印原始符号
    for (size_t i = 0; i < symbol_num; i++)
    {
        printf("{%d:%d},{%d:%d}\n",
               rmt_nec_symbols[i].level0, rmt_nec_symbols[i].duration0,
               rmt_nec_symbols[i].level1, rmt_nec_symbols[i].duration1);
    }

    printf("----------nec frame end----------\n");

    printf("nec parse: ");

    switch (symbol_num)
    {
    case 34: // nec normal frame（典型 34 个符号：leading + 32 bits + ending）
        if (nec_parse_frame(rmt_nec_symbols))
        {
            printf("address=%#X, command=%#X\n", s_nec_code_address, s_nec_code_command);
        }
        else
        {
            printf("invalid normal frame\n");
        }
        break;

    case 2: // nec repeat frame
        if (nec_parse_frame_repeat(rmt_nec_symbols))
        {
            printf("address=%#X, command=%#X, repeat\n", s_nec_code_address, s_nec_code_command);
        }
        else
        {
            printf("invalid repeat frame\n");
        }
        break;

    default:
        printf("unknown nec frame (symbol count: %zu)\n", symbol_num);
        break;
    }
}

bool Rmt_Init(void)
{
    printf("create rmt rx channel\n");
    rmt_rx_channel_config_t rx_channel_cfg =
        {
            .gpio_num = static_cast<gpio_num_t>(INFRARED_RX),
            .clk_src = RMT_CLK_SRC_DEFAULT,
            .resolution_hz = INFRARED_RESOLUTION_HZ,
            .mem_block_symbols = 64,
        };
    ESP_ERROR_CHECK(rmt_new_rx_channel(&rx_channel_cfg, &rx_channel));

    printf("register rx done callback\n");
    receive_queue = xQueueCreate(1, sizeof(rmt_rx_done_event_data_t));
    if (receive_queue == NULL)
    {
        printf("create receive queue failed\n");
        return false;
    }

    rmt_rx_event_callbacks_t cbs = {
        .on_recv_done = Rmt_Rx_Done_Callback,
    };
    ESP_ERROR_CHECK(rmt_rx_register_event_callbacks(rx_channel, &cbs, receive_queue));

    printf("create rmt tx channel\n");
    rmt_tx_channel_config_t tx_channel_cfg =
        {
            .gpio_num = static_cast<gpio_num_t>(INFRARED_TX),
            .clk_src = RMT_CLK_SRC_DEFAULT,
            .resolution_hz = INFRARED_RESOLUTION_HZ,
            .mem_block_symbols = 64,
            .trans_queue_depth = 4,
        };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_channel_cfg, &tx_channel));

    printf("modulate carrier to tx channel\n");
    rmt_carrier_config_t carrier_cfg =
        {
            .frequency_hz = 38000,
            .duty_cycle = 0.33f,
        };
    ESP_ERROR_CHECK(rmt_apply_carrier(tx_channel, &carrier_cfg));

    printf("install ir nec encoder\n");
    ir_nec_encoder_config_t nec_encoder_cfg =
        {
            .resolution = INFRARED_RESOLUTION_HZ,
        };
    ESP_ERROR_CHECK(rmt_new_ir_nec_encoder(&nec_encoder_cfg, &nec_encoder));

    printf("enable rmt tx and rx channels\n");
    ESP_ERROR_CHECK(rmt_enable(tx_channel));
    ESP_ERROR_CHECK(rmt_enable(rx_channel));

    rmt_receive_config_t receive_config =
        {
            .signal_range_min_ns = 1250,
            .signal_range_max_ns = 12000000,
        };

    // 第一次启动接收
    ESP_ERROR_CHECK(rmt_receive(rx_channel, raw_symbols, sizeof(raw_symbols), &receive_config));

    return true;
}

extern "C" void app_main(void)
{
    printf("Ciallo\n");

    Esp32p4->pin_mode(ESP32P4_BOOT, Cpp_Bus_Driver::Tool::Pin_Mode::INPUT, Cpp_Bus_Driver::Tool::Pin_Status::PULLUP);

    Axp517->begin();
    Xl9555->begin();
    Xl9555->pin_mode(XL9555_3_3_POWER_EN, Cpp_Bus_Driver::Xl95x5::Mode::OUTPUT);
    Xl9555->pin_write(XL9555_3_3_POWER_EN, Cpp_Bus_Driver::Xl95x5::Value::HIGH);

    Rmt_Init();

    rmt_rx_done_event_data_t rx_data;

    while (1)
    {
        if (xQueueReceive(receive_queue, &rx_data, pdMS_TO_TICKS(10)) == pdPASS)
        {
            // 收到红外信号，解析并打印
            parse_nec_frame(rx_data.received_symbols, rx_data.num_symbols);

            rmt_receive_config_t receive_config =
                {
                    .signal_range_min_ns = 1250,
                    .signal_range_max_ns = 12000000,
                };

            // 重新启动接收
            ESP_ERROR_CHECK(rmt_receive(rx_channel, raw_symbols, sizeof(raw_symbols), &receive_config));
        }

        if (Esp32p4->pin_read(ESP32P4_BOOT) == 0)
        {
            vTaskDelay(pdMS_TO_TICKS(300));

            printf("rmt send start\n");

            const ir_nec_scan_code_t test_code =
                {
                    .address = 0x0440,
                    .command = 0x3003,
                };
            rmt_transmit_config_t transmit_config =
                {
                    .loop_count = 0, // no loop
                };
            ESP_ERROR_CHECK(rmt_transmit(tx_channel, nec_encoder, &test_code, sizeof(test_code), &transmit_config));
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
