/*
 * @Description: None
 * @Author: LILYGO_L
 * @Date: 2026-02-09 16:54:46
 * @LastEditTime: 2026-02-09 17:17:05
 * @License: GPL 3.0
 */

#include "sdkconfig.h"
#include "lilygo_device_driver_library.h"
#include "cpp_bus_driver_library.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "esp_http_client.h"
#include "esp_crt_bundle.h"

#define WIFI_SSID "xinyuandianzi_AX_Wi-Fi5"
#define WIFI_PASSWORD "AA15994823428"
// #define DOWNLOAD_URL "https://cd001.www.duba.net/duba/install/packages/ever/kinsthomeui_150_15.exe"
#define DOWNLOAD_URL "https://dldir1.qq.com/qqfile/qq/PCQQ9.7.17/QQ9.7.17.29225.exe"

bool wifi_connected_flag = false;

auto ESP32 = std::make_unique<Cpp_Bus_Driver::Tool>();

auto Axp517_Iic_Bus = std::make_shared<Cpp_Bus_Driver::Hardware_Iic_1>(AXP517_SDA, AXP517_SCL, I2C_NUM_0);
auto Xl9555_Iic_Bus = std::make_shared<Cpp_Bus_Driver::Hardware_Iic_1>(XL9555_SDA, XL9555_SCL, I2C_NUM_1);

auto Axp517 = std::make_unique<Cpp_Bus_Driver::Axp517>(Axp517_Iic_Bus, AXP517_IIC_ADDRESS);
auto Xl9555 = std::make_unique<Cpp_Bus_Driver::Xl95x5>(Xl9555_Iic_Bus, XL9555_IIC_ADDRESS);

void download_task(void *pvParameters)
{
    printf("Starting download from: %s\n", DOWNLOAD_URL);

    esp_http_client_config_t config = {
        .url = DOWNLOAD_URL,
        .timeout_ms = 10000,
        .crt_bundle_attach = esp_crt_bundle_attach, // 必须包含这个来处理 HTTPS 证书
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err;

    if ((err = esp_http_client_open(client, 0)) != ESP_OK)
    {
        printf("fail to open http connection: %s\n", esp_err_to_name(err));
        vTaskDelete(NULL);
        return;
    }

    // 获取文件总长度
    int content_length = esp_http_client_fetch_headers(client);
    int total_read_len = 0;
    int read_len = 0;
    char *buffer = (char *)malloc(4096);

    uint32_t start_time = ESP32->get_system_time_ms();
    uint32_t last_calc_time = start_time;
    int last_read_len = 0;

    printf("download started. total size: %d bytes\n", content_length);
    printf("--------------------------------------------------\n");

    while (total_read_len < content_length || content_length == -1)
    {
        read_len = esp_http_client_read(client, buffer, 4096);
        if (read_len <= 0)
        {
            printf("download fail\n");
            break;
        }

        total_read_len += read_len;
        uint32_t current_time = ESP32->get_system_time_ms();

        // 每隔约 1 秒打印一次速度
        if (current_time - last_calc_time >= 1000)
        {
            float duration = (current_time - start_time) / 1000.0f;
            float speed_instant = (total_read_len - last_read_len) / 1024.0f / ((current_time - last_calc_time) / 1000.0f);
            float speed_avg = (total_read_len / 1024.0f) / duration;

            printf("progress: %.2f kb | speed: %.2f kb/s (avg: %.2f kb/s)\n",
                   total_read_len / 1024.0f, speed_instant, speed_avg);

            last_calc_time = current_time;
            last_read_len = total_read_len;
        }
    }

    uint32_t end_time = ESP32->get_system_time_ms();
    float total_duration = (end_time - start_time) / 1000.0f;
    printf("--------------------------------------------------\n");
    printf("download finished! total: %d bytes in %.2f seconds\n", total_read_len, total_duration);
    printf("final average speed: %.2f kb/s\n", (total_read_len / 1024.0f) / total_duration);

    free(buffer);
    esp_http_client_cleanup(client);
    vTaskDelete(NULL);
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

    xTaskCreate(download_task, "download_task", 8192, NULL, 5, NULL);

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}