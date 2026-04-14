/*
 * @Description: None
 * @Author: LILYGO_L
 * @Date: 2026-02-25 09:09:45
 * @LastEditTime: 2026-04-14 09:52:52
 * @License: GPL 3.0
 */
#include "lilygo_device_driver_library.h"
#include "cpp_bus_driver_library.h"

#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "esp_audio_dec_default.h"
#include "esp_audio_dec.h"

#define AUDIO_MCLK_MULTIPLE i2s_mclk_multiple_t::I2S_MCLK_MULTIPLE_256
#define AUDIO_CHANNELS 2
#define AUDIO_BITS_PER_SAMPLE i2s_data_bit_width_t::I2S_DATA_BIT_WIDTH_16BIT
// #define MP3_FILE_PATH SD_BASE_PATH "/Nocturne, Op.9 No.2 in E-flat major-Aya Higuchi (piano).mp3"
#define MP3_FILE_PATH SD_BASE_PATH "/music.mp3"

uint32_t Audio_Sample_Rate = 44100;

esp_codec_dev_handle_t codec_dev;

size_t Cycle_Time = 0;

auto Axp517_Iic_Bus = std::make_shared<Cpp_Bus_Driver::Hardware_Iic_1>(AXP517_SDA, AXP517_SCL, I2C_NUM_0);
auto Xl9535_Iic_Bus = std::make_shared<Cpp_Bus_Driver::Hardware_Iic_1>(XL9535_SDA, XL9535_SCL, I2C_NUM_1);
auto Es8388_Iic_Bus = std::make_shared<Cpp_Bus_Driver::Hardware_Iic_1>(ES8388_SDA, ES8388_SCL, I2C_NUM_1);

auto Es8388_Iis_Bus = std::make_shared<Cpp_Bus_Driver::Hardware_Iis>(ES8388_ADC_DATA, ES8388_DAC_DATA, ES8388_WS_LRCK, ES8388_BCLK, ES8388_MCLK,
                                                                     i2s_port_t::I2S_NUM_0, Cpp_Bus_Driver::Hardware_Iis::Data_Mode::INPUT_OUTPUT, Cpp_Bus_Driver::Hardware_Iis::Iis_Mode::STD,
                                                                     i2s_clock_src_t::I2S_CLK_SRC_DEFAULT);

auto Axp517 = std::make_unique<Cpp_Bus_Driver::Axp517>(Axp517_Iic_Bus, AXP517_IIC_ADDRESS);
auto Xl9535 = std::make_unique<Cpp_Bus_Driver::Xl95x5>(Xl9535_Iic_Bus, XL9535_IIC_ADDRESS);
auto Esp32p4 = std::make_unique<Cpp_Bus_Driver::Tool>();

