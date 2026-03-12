/*
 * @Description: None
 * @Author: LILYGO_L
 * @Date: 2026-03-03 13:51:54
 * @LastEditTime: 2026-03-12 17:19:56
 * @License: GPL 3.0
 */
#include "lilygo_device_driver_library.h"
#include "cpp_bus_driver_library.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "New Notification 010_c2_b16_s44100.h"

#define AUDIO_MCLK_MULTIPLE i2s_mclk_multiple_t::I2S_MCLK_MULTIPLE_256
#define AUDIO_SAMPLE_RATE 44100
#define AUDIO_CHANNELS 2
#define AUDIO_BITS_PER_SAMPLE i2s_data_bit_width_t::I2S_DATA_BIT_WIDTH_16BIT

size_t Cycle_Time = 0;

auto Axp517_Iic_Bus = std::make_shared<Cpp_Bus_Driver::Hardware_Iic_1>(AXP517_SDA, AXP517_SCL, I2C_NUM_0);
auto Xl9555_Iic_Bus = std::make_shared<Cpp_Bus_Driver::Hardware_Iic_1>(XL9555_SDA, XL9555_SCL, I2C_NUM_1);
auto Es8388_Iic_Bus = std::make_shared<Cpp_Bus_Driver::Hardware_Iic_1>(ES8388_SDA, ES8388_SCL, I2C_NUM_1);

auto Es8388_Iis_Bus = std::make_shared<Cpp_Bus_Driver::Hardware_Iis>(ES8388_ADC_DATA, ES8388_DAC_DATA, ES8388_WS_LRCK, ES8388_BCLK, ES8388_MCLK,
                                                                     i2s_port_t::I2S_NUM_0, Cpp_Bus_Driver::Hardware_Iis::Data_Mode::INPUT_OUTPUT, Cpp_Bus_Driver::Hardware_Iis::Iis_Mode::STD,
                                                                     i2s_clock_src_t::I2S_CLK_SRC_APLL);

auto Axp517 = std::make_unique<Cpp_Bus_Driver::Axp517>(Axp517_Iic_Bus, AXP517_IIC_ADDRESS);
auto Xl9555 = std::make_unique<Cpp_Bus_Driver::Xl95x5>(Xl9555_Iic_Bus, XL9555_IIC_ADDRESS);
auto Esp32p4 = std::make_unique<Cpp_Bus_Driver::Tool>();

