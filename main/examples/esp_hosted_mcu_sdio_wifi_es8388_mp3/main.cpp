/*
 * @Description: None
 * @Author: LILYGO_L
 * @Date: 2026-02-26 10:39:24
 * @LastEditTime: 2026-03-12 17:20:19
 * @License: GPL 3.0
 */
#include "lilygo_device_driver_library.h"
#include "cpp_bus_driver_library.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "esp_audio_dec_default.h"
#include "esp_audio_dec.h"

#define WIFI_SSID "xinyuandianzi_AX_Wi-Fi5"
#define WIFI_PASSWORD "AA15994823428"

#define DOWNLOAD_URL "https://s9.imslp.org/files/imglnks/usimg/b/ba/IMSLP479619-PMLP02312-Nocturne,_Op.9_No.2_in_E-flat_major_Aya_Higuchi.mp3"

#define AUDIO_SAMPLE_RATE 44100
#define AUDIO_CHANNELS 2
#define AUDIO_BITS_PER_SAMPLE i2s_data_bit_width_t::I2S_DATA_BIT_WIDTH_16BIT
#define AUDIO_MCLK_MULTIPLE i2s_mclk_multiple_t::I2S_MCLK_MULTIPLE_256

bool wifi_connected_flag = false;

esp_codec_dev_handle_t codec_dev = NULL;

auto ESP32P4 = std::make_unique<Cpp_Bus_Driver::Tool>();
auto Axp517_Iic_Bus = std::make_shared<Cpp_Bus_Driver::Hardware_Iic_1>(AXP517_SDA, AXP517_SCL, I2C_NUM_0);
auto Xl9555_Iic_Bus = std::make_shared<Cpp_Bus_Driver::Hardware_Iic_1>(XL9555_SDA, XL9555_SCL, I2C_NUM_1);
auto Es8388_Iic_Bus = std::make_shared<Cpp_Bus_Driver::Hardware_Iic_1>(ES8388_SDA, ES8388_SCL, I2C_NUM_1);

auto Axp517 = std::make_unique<Cpp_Bus_Driver::Axp517>(Axp517_Iic_Bus, AXP517_IIC_ADDRESS);
auto Xl9555 = std::make_unique<Cpp_Bus_Driver::Xl95x5>(Xl9555_Iic_Bus, XL9555_IIC_ADDRESS);

auto Es8388_Iis_Bus = std::make_shared<Cpp_Bus_Driver::Hardware_Iis>(ES8388_ADC_DATA, ES8388_DAC_DATA, ES8388_WS_LRCK, ES8388_BCLK, ES8388_MCLK,
                                                                     i2s_port_t::I2S_NUM_0, Cpp_Bus_Driver::Hardware_Iis::Data_Mode::INPUT_OUTPUT,
                                                                     Cpp_Bus_Driver::Hardware_Iis::Iis_Mode::STD, i2s_clock_src_t::I2S_CLK_SRC_APLL);

// 转换 synchsafe 32 位整数到 uint32_t
uint32_t synchsafe_to_uint32(const uint8_t *buf)
{
    return ((buf[0] & 0x7F) << 21) | ((buf[1] & 0x7F) << 14) | ((buf[2] & 0x7F) << 7) | (buf[3] & 0x7F);
}