size_t parse_and_print_id3v2(FILE *f)
{
    // 处理字节序转换
    auto synchsafe_to_uint32 = [](const uint8_t *buf) -> uint32_t
    {
        return ((buf[0] & 0x7F) << 21) | ((buf[1] & 0x7F) << 14) | ((buf[2] & 0x7F) << 7) | (buf[3] & 0x7F);
    };

    auto bigendian_to_uint32 = [](const uint8_t *buf) -> uint32_t
    {
        return (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
    };

    // 获取文件总大小用于计算时长
    long current_pos = ftell(f);
    fseek(f, 0, SEEK_END);
    long file_total_size = ftell(f);
    fseek(f, current_pos, SEEK_SET);

    uint8_t header[10];
    bool has_id3v2 = false;
    uint32_t id3v2_size = 0;
    uint8_t major_version = 0;

    // 读取 ID3v2 头部
    if (fread(header, 1, 10, f) == 10 && memcmp(header, "ID3", 3) == 0)
    {
        has_id3v2 = true;
        major_version = header[3];
        id3v2_size = synchsafe_to_uint32(header + 6);

        printf("Input from '%s':\n", MP3_FILE_PATH);
        printf("  Metadata:\n");

        size_t pos = 10;
        if (header[5] & 0x40) // 扩展头
        {
            uint8_t ext_buf[4];
            fread(ext_buf, 1, 4, f);
            uint32_t ext_size = (major_version == 4) ? synchsafe_to_uint32(ext_buf) : bigendian_to_uint32(ext_buf);
            fseek(f, ext_size - 4, SEEK_CUR);
            pos += ext_size;
        }

        // 解析 ID3v2 Frames
        while (pos < 10 + id3v2_size)
        {
            char frame_id[5] = {0};
            if (fread(frame_id, 1, 4, f) != 4 || frame_id[0] == 0)
                break;
            pos += 4;

            uint8_t sz_buf[4];
            fread(sz_buf, 1, 4, f);
            uint32_t frame_size = (major_version == 4) ? synchsafe_to_uint32(sz_buf) : bigendian_to_uint32(sz_buf);
            pos += 4;

            fseek(f, 2, SEEK_CUR); // 跳过 flags
            pos += 2;

            // 映射常见的 ID3 帧 ID 到对应标签
            const char *tag_key = nullptr;
            if (strcmp(frame_id, "TIT2") == 0)
                tag_key = "title";
            else if (strcmp(frame_id, "TALB") == 0)
                tag_key = "album";
            else if (strcmp(frame_id, "TPE1") == 0)
                tag_key = "artist";
            else if (strcmp(frame_id, "TPE2") == 0)
                tag_key = "album_artist";
            else if (strcmp(frame_id, "TCON") == 0)
                tag_key = "genre";
            else if (strcmp(frame_id, "TSSE") == 0)
                tag_key = "encoder";

            if (tag_key && frame_size > 1)
            {
                auto frame_data = std::make_unique<uint8_t[]>(frame_size + 1);
                fread(frame_data.get(), 1, frame_size, f);
                frame_data[frame_size] = 0;

                uint8_t encoding = frame_data[0];
                if (encoding == 0 || encoding == 3)
                { // ASCII / UTF-8
                    printf("    %-16s: %s\n", tag_key, (char *)frame_data.get() + 1);
                }
                else
                {
                    // 对于 UTF-16 等复杂编码，暂时避免乱码输出
                    printf("    %-16s: (UTF-16 text)\n", tag_key);
                }
            }
            else
            {
                fseek(f, frame_size, SEEK_CUR);
            }
            pos += frame_size;
        }
    }
    else
    {
        printf("Error metadata input from '%s':\n", MP3_FILE_PATH);
        rewind(f);
    }

    // 寻找真实音频流的起始位置
    long mp3_data_start = has_id3v2 ? (10 + id3v2_size) : 0;
    fseek(f, mp3_data_start, SEEK_SET);

    uint8_t frame_header[4];
    bool header_found = false;
    uint32_t sample_rate = 0, bitrate_kbps = 0;
    const char *channel_str = "stereo";

    for (int i = 0; i < 8192; ++i)
    {
        if (fread(frame_header, 1, 4, f) != 4)
            break;

        // 验证同步字 11位连续的1 (0xFFE0)
        if ((frame_header[0] == 0xFF) && ((frame_header[1] & 0xE0) == 0xE0))
        {
            uint8_t m_idx = (frame_header[1] >> 3) & 0x03;
            uint8_t l_idx = (frame_header[1] >> 1) & 0x03;
            uint8_t br_idx = (frame_header[2] >> 4) & 0x0F;
            uint8_t sr_idx = (frame_header[2] >> 2) & 0x03;
            uint8_t padding = (frame_header[2] >> 1) & 0x01;
            uint8_t ch_mode = (frame_header[3] >> 6) & 0x03;

            // 过滤无效帧或非 Layer III 帧
            if (l_idx != 1 || br_idx == 0 || br_idx == 15 || sr_idx == 3)
            {
                fseek(f, -3, SEEK_CUR);
                continue;
            }

            static const uint32_t sr_tbl[4][4] = {
                {11025, 12000, 8000, 0}, {0, 0, 0, 0}, {22050, 24000, 16000, 0}, {44100, 48000, 32000, 0}};
            sample_rate = sr_tbl[m_idx][sr_idx];

            static const uint16_t br_tbl[2][16] = {
                {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0},
                {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0}};
            bitrate_kbps = br_tbl[(m_idx == 3) ? 1 : 0][br_idx];

            if (sample_rate > 0 && bitrate_kbps > 0)
            {
                // 计算当前帧的长度（字节）
                uint32_t frame_len = (m_idx == 3) ? (144000 * bitrate_kbps / sample_rate + padding) : (72000 * bitrate_kbps / sample_rate + padding);

                // 检查并跳过 Xing / Info 帧
                long sync_pos = ftell(f) - 4;
                uint8_t peek_buf[40] = {0};
                fread(peek_buf, 1, 40, f);

                bool is_xing_frame = false;
                for (int p = 0; p < 36; ++p)
                {
                    if (memcmp(peek_buf + p, "Xing", 4) == 0 || memcmp(peek_buf + p, "Info", 4) == 0)
                    {
                        is_xing_frame = true;
                        break;
                    }
                }

                if (is_xing_frame)
                {
                    // 如果这是 Xing/Info 帧，直接跳过整个帧大小，去读取下一帧（真实音频帧）
                    fseek(f, sync_pos + frame_len, SEEK_SET);
                    continue;
                }

                // 还原指针到帧头后面，说明找到了真实音频帧
                fseek(f, sync_pos + 4, SEEK_SET);

                if (ch_mode == 3)
                    channel_str = "mono";
                else if (ch_mode == 1 || ch_mode == 2)
                    channel_str = "stereo";

                header_found = true;
                break;
            }
        }
        fseek(f, -3, SEEK_CUR); // 如果不是同步字，逐字节往后挪动搜索
    }

    // 计算时长并输出最终信息
    if (header_found)
    {
        // (文件总大小 - ID3标签大小) * 8 / 比特率 = 时长（秒）
        double duration = ((file_total_size - mp3_data_start) * 8.0) / (bitrate_kbps * 1000.0);
        int h = (int)duration / 3600;
        int m = ((int)duration % 3600) / 60;
        double s = duration - (h * 3600) - (m * 60);

        printf("  Duration: %02d:%02d:%05.2f, start: 0.000000, bitrate: %lu kb/s\n", h, m, s, bitrate_kbps);
        printf("  Stream #0:0: Audio: mp3, %lu Hz, %s, fltp, %lu kb/s\n", sample_rate, channel_str, bitrate_kbps);
        printf("    Metadata:\n");
        printf("      encoder         : Lavc60.3\n");

        if (sample_rate != Audio_Sample_Rate)
        {
            printf("match sample rate %ldhz -> %ldhz\n", Audio_Sample_Rate, sample_rate);

            esp_codec_dev_close(codec_dev);

            esp_codec_dev_sample_info_t fs =
                {
                    .bits_per_sample = AUDIO_BITS_PER_SAMPLE,
                    .channel = AUDIO_CHANNELS,
                    .channel_mask = 0,
                    .sample_rate = sample_rate,
                    .mclk_multiple = AUDIO_MCLK_MULTIPLE,
                };

            // 打开音频设备
            esp_err_t assert = esp_codec_dev_open(codec_dev, &fs);
            if (assert != ESP_OK)
            {
                printf("esp_codec_dev_open fail (error code: %#X)\n", assert);
            }
            else
            {
                Audio_Sample_Rate = sample_rate;
            }
        }
    }
    else
    {
        printf("The ID3v2 header does not exist\n");
    }

    // 恢复指针到有效数据的开始
    fseek(f, mp3_data_start, SEEK_SET);
    return (size_t)mp3_data_start;
}

void parse_and_print_id3v1(FILE *f)
{
    long original_pos = ftell(f);

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    if (file_size < 128)
    {
        fseek(f, original_pos, SEEK_SET);
        return;
    }

    fseek(f, -128, SEEK_END);
    uint8_t tag[128];
    if (fread(tag, 1, 128, f) != 128)
    {
        fseek(f, original_pos, SEEK_SET);
        return;
    }

    if (memcmp(tag, "TAG", 3) != 0)
    {
        printf("No ID3v1 tag found\n");
        fseek(f, original_pos, SEEK_SET);
        return;
    }

    printf("ID3v1 tag detected:\n");
    printf("Title: %.30s\n", reinterpret_cast<char *>(tag + 3));
    printf("Artist: %.30s\n", reinterpret_cast<char *>(tag + 33));
    printf("Album: %.30s\n", reinterpret_cast<char *>(tag + 63));
    printf("Year: %.4s\n", reinterpret_cast<char *>(tag + 93));

    if (tag[125] == 0 && tag[126] != 0)
    { // ID3v1.1
        printf("Comment: %.28s\n", reinterpret_cast<char *>(tag + 97));
        printf("Track: %u\n", tag[126]);
    }
    else
    {
        printf("Comment: %.30s\n", reinterpret_cast<char *>(tag + 97));
    }
    printf("Genre: %u\n", tag[127]);

    fseek(f, original_pos, SEEK_SET);
}

extern "C" void app_main(void)
{
    printf("Ciallo\n");

    Esp32p4->pin_mode(ESP32P4_BOOT, Cpp_Bus_Driver::Tool::Pin_Mode::INPUT, Cpp_Bus_Driver::Tool::Pin_Status::PULLUP);

    Axp517->begin();

    Xl9535->begin();

    Xl9535->pin_mode(XL9535_3_3_POWER_EN, Cpp_Bus_Driver::Xl95x5::Mode::OUTPUT);
    Xl9535->pin_write(XL9535_3_3_POWER_EN, Cpp_Bus_Driver::Xl95x5::Value::HIGH);

    Xl9535->pin_mode(XL9535_SD_POWER_EN, Cpp_Bus_Driver::Xl95x5::Mode::OUTPUT);
    Xl9535->pin_write(XL9535_SD_POWER_EN, Cpp_Bus_Driver::Xl95x5::Value::HIGH);

    Lilygo_Device_Driver::Init_Ldo_Channel_Power(4, 3300);

    vTaskDelay(pdMS_TO_TICKS(100));

    if (Lilygo_Device_Driver::Sdmmc_Init(SD_BASE_PATH, SDMMC_FREQ_52M) == false)
    {
        printf("Sdmmc_Init fail\n");
    }

    Xl9535->pin_mode(XL9535_NS4150_EN, Cpp_Bus_Driver::Xl95x5::Mode::OUTPUT);
    Xl9535->pin_write(XL9535_NS4150_EN, Cpp_Bus_Driver::Xl95x5::Value::HIGH);

    Es8388_Iic_Bus->set_bus_handle(Xl9535_Iic_Bus->get_bus_handle());
    Es8388_Iic_Bus->begin(-1, ES8388_IIC_ADDRESS);

    Es8388_Iis_Bus->begin(AUDIO_MCLK_MULTIPLE, Audio_Sample_Rate, AUDIO_BITS_PER_SAMPLE);

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
            .clk_src = i2s_clock_src_t::I2S_CLK_SRC_DEFAULT,
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

    codec_dev = esp_codec_dev_new(&dev_cfg);

    // 设置音频采样参数
    esp_codec_dev_sample_info_t fs =
        {
            .bits_per_sample = AUDIO_BITS_PER_SAMPLE,
            .channel = AUDIO_CHANNELS,
            .channel_mask = 0,
            .sample_rate = Audio_Sample_Rate,
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
    assert = esp_codec_dev_set_out_vol(codec_dev, 60);
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
                    // Parse and print ID3v2, get offset to MP3 data
                    size_t mp3_offset = parse_and_print_id3v2(f_in);

                    // Parse and print ID3v1 (restores file position)
                    parse_and_print_id3v1(f_in);

                    // Seek to the start of actual MP3 data
                    fseek(f_in, mp3_offset, SEEK_SET);

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
                            memmove(read_buf.get(), in_raw.buffer, remain_bytes);
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