extern "C" void app_main(void)
{
    printf("Ciallo\n");

    Esp32p4->pin_mode(ESP32P4_BOOT, Cpp_Bus_Driver::Tool::Pin_Mode::INPUT, Cpp_Bus_Driver::Tool::Pin_Status::PULLUP);

    Axp517->begin();

    Xl9555->begin();

    Xl9555->pin_mode(XL9555_3_3_POWER_EN, Cpp_Bus_Driver::Xl95x5::Mode::OUTPUT);

    Xl9555->pin_write(XL9555_3_3_POWER_EN, Cpp_Bus_Driver::Xl95x5::Value::HIGH);

    Xl9555->pin_mode(XL9555_NS4150_EN, Cpp_Bus_Driver::Xl95x5::Mode::OUTPUT);

    Xl9555->pin_write(XL9555_NS4150_EN, Cpp_Bus_Driver::Xl95x5::Value::HIGH);

    Es8388_Iic_Bus->set_bus_handle(Xl9555_Iic_Bus->get_bus_handle());
    Es8388_Iic_Bus->begin(-1, ES8388_IIC_ADDRESS);

    Es8388_Iis_Bus->begin(AUDIO_MCLK_MULTIPLE, AUDIO_SAMPLE_RATE, AUDIO_BITS_PER_SAMPLE);

    audio_codec_ctrl_if_t ctrl_if;

    ctrl_if.open = [](const audio_codec_ctrl_if_t *ctrl, void *cfg, int cfg_size) -> int
    {
        return 0;
    };

    ctrl_if.is_open = [](const audio_codec_ctrl_if_t *ctrl) -> bool
    {
        return true;
    };

    ctrl_if.close = [](const audio_codec_ctrl_if_t *ctrl) -> int
    {
        return 0;
    };

    ctrl_if.write_reg = [](const audio_codec_ctrl_if_t *ctrl, int reg, int reg_len, void *data, int data_len) -> int
    {
        int len = reg_len + data_len;
        if (len <= 4)
        {
            // Not support write huge data
            uint8_t write_data[4] = {0};
            int i = 0;
            if (reg_len > 1)
            {
                write_data[i++] = reg >> 8;
                write_data[i++] = reg & 0xff;
            }
            else
            {
                write_data[i++] = reg & 0xff;
            }
            uint8_t *w = (uint8_t *)data;
            while (i < len)
            {
                write_data[i++] = *(w++);
            }

            if (Es8388_Iic_Bus->write(write_data, len) == false)
            {
                printf("Es8388_Iic_Bus write fail\n");

                return ESP_CODEC_DEV_READ_FAIL;
            }
        }
        else
        {
            printf("not support write huge data\n");

            return ESP_CODEC_DEV_WRITE_FAIL;
        }

        return ESP_CODEC_DEV_OK;
    };

    ctrl_if.read_reg = [](const audio_codec_ctrl_if_t *ctrl, int reg, int reg_len, void *data, int data_len) -> int
    {
        uint8_t addr_data[2] = {0};
        if (reg_len > 1)
        {
            addr_data[0] = reg >> 8;
            addr_data[1] = reg & 0xff;
        }
        else
        {
            addr_data[0] = reg & 0xff;
        }

        if (Es8388_Iic_Bus->write_read(addr_data, reg_len, static_cast<uint8_t *>(data), data_len) == false)
        {
            printf("Es8388_Iic_Bus write_read fail\n");

            return ESP_CODEC_DEV_READ_FAIL;
        }

        return ESP_CODEC_DEV_OK;
    };

    audio_codec_i2s_cfg_t i2s_cfg =
        {
            .port = Es8388_Iis_Bus->_port,
            .rx_handle = Es8388_Iis_Bus->_chan_rx_handle,
            .tx_handle = Es8388_Iis_Bus->_chan_tx_handle,
            .clk_src = i2s_clock_src_t::I2S_CLK_SRC_APLL,
        };
    const audio_codec_data_if_t *data_if = audio_codec_new_i2s_data(&i2s_cfg);

    const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();

    es8388_codec_cfg_t es8388_cfg =
        {
            .ctrl_if = &ctrl_if,
            .gpio_if = gpio_if,
            .codec_mode = esp_codec_dec_work_mode_t::ESP_CODEC_DEV_WORK_MODE_BOTH, // 同时支持播放和录音
            .master_mode = 0,
            .pa_pin = -1,         // 功放使能引脚
            .pa_reverted = false, // 高电平使能功放
            .hw_gain = {
                .pa_voltage = 3.3,        // 功放电压
                .codec_dac_voltage = 3.3, // dac电压
                .pa_gain = 0.0},
        };

    const audio_codec_if_t *codec_if = es8388_codec_new(&es8388_cfg);

    esp_codec_dev_cfg_t dev_cfg =
        {
            .dev_type = esp_codec_dev_type_t::ESP_CODEC_DEV_TYPE_IN_OUT, // 输入输出设备
            .codec_if = codec_if,
            .data_if = data_if,
        };

    esp_codec_dev_handle_t codec_dev = esp_codec_dev_new(&dev_cfg);

    // 设置音频采样参数
    esp_codec_dev_sample_info_t fs =
        {
            .bits_per_sample = AUDIO_BITS_PER_SAMPLE,
            .channel = AUDIO_CHANNELS,
            .channel_mask = 0,
            .sample_rate = AUDIO_SAMPLE_RATE,
            .mclk_multiple = AUDIO_MCLK_MULTIPLE,
        };

    // 打开音频设备
    esp_err_t assert = esp_codec_dev_open(codec_dev, &fs);
    if (assert != ESP_OK)
    {
        printf("esp_codec_dev_open fail (error code: %#X)\n", assert);
    }

    printf("codec device opened successfully\n");

    // 设置DAC音量（0-100）
    assert = esp_codec_dev_set_out_vol(codec_dev, 100);
    if (assert != ESP_OK)
    {
        printf("esp_codec_dev_set_out_vol fail (error code: %#X)\n", assert);
    }

    // 设置ADC增益
    assert = esp_codec_dev_set_in_gain(codec_dev, 20);
    if (assert != ESP_OK)
    {
        printf("esp_codec_dev_set_in_gain fail (error code: %#X)\n", assert);
    }

    // 打印所有寄存器
    // uint8_t buffer = 0;
    // for (size_t i = 0; i < 53; i++)
    // {
    //     Es8388_Iic_Bus->Bus_Iic_Guide::read(static_cast<uint8_t>(i), &buffer);
    //     printf("es8388 register[%d]: %#X\n", i, buffer);
    // }

    while (1)
    {
        if (Esp32p4->pin_read(ESP32P4_BOOT) == 0)
        {
            vTaskDelay(pdMS_TO_TICKS(300));

            printf("play_audio start\n");

            esp_codec_dev_write(codec_dev, (uint8_t *)c2_b16_s44100, sizeof(c2_b16_s44100));

            vTaskDelay(pdMS_TO_TICKS(100));

            printf("play_audio finish\n");
        }

        // if (Esp32p4->get_system_time_ms() > Cycle_Time)
        // {
        //     size_t assert = esp_codec_dev_read(codec_dev, buffer.get(), 100 * sizeof(uint16_t));
        //     if (assert == ESP_CODEC_DEV_OK)
        //     {
        //         printf("esp_codec_dev_read[0]: %d\n", buffer[0]);
        //     }
        //     else
        //     {
        //         printf("esp_codec_dev_read fail (error code: %#X)\n", assert);
        //     }

        //     Cycle_Time = Esp32p4->get_system_time_ms() + 100;
        // }

        auto buffer = std::make_unique<int16_t[]>(1024);
        esp_codec_dev_read(codec_dev, buffer.get(), 1024 * sizeof(int16_t));
        esp_codec_dev_write(codec_dev, buffer.get(), 1024 * sizeof(int16_t));

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}