// 转换大端 32 位整数到 uint32_t
uint32_t bigendian_to_uint32(const uint8_t *buf)
{
    return (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
}

size_t parse_and_skip_id3v2_from_http(esp_http_client_handle_t client)
{
    uint8_t header[10];
    int read_len = esp_http_client_read(client, (char *)header, 10);
    if (read_len != 10)
    {
        printf("failed to read id3v2 header, read %d bytes\n", read_len);
        return 0;
    }

    if (memcmp(header, "ID3", 3) != 0)
    {
        printf("no id3v2 tag found\n");
        return 0;
    }

    uint8_t major_version = header[3];
    uint8_t flags = header[5];
    uint32_t tag_size = synchsafe_to_uint32(header + 6);

    printf("id3v2.%d tag detected, flags: 0x%02x, size: %lu bytes\n", major_version, flags, tag_size);

    size_t skipped = 10;
    uint32_t bytes_processed_in_tag = 0;

    // 跳过指定字节数
    auto skip_bytes = [&](uint32_t bytes_to_skip) -> bool
    {
        auto skip_buf = std::make_unique<uint8_t[]>(4096);
        uint32_t remaining = bytes_to_skip;
        while (remaining > 0)
        {
            size_t chunk = std::min((size_t)4096, (size_t)remaining);
            int r = esp_http_client_read(client, (char *)skip_buf.get(), chunk);
            if (r <= 0)
            {
                printf("skip failed, read_len=%d, remaining=%lu\n", r, remaining);
                return false;
            }
            skipped += r;
            bytes_processed_in_tag += r;
            remaining -= r;
        }
        return true;
    };

    // 处理 extended header（如果存在）
    if (flags & 0x40)
    {
        uint8_t ext_size_buf[4];
        read_len = esp_http_client_read(client, (char *)ext_size_buf, 4);
        if (read_len != 4)
        {
            printf("failed to read extended header size\n");
            // 继续跳过剩余标签
        }
        else
        {
            skipped += 4;
            bytes_processed_in_tag += 4;

            uint32_t ext_size = (major_version == 4) ? synchsafe_to_uint32(ext_size_buf) : bigendian_to_uint32(ext_size_buf);
            printf("extended header size: %lu bytes\n", ext_size);

            // 跳过 extended header 剩余部分
            if (!skip_bytes(ext_size - 4))
            {
                // 跳过失败，直接进入最终剩余跳过
            }
        }
    }

    // 解析帧部分
    const int MAX_FRAMES_TO_PARSE = 20;
    int frames_parsed = 0;

    auto frame_buf = std::make_unique<uint8_t[]>(4096);

    bool continue_parsing = true;

    while (continue_parsing && bytes_processed_in_tag < tag_size && frames_parsed < MAX_FRAMES_TO_PARSE)
    {
        // 读取帧 ID (4 bytes)
        read_len = esp_http_client_read(client, (char *)frame_buf.get(), 4);
        if (read_len != 4)
        {
            continue_parsing = false;
            continue;
        }
        skipped += 4;
        bytes_processed_in_tag += 4;

        char frame_id[5] = {0};
        memcpy(frame_id, frame_buf.get(), 4);

        if (frame_id[0] == 0 && frame_id[1] == 0 && frame_id[2] == 0 && frame_id[3] == 0)
        {
            continue_parsing = false;
            continue;
        }

        // 读取帧大小 (4 bytes)
        read_len = esp_http_client_read(client, (char *)(frame_buf.get() + 4), 4);
        if (read_len != 4)
        {
            continue_parsing = false;
            continue;
        }
        skipped += 4;
        bytes_processed_in_tag += 4;

        uint32_t frame_size = (major_version == 4) ? synchsafe_to_uint32(frame_buf.get() + 4)
                                                   : bigendian_to_uint32(frame_buf.get() + 4);

        // 读取标志 (2 bytes)
        read_len = esp_http_client_read(client, (char *)(frame_buf.get() + 8), 2);
        if (read_len != 2)
        {
            continue_parsing = false;
            continue;
        }
        skipped += 2;
        bytes_processed_in_tag += 2;

        uint8_t frame_flags[2];
        memcpy(frame_flags, frame_buf.get() + 8, 2);

        printf("frame %s (size %lu, flags 0x%02x%02x): ", frame_id, (unsigned long)frame_size, frame_flags[0], frame_flags[1]);

        bool is_text_frame = (frame_id[0] == 'T' && strcmp(frame_id, "TXXX") != 0);

        if (is_text_frame && frame_size > 1 && frame_size < 1024)
        {
            size_t data_to_read = frame_size;
            size_t data_read = 0;

            while (data_read < data_to_read)
            {
                size_t chunk = std::min((size_t)4096, data_to_read - data_read);
                read_len = esp_http_client_read(client, (char *)(frame_buf.get() + data_read), chunk);
                if (read_len <= 0)
                {
                    printf("read text frame data failed\n");
                    continue_parsing = false;
                    break;
                }
                data_read += read_len;
            }

            if (read_len <= 0)
            {
                continue;
            }

            skipped += data_read;
            bytes_processed_in_tag += data_read;

            uint8_t encoding = frame_buf[0];
            const uint8_t *text_ptr = frame_buf.get() + 1;
            size_t text_len = frame_size - 1;

            size_t actual_len = text_len;
            if (encoding == 0 || encoding == 3)
            {
                for (size_t i = 0; i < text_len; ++i)
                {
                    if (text_ptr[i] == 0)
                    {
                        actual_len = i;
                        break;
                    }
                }
            }
            else if (encoding == 1 || encoding == 2)
            {
                for (size_t i = 0; i + 1 < text_len; i += 2)
                {
                    if (text_ptr[i] == 0 && text_ptr[i + 1] == 0)
                    {
                        actual_len = i;
                        break;
                    }
                }
            }

            const char *enc_name = (encoding == 0) ? "iso-8859-1" : (encoding == 3) ? "utf-8"
                                                                : (encoding == 1)   ? "utf-16 bom"
                                                                : (encoding == 2)   ? "utf-16be"
                                                                                    : "unknown";

            printf("text (%s): %.*s\n", enc_name, (int)actual_len, (const char *)text_ptr);
        }
        else
        {
            printf("skipped (binary/large frame)\n");

            if (!skip_bytes(frame_size))
            {
                continue_parsing = false;
            }
        }

        frames_parsed++;
    }

    // 无论解析是否成功，都尝试跳过标签剩余部分
    uint32_t remaining = tag_size - bytes_processed_in_tag;
    if (remaining > 0)
    {
        printf("skipping remaining %lu bytes...\n", remaining);
        skip_bytes(remaining);
    }

    printf("total skipped id3v2 bytes: %zu\n", skipped);
    return skipped;
}

void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        printf("got ip: " IPSTR "\n", IP2STR(&event->ip_info.ip));
        wifi_connected_flag = true;
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        printf("wifi disconnected, retrying...\n");
        wifi_connected_flag = false;
        esp_wifi_connect();
    }
}

void audio_init()
{
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
            // 不支持写入大量数据
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
                printf("es8388_iic_bus write fail\n");

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
            printf("es8388_iic_bus write_read fail\n");

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

    codec_dev = esp_codec_dev_new(&dev_cfg);

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
        printf("esp_codec_dev_open fail (error code: %#x)\n", assert);
    }

    printf("codec device opened successfully\n");

    // 设置DAC音量（0-100）
    assert = esp_codec_dev_set_out_vol(codec_dev, 100);
    if (assert != ESP_OK)
    {
        printf("esp_codec_dev_set_out_vol fail (error code: %#x)\n", assert);
    }

    // 设置ADC增益
    assert = esp_codec_dev_set_in_gain(codec_dev, 20);
    if (assert != ESP_OK)
    {
        printf("esp_codec_dev_set_in_gain fail (error code: %#x)\n", assert);
    }
}

void http_stream_play_task(void *pvParameters)
{
    printf("starting stream playback: %s\n", DOWNLOAD_URL);

    esp_http_client_config_t config = {
        .url = DOWNLOAD_URL,
        .timeout_ms = 15000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    if (esp_http_client_open(client, 0) != ESP_OK)
    {
        printf("esp_http_client_open fail\n");
        esp_http_client_cleanup(client);
        vTaskDelete(NULL);
        return;
    }

    int content_length = esp_http_client_fetch_headers(client);
    printf("stream start total size: %d bytes\n", content_length);

    // 跳过 ID3v2 标签
    size_t id3_skipped = parse_and_skip_id3v2_from_http(client);
    printf("skipped id3v2 bytes: %zu\n", id3_skipped);

    esp_audio_dec_register_default();
    esp_audio_dec_cfg_t dec_cfg = {.type = ESP_AUDIO_TYPE_MP3};
    esp_audio_dec_handle_t decoder = NULL;

    if (esp_audio_dec_open(&dec_cfg, &decoder) != ESP_AUDIO_ERR_OK)
    {
        printf("esp_audio_dec_open fail\n");
        esp_http_client_cleanup(client);
        vTaskDelete(NULL);
        return;
    }

    const size_t READ_BUF_SIZE = 4 * 1024;
    const size_t PCM_BUF_SIZE = 8 * 1024;
    auto read_buf = std::make_unique<uint8_t[]>(READ_BUF_SIZE);
    auto pcm_buf = std::make_unique<uint8_t[]>(PCM_BUF_SIZE);

    size_t remain_bytes = 0;
    int total_download = 0;
    uint32_t start_time = ESP32P4->get_system_time_ms();
    uint32_t last_print_time = start_time;

    while (1)
    {
        // 从 HTTP 读取数据
        int read_len = esp_http_client_read(client, (char *)(read_buf.get() + remain_bytes), READ_BUF_SIZE - remain_bytes);

        if (read_len < 0)
        {
            printf("http read error\n");
            break;
        }

        // 计算当前 buffer 中总共可用的字节
        size_t total_in_buf = remain_bytes + read_len;
        total_download += read_len;

        if (total_in_buf == 0)
        {
            break;
        }

        esp_audio_dec_in_raw_t in_raw = {.buffer = read_buf.get(), .len = total_in_buf};
        esp_audio_dec_out_frame_t out_frame = {.buffer = pcm_buf.get(), .len = PCM_BUF_SIZE};

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

            if (out_frame.decoded_size > 0)
            {
                esp_codec_dev_write(codec_dev, out_frame.buffer, out_frame.decoded_size);
            }

            // 如果 consumed 为 0，说明剩下的数据不够一帧了，退出内循环去拉新数据
            if (in_raw.consumed == 0)
            {
                break;
            }

            in_raw.buffer += in_raw.consumed;
            in_raw.len -= in_raw.consumed;
        }

        // 处理解码剩下的残余字节（不够一帧的部分）
        remain_bytes = in_raw.len;
        if (remain_bytes > 0)
        {
            memmove(read_buf.get(), in_raw.buffer, remain_bytes);
        }

        uint32_t now = ESP32P4->get_system_time_ms();
        if (now - last_print_time >= 1000)
        {
            float duration = (now - start_time) / 1000.0f;
            float speed = (total_download / 1024.0f) / duration;
            float progress = (content_length > 0) ? (total_download * 100.0f / content_length) : 0;
            printf("[music] progress: %.2f%% | speed: %.2f kb/s | total size: %.1f kb\n",
                   progress, speed, total_download / 1024.0f);
            last_print_time = now;
        }

        if (read_len == 0 && remain_bytes == total_in_buf)
        {
            break;
        }
    }

    printf("playback finished. cleaning up...\n");
    esp_audio_dec_close(decoder);
    esp_audio_dec_unregister_default();
    esp_http_client_cleanup(client);
    vTaskDelete(NULL);
}

void wifi_init_sta()
{
    // esp_err_t err = nvs_flash_init();
    // if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    // {
    //     ESP_ERROR_CHECK(nvs_flash_erase());
    //     err = nvs_flash_init();
    // }
    // ESP_ERROR_CHECK(err);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    wifi_config_t wifi_config =
        {
            .sta =
                {
                    .ssid = WIFI_SSID,
                    .password = WIFI_PASSWORD,
                },
        };

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

extern "C" void app_main(void)
{
    printf("Ciallo\n");

    Axp517->begin();

    Xl9555->begin();
    Xl9555->pin_mode(XL9555_3_3_POWER_EN, Cpp_Bus_Driver::Xl95x5::Mode::OUTPUT);
    Xl9555->pin_write(XL9555_3_3_POWER_EN, Cpp_Bus_Driver::Xl95x5::Value::HIGH);
    Xl9555->pin_mode(XL9555_NS4150_EN, Cpp_Bus_Driver::Xl95x5::Mode::OUTPUT);
    Xl9555->pin_write(XL9555_NS4150_EN, Cpp_Bus_Driver::Xl95x5::Value::HIGH);

    Xl9555->pin_mode(XL9555_ESP32C5_EN, Cpp_Bus_Driver::Xl95x5::Mode::OUTPUT);

    Xl9555->pin_write(XL9555_ESP32C5_EN, Cpp_Bus_Driver::Xl95x5::Value::HIGH);
    vTaskDelay(pdMS_TO_TICKS(100));
    Xl9555->pin_write(XL9555_ESP32C5_EN, Cpp_Bus_Driver::Xl95x5::Value::LOW);
    vTaskDelay(pdMS_TO_TICKS(100));
    Xl9555->pin_write(XL9555_ESP32C5_EN, Cpp_Bus_Driver::Xl95x5::Value::HIGH);
    vTaskDelay(pdMS_TO_TICKS(100));

    wifi_init_sta();

    while (!wifi_connected_flag)
    {
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    Es8388_Iic_Bus->set_bus_handle(Xl9555_Iic_Bus->get_bus_handle());
    Es8388_Iic_Bus->begin(-1, ES8388_IIC_ADDRESS);

    // 音频初始化
    audio_init();

    // 开启流式下载播放任务 (分配稍大的堆栈以防解码溢出)
    xTaskCreate(http_stream_play_task, "stream_task", 10 * 1024, NULL, 5, NULL);
}