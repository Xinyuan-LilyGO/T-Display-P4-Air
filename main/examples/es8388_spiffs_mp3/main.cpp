/*
 * @Description: None
 * @Author: LILYGO_L
 * @Date: 2026-02-25 09:09:45
 * @LastEditTime: 2026-02-26 09:13:24
 * @License: GPL 3.0
 */
#include "sdkconfig.h"
#include "lilygo_device_driver_library.h"
#include "cpp_bus_driver_library.h"
#include "esp_spiffs.h"

#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "esp_audio_dec_default.h"
#include "esp_audio_dec.h"

#define AUDIO_MCLK_MULTIPLE i2s_mclk_multiple_t::I2S_MCLK_MULTIPLE_256
#define AUDIO_SAMPLE_RATE 44100
#define AUDIO_CHANNELS 2
#define AUDIO_BITS_PER_SAMPLE i2s_data_bit_width_t::I2S_DATA_BIT_WIDTH_16BIT
#define MP3_FILE_PATH SPIFFS_BASE_PATH "/Nocturne, Op.9 No.2 in E-flat major-Aya Higuchi (piano).mp3"
// #define MP3_FILE_PATH SPIFFS_BASE_PATH "/music.mp3"

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

    printf("initializing spiffs\n");
    esp_vfs_spiffs_conf_t conf;
    Lilygo_Device_Driver::Spiffs_Init(SPIFFS_BASE_PATH, conf);

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

    while (1)
    {
        // 监测按键，按下后触发 MP3 解码播放
        if (Esp32p4->pin_read(ESP32P4_BOOT) == 0)
        {
            vTaskDelay(pdMS_TO_TICKS(300));
            printf("play_mp3 start\n");

            esp_audio_dec_register_default();
            esp_audio_dec_cfg_t dec_cfg;
            dec_cfg.type = ESP_AUDIO_TYPE_MP3;

            esp_audio_dec_handle_t decoder = NULL;
            if (esp_audio_dec_open(&dec_cfg, &decoder) != ESP_AUDIO_ERR_OK)
            {
                printf("esp_audio_dec_open fail\n");
            }
            else
            {
                FILE *f_in = fopen(MP3_FILE_PATH, "rb");
                if (f_in == nullptr)
                {
                    printf("fopen mp3 fail: %s\n", MP3_FILE_PATH);
                }
                else
                {
                    auto read_buf = std::make_unique<uint8_t[]>(4 * 1024);
                    auto pcm_buf = std::make_unique<uint8_t[]>(8 * 1024);
                    size_t remain_bytes = 0;

                    // 流式读取与解码循环
                    while (1)
                    {
                        size_t read_bytes = fread(read_buf.get() + remain_bytes, 1, (4 * 1024) - remain_bytes, f_in);
                        size_t total_bytes = remain_bytes + read_bytes;

                        if (total_bytes == 0)
                        {
                            break; // 读取完毕
                        }

                        esp_audio_dec_in_raw_t in_raw = {.buffer = read_buf.get(), .len = total_bytes};
                        esp_audio_dec_out_frame_t out_frame = {.buffer = pcm_buf.get(), .len = 8 * 1024};

                        while (in_raw.len > 0)
                        {
                            esp_audio_err_t assert = esp_audio_dec_process(decoder, &in_raw, &out_frame);
                            if (assert != ESP_AUDIO_ERR_OK)
                            {
                                if (assert == ESP_AUDIO_ERR_FAIL)
                                {
                                    printf("esp_audio_dec_process fail\n");
                                }

                                break;
                            }

                            // 如果成功解码出数据，将其推入 ES8388 播放
                            if (out_frame.decoded_size > 0)
                            {
                                esp_codec_dev_write(codec_dev, out_frame.buffer, out_frame.decoded_size);
                            }

                            // 消耗量为 0，说明当前数据不足以解码一帧，需跳出内循环读取更多数据
                            if (in_raw.consumed == 0)
                            {
                                break;
                            }

                            in_raw.buffer += in_raw.consumed;
                            in_raw.len -= in_raw.consumed;
                        }

                        // 将剩余未处理的数据移到 buffer 头部
                        remain_bytes = in_raw.len;
                        if (remain_bytes > 0)
                        {
                            memcpy(read_buf.get(), in_raw.buffer, remain_bytes);
                        }

                        if (read_bytes == 0 && remain_bytes == total_bytes)
                        {
                            break; // 遇到 EOF 且无法再消耗数据
                        }
                    }

                    fclose(f_in);
                }
                esp_audio_dec_close(decoder);
            }
            esp_audio_dec_unregister_default();

            printf("play_mp3 finish\n");
        }

        // ES8388 的回环读取
        auto buffer = std::make_unique<int16_t[]>(1024);
        esp_codec_dev_read(codec_dev, buffer.get(), 1024 * sizeof(int16_t));
        esp_codec_dev_write(codec_dev, buffer.get(), 1024 * sizeof(int16_t));

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}