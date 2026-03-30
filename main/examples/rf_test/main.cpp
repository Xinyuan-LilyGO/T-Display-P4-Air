/*
 * @Description: None
 * @Author: LILYGO_L
 * @Date: 2026-02-04 11:01:39
 * @LastEditTime: 2026-03-30 17:26:33
 * @License: GPL 3.0
 */
#include "lilygo_device_driver_library.h"
#include "cpp_bus_driver_library.h"
#include "radiolib_bridge_driver.h"
#include "st25r3916_driver.h"
#include "esp_wifi.h"

#include "lwip/sockets.h"

auto Axp517_Iic_Bus = std::make_shared<Cpp_Bus_Driver::Hardware_Iic_1>(AXP517_SDA, AXP517_SCL, I2C_NUM_0);
auto Xl9555_Iic_Bus = std::make_shared<Cpp_Bus_Driver::Hardware_Iic_1>(XL9555_SDA, XL9555_SCL, I2C_NUM_1);

auto LR1121_Spi_Bus = std::make_shared<Cpp_Bus_Driver::Hardware_Spi>(LR1121_MOSI, LR1121_SCLK, LR1121_MISO, SPI2_HOST, 0);

auto Nrf9151_Uart_Bus = std::make_shared<Cpp_Bus_Driver::Hardware_Uart>(NRF9151_UART_RX, NRF9151_UART_TX,
                                                                        NRF9151_UART_CTS, NRF9151_UART_RTS, UART_NUM_1);

auto Axp517 = std::make_unique<Cpp_Bus_Driver::Axp517>(Axp517_Iic_Bus, AXP517_IIC_ADDRESS);
auto Xl9555 = std::make_unique<Cpp_Bus_Driver::Xl95x5>(Xl9555_Iic_Bus, XL9555_IIC_ADDRESS);

RadioLibHal *Radiolib_Hal = new Radiolib_Cpp_Bus_Driver_Hal(LR1121_Spi_Bus, 10000000, LR1121_CS);
LR1121 Lr1121 = new Module(Radiolib_Hal, static_cast<uint32_t>(RADIOLIB_NC), LR1121_INT, LR1121_RST, LR1121_BUSY);

int16_t Lr1121_Transmit_Test(float freq, float bw, int8_t pwr)
{
    int16_t status;

    status = Lr1121.setFrequency(freq);
    if (status != RADIOLIB_ERR_NONE)
    {
        printf("lr1121 setFrequency fail at %.1f mhz (error code: %d)\n", freq, status);
        return status;
    }

    status = Lr1121.setBandwidth(bw, freq > 1000.0);
    if (status != RADIOLIB_ERR_NONE)
    {
        printf("lr1121 setBandwidth fail at %.1f mhz (error code: %d)\n", freq, status);
        return status;
    }

    status = Lr1121.setOutputPower(pwr);
    if (status != RADIOLIB_ERR_NONE)
    {
        printf("lr1121 setOutputPower fail at %.1f mhz (error code: %d)\n", freq, status);
        return status;
    }

    status = Lr1121.startTransmit("ciallo", 0);
    if (status != RADIOLIB_ERR_NONE)
    {
        printf("lr1121 startTransmit fail at %.1f mhz (error code: %#X)\n", freq, status);
    }
    else
    {
        printf("lr1121 startTransmit success at %.1f mhz\n", freq);
    }

    vTaskDelay(pdMS_TO_TICKS(100));
    Lr1121.standby();

    return status;
}

void Lr1121_Rf_Task(void *arg)
{
    printf("Lr1121_Rf_Task start\n");

    while (1)
    {
        Lr1121_Transmit_Test(1900.0, 406.25, 13);

        Lr1121_Transmit_Test(2200.0, 406.25, 13);

        Lr1121_Transmit_Test(2400.0, 406.25, 13);

        Lr1121_Transmit_Test(2500.0, 406.25, 13);

        Lr1121_Transmit_Test(433.0, 125.0, 22);

        Lr1121_Transmit_Test(868.0, 125.0, 22);

        Lr1121_Transmit_Test(915.0, 125.0, 22);
    }
}

void St25r3916_Rf_Task(void *arg)
{
    printf("St25r3916_Rf_Task start\n");

    while (1)
    {
        St25r3916_Loop();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void Esp32c5_Rf_Task(void *arg)
{
    printf("Esp32c5_Rf_Task (udp max power test) start\n");

    uint8_t cycle = 0;
    char dummy_data[1024];
    memset(dummy_data, 0xA5, sizeof(dummy_data)); // 填充测试数据

    while (1)
    {
        cycle = (cycle + 1) % 2;

        // 切换频段的硬逻辑
        esp_wifi_stop();
        vTaskDelay(pdMS_TO_TICKS(200));

        if (cycle == 0)
        {
            printf("mode: 2.4ghz (ch 6)\n");
            esp_wifi_set_band_mode(WIFI_BAND_MODE_2G_ONLY);
            esp_wifi_start();
            esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B);
            esp_wifi_set_channel(6, WIFI_SECOND_CHAN_NONE);
        }
        else
        {
            printf("mode: 5ghz (ch 36)\n");
            esp_wifi_set_band_mode(WIFI_BAND_MODE_5G_ONLY);
            esp_wifi_start();
            esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11A);
            esp_wifi_set_channel(36, WIFI_SECOND_CHAN_NONE);
        }

        // 设置最大发射功率
        esp_wifi_set_max_tx_power(80);

        // 创建 udp socket 进行压力测试
        int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
        if (sock < 0)
        {
            printf("socket fail\n");
            continue;
        }

        struct sockaddr_in dest_addr;
        dest_addr.sin_addr.s_addr = inet_addr("255.255.255.255");
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(12345);

        printf("esp32c5 starting tx stress test for 5s\n");

        uint32_t start_time = xTaskGetTickCount();
        while (xTaskGetTickCount() - start_time < pdMS_TO_TICKS(5000))
        {
            // 通过向广播地址发送 udp 包，触发物理层发射
            sendto(sock, dummy_data, sizeof(dummy_data), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));

            // 极短延时或不延时
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        close(sock);

        if (cycle == 0)
        {
            printf("esp32c5 2.4ghz test finish\n");
        }
        else
        {
            printf("esp32c5 5ghz test finish\n");
        }
    }
}

void Wifi_Init_Sta(void)
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

    // 强制使用 RAM 存储，并清空所有配置，防止它自动连接
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    wifi_config_t empty_conf = {}; // 空配置
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &empty_conf));

    // 设置国家码，确保 5G 信道可用
    const wifi_country_t country =
        {
            .cc = "CN",
            .schan = 1,
            .nchan = 13,
            .max_tx_power = 80,
            .policy = WIFI_COUNTRY_POLICY_AUTO,
        };
    esp_wifi_set_country(&country);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    ESP_ERROR_CHECK(esp_wifi_start());
}

std::string send_at_command(const std::string &cmd, int timeout_ms = 3000)
{
    std::string full_cmd = cmd + "\r\n";
    Nrf9151_Uart_Bus->write((uint8_t *)full_cmd.c_str(), full_cmd.length());

    // 清空输入缓存，避免残留数据干扰
    Nrf9151_Uart_Bus->clear_rx_buffer_data();

    std::string response;
    uint32_t start = xTaskGetTickCount();
    while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(timeout_ms))
    {
        size_t len = Nrf9151_Uart_Bus->get_rx_buffer_length();
        if (len > 0)
        {
            std::vector<uint8_t> buf(len + 1, 0);
            Nrf9151_Uart_Bus->read(buf.data(), len);
            response.append((char *)buf.data(), len);
            // 检测到 OK 或 ERROR 即认为指令执行完毕
            if (response.find("OK") != std::string::npos ||
                response.find("ERROR") != std::string::npos)
            {
                break;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    // 去除末尾的换行符，便于打印
    while (!response.empty() && (response.back() == '\r' || response.back() == '\n'))
    {
        response.pop_back();
    }
    return response;
}

bool check_sim_card()
{
    std::string resp = send_at_command("AT+CPIN?");
    return (resp.find("+CPIN: READY") != std::string::npos);
}

bool is_network_registered()
{
    std::string resp = send_at_command("AT+CEREG?");
    // 解析响应，例如 +CEREG: 2,2,... 或 +CEREG: 2,5,...
    size_t pos = resp.find("+CEREG:");
    if (pos != std::string::npos)
    {
        // 跳过 "+CEREG: " 后找到第一个逗号
        pos = resp.find(',', pos);
        if (pos != std::string::npos)
        {
            pos++; // 移动到状态字符
            char stat = resp[pos];
            return (stat == '2' || stat == '5');
        }
    }
    return false;
}

void Nrf9151_At_Task(void *arg)
{
    printf("Nrf9151_At_Task started\n");

    vTaskDelay(pdMS_TO_TICKS(1000));

    // 配置系统模式：LTE-M+NB-IoT+GNSS 支持，LTE 优先
    printf("configuring system mode...\n");
    std::string resp = send_at_command("AT%XSYSTEMMODE=1,1,1,1");
    printf("response: %s\n", resp.c_str());

    // 开启 modem 全功能
    printf("setting CFUN=1...\n");
    resp = send_at_command("AT+CFUN=1");
    printf("Response: %s\n", resp.c_str());
    vTaskDelay(pdMS_TO_TICKS(500));

    // 检查 SIM 卡
    printf("checking sim card...\n");
    if (check_sim_card() == false)
    {
        printf("sim card not ready\n");
    }
    else
    {
        printf("sim card ready\n");
    }

    // 启用网络注册 URC
    printf("enabling CEREG URC...\n");
    resp = send_at_command("AT+CEREG=2");
    printf("response: %s\n", resp.c_str());

    // 等待网络注册成功（最多等待 2 分钟，每 5 秒查询一次）
    printf("waiting for network registration...\n");
    bool registered = false;
    for (int i = 0; i < 24; i++)
    {
        // 24 * 5 = 120 秒
        if (is_network_registered())
        {
            registered = true;
            printf("network registered successfully\n");
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
    if (!registered)
    {
        printf("network registration timeout\n");
    }

    // 开启 GNSS 功能
    printf("enabling GNSS...\n");
    resp = send_at_command("AT#XGNSS=1,0,0,0");
    printf("response: %s\n", resp.c_str());
    vTaskDelay(pdMS_TO_TICKS(1000));

    // 循环查询实时时间
    while (1)
    {
        resp = send_at_command("AT+CCLK?");
        if (resp.find("+CCLK:") != std::string::npos)
        {
            printf("current time: %s\n", resp.c_str());
        }
        else
        {
            printf("time query failed: %s\n", resp.c_str());
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

extern "C" void app_main(void)
{
    printf("Ciallo\n");

    Axp517->begin();

    Xl9555->begin();

    Xl9555->pin_mode(XL9555_3_3_POWER_EN, Cpp_Bus_Driver::Xl95x5::Mode::OUTPUT);
    Xl9555->pin_mode(XL9555_LR1121_POWER_EN, Cpp_Bus_Driver::Xl95x5::Mode::OUTPUT);
    Xl9555->pin_mode(XL9555_ESP32C5_EN, Cpp_Bus_Driver::Xl95x5::Mode::OUTPUT);
    Xl9555->pin_mode(XL9555_NRF9151_EN, Cpp_Bus_Driver::Xl95x5::Mode::OUTPUT);

    Xl9555->pin_write(XL9555_3_3_POWER_EN, Cpp_Bus_Driver::Xl95x5::Value::HIGH);
    // vTaskDelay(pdMS_TO_TICKS(100));
    // Xl9555->pin_write(XL9555_3_3_POWER_EN, Cpp_Bus_Driver::Xl95x5::Value::LOW);
    // vTaskDelay(pdMS_TO_TICKS(100));
    // Xl9555->pin_write(XL9555_3_3_POWER_EN, Cpp_Bus_Driver::Xl95x5::Value::HIGH);
    // vTaskDelay(pdMS_TO_TICKS(100));

    Xl9555->pin_write(XL9555_LR1121_POWER_EN, Cpp_Bus_Driver::Xl95x5::Value::HIGH);
    Xl9555->pin_write(XL9555_ESP32C5_EN, Cpp_Bus_Driver::Xl95x5::Value::HIGH);
    Xl9555->pin_write(XL9555_NRF9151_EN, Cpp_Bus_Driver::Xl95x5::Value::HIGH);
    vTaskDelay(pdMS_TO_TICKS(100));
    Xl9555->pin_write(XL9555_LR1121_POWER_EN, Cpp_Bus_Driver::Xl95x5::Value::LOW);
    Xl9555->pin_write(XL9555_ESP32C5_EN, Cpp_Bus_Driver::Xl95x5::Value::LOW);
    Xl9555->pin_write(XL9555_NRF9151_EN, Cpp_Bus_Driver::Xl95x5::Value::LOW);
    vTaskDelay(pdMS_TO_TICKS(100));
    Xl9555->pin_write(XL9555_LR1121_POWER_EN, Cpp_Bus_Driver::Xl95x5::Value::HIGH);
    Xl9555->pin_write(XL9555_ESP32C5_EN, Cpp_Bus_Driver::Xl95x5::Value::HIGH);
    Xl9555->pin_write(XL9555_NRF9151_EN, Cpp_Bus_Driver::Xl95x5::Value::HIGH);
    vTaskDelay(pdMS_TO_TICKS(100));

    int16_t status = Lr1121.begin(2450.0, 406.25, 12, 7, RADIOLIB_LR11X0_LORA_SYNC_WORD_PRIVATE, 13, 8);
    if (status == RADIOLIB_ERR_NONE)
    {
        printf("lr1121 init success\n");
    }
    else
    {
        printf("lr1121 init fail (error code: %d)\n", status);
    }

    const uint32_t rfswitch_dio_pins[] =
        {
            RADIOLIB_LR11X0_DIO5, RADIOLIB_LR11X0_DIO6,
            RADIOLIB_NC, RADIOLIB_NC, RADIOLIB_NC};

    const Module::RfSwitchMode_t rfswitch_table[] =
        {
            // mode                  DIO5  DIO6
            {LR11x0::MODE_STBY, {0, 0}},
            {LR11x0::MODE_RX, {0, 1}},
            {LR11x0::MODE_TX, {0, 0}},
            {LR11x0::MODE_TX_HP, {1, 0}},
            {LR11x0::MODE_TX_HF, {0, 0}},
            {LR11x0::MODE_GNSS, {0, 0}},
            {LR11x0::MODE_WIFI, {0, 0}},
            END_OF_MODE_TABLE,
        };

    Lr1121.setRfSwitchTable(rfswitch_dio_pins, rfswitch_table);

    Wire._bus->set_bus_handle(Xl9555_Iic_Bus->get_bus_handle());
    St25r3916_Init();

    Wifi_Init_Sta();

    Nrf9151_Uart_Bus->begin();

    xTaskCreate(Lr1121_Rf_Task, "Lr1121_Rf_Task", 4 * 1024, NULL, 3, NULL);
    xTaskCreate(St25r3916_Rf_Task, "St25r3916_Rf_Task", 4 * 1024, NULL, 3, NULL);
    xTaskCreate(Esp32c5_Rf_Task, "Esp32c5_Rf_Task", 4 * 1024, NULL, 3, NULL);
    xTaskCreate(Nrf9151_At_Task, "Nrf9151_At_Task", 8 * 1024, NULL, 3, NULL);
}