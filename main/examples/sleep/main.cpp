/*
 * @Description: None
 * @Author: LILYGO_L
 * @Date: 2026-03-31 09:24:53
 * @LastEditTime: 2026-04-09 18:11:00
 * @License: GPL 3.0
 */
#include "cpp_bus_driver_library.h"
#include "lilygo_device_driver_library.h"

#include "esp_netif.h"
#include "esp_event.h"
#include "esp_private/wifi.h"

#include "app_video.h"
#include "driver/ppa.h"
#include "esp_private/esp_cache_private.h"
#include "esp_video_init.h"

#include "esp_hosted.h"
#include "esp_wifi_remote.h"

#include "esp_sleep.h"
#include "light_sleep_example.h"

#include "radiolib_bridge_driver.h"

#include "SensorBHI260AP.hpp"
#include "SensorQMC6310.hpp"

#define BOSCH_APP30_SHUTTLE_BHI260_FW
#include "BoschFirmware.h"

#define ALIGN_UP(num, align) (((num) + ((align) - 1)) & ~((align) - 1))

#define MCLK_MULTIPLE i2s_mclk_multiple_t::I2S_MCLK_MULTIPLE_256
#define SAMPLE_RATE 44100

// #define USE_SCREEN

#define WIFI_SSID "xinyuandianzi"
#define WIFI_PASSWORD "AA15994823428"

ppa_client_handle_t ppa_srm_handle = NULL;
size_t data_cache_line_size = 0;
int32_t video_cam_fd0;

int32_t fps_count;
int64_t start_time;

// IIC 1
auto Xl9555_Iic_Bus = std::make_shared<Cpp_Bus_Driver::Hardware_Iic_1>(XL9555_SDA, XL9555_SCL, I2C_NUM_0);

// IIC 2
auto Axp517_Iic_Bus = std::make_shared<Cpp_Bus_Driver::Hardware_Iic_1>(AXP517_SDA, AXP517_SCL, I2C_NUM_1);
auto Sgm38121_Iic_Bus = std::make_shared<Cpp_Bus_Driver::Hardware_Iic_1>(SGM38121_SDA, SGM38121_SCL, I2C_NUM_1);
auto Aw86224_Iic_Bus = std::make_shared<Cpp_Bus_Driver::Hardware_Iic_1>(AW86224_SDA, AW86224_SCL, I2C_NUM_1);
auto Es8388_Iic_Bus = std::make_shared<Cpp_Bus_Driver::Hardware_Iic_1>(ES8388_SDA, ES8388_SCL, I2C_NUM_1);

// IIS
auto Es8388_Iis_Bus = std::make_shared<Cpp_Bus_Driver::Hardware_Iis>(ES8388_ADC_DATA, ES8388_DAC_DATA, ES8388_WS_LRCK, ES8388_BCLK, ES8388_MCLK,
                                                                     i2s_port_t::I2S_NUM_0, Cpp_Bus_Driver::Hardware_Iis::Data_Mode::INPUT_OUTPUT, Cpp_Bus_Driver::Hardware_Iis::Iis_Mode::STD,
                                                                     i2s_clock_src_t::I2S_CLK_SRC_APLL);

// SPI
auto LR1121_Spi_Bus = std::make_shared<Cpp_Bus_Driver::Hardware_Spi>(LR1121_MOSI, LR1121_SCLK, LR1121_MISO, SPI2_HOST, 0);

auto Screen_Mipi_Bus = std::make_shared<Cpp_Bus_Driver::Hardware_Mipi>(SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_MIPI_DSI_HSYNC, SCREEN_MIPI_DSI_HBP, SCREEN_MIPI_DSI_HFP,
                                                                       SCREEN_MIPI_DSI_VSYNC, SCREEN_MIPI_DSI_VBP, SCREEN_MIPI_DSI_VFP, SCREEN_DATA_LANE_NUM,
                                                                       [](uint8_t format) -> Cpp_Bus_Driver::Hardware_Mipi::Color_Format
                                                                       {
                                                                    switch (format)
                                                                    {
                                                                    case 16:
                                                                        return Cpp_Bus_Driver::Hardware_Mipi::Color_Format::RGB565;
                                                                    case 24:
                                                                        return Cpp_Bus_Driver::Hardware_Mipi::Color_Format::RGB888;
                                                                    default:
                                                                        return Cpp_Bus_Driver::Hardware_Mipi::Color_Format::RGB565;
                                                                    } }(SCREEN_BITS_PER_PIXEL));

// IIC 1
auto Xl9555 = std::make_unique<Cpp_Bus_Driver::Xl95x5>(Xl9555_Iic_Bus, XL9555_IIC_ADDRESS);

// IIC 2
auto Axp517 = std::make_unique<Cpp_Bus_Driver::Axp517>(Axp517_Iic_Bus, AXP517_IIC_ADDRESS);
auto Sgm38121 = std::make_unique<Cpp_Bus_Driver::Sgm38121>(Sgm38121_Iic_Bus, SGM38121_IIC_ADDRESS);
auto Aw86224 = std::make_unique<Cpp_Bus_Driver::Aw862xx>(Aw86224_Iic_Bus, AW86224_IIC_ADDRESS);

RadioLibHal *Radiolib_Hal = new Radiolib_Cpp_Bus_Driver_Hal(LR1121_Spi_Bus, 10000000, LR1121_CS);
LR1121 Lr1121 = new Module(Radiolib_Hal, static_cast<uint32_t>(RADIOLIB_NC), LR1121_INT, LR1121_RST, LR1121_BUSY);

auto Esp32p4 = std::make_unique<Cpp_Bus_Driver::Tool>();

auto Hi8561_Iic_Touch_Bus = std::make_shared<Cpp_Bus_Driver::Hardware_Iic_1>(HI8561_TOUCH_SDA, HI8561_TOUCH_SCL, I2C_NUM_0);

auto Hi8561_Touch = std::make_unique<Cpp_Bus_Driver::Hi8561_Touch>(Hi8561_Iic_Touch_Bus, HI8561_TOUCH_IIC_ADDRESS);

auto Screen = std::make_unique<Cpp_Bus_Driver::Hi8561>(Screen_Mipi_Bus);

auto Bhi260ap = std::make_unique<SensorBHI260AP>();
auto Qmc6310n = std::make_unique<SensorQMC6310>();

void wifi_cleanup(void)
{
    esp_wifi_disconnect();
    vTaskDelay(pdMS_TO_TICKS(200));

    /* Post disconnect/stop events to clean up netif state */
    wifi_event_sta_disconnected_t disconnect_event = {0};
    disconnect_event.reason = WIFI_REASON_ASSOC_LEAVE;
    esp_event_post(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED,
                   &disconnect_event, sizeof(wifi_event_sta_disconnected_t), portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(100));

    esp_event_post(WIFI_EVENT, WIFI_EVENT_STA_STOP, NULL, 0, portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(500));

    esp_wifi_internal_reg_rxcb(WIFI_IF_STA, NULL);
    esp_wifi_internal_reg_rxcb(WIFI_IF_AP, NULL);

    esp_wifi_stop();
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_wifi_deinit();
}

void Device_Sleep_Status(bool status)
{
    if (status == true)
    {
        printf("device sleep start\n");

        Aw86224->set_force_enter_mode(Cpp_Bus_Driver::Aw862xx::Force_Mode::STANDBY, true);
        Qmc6310n->setMode(SensorQMC6310::MODE_SUSPEND);

        Bhi260ap->configure(SensorBHI260AP::ACCEL_PASSTHROUGH, 0.0, 0);
        Bhi260ap->configure(SensorBHI260AP::GYRO_PASSTHROUGH, 0.0, 0);

#if defined CONFIG_CAMERA_TYPE_SC2336
        Sgm38121->set_channel_status(Cpp_Bus_Driver::Sgm38121::Channel::AVDD_1, Cpp_Bus_Driver::Sgm38121::Status::OFF);
        Sgm38121->set_channel_status(Cpp_Bus_Driver::Sgm38121::Channel::AVDD_2, Cpp_Bus_Driver::Sgm38121::Status::OFF);
#elif (defined CONFIG_CAMERA_TYPE_OV2710) || (defined CONFIG_CAMERA_TYPE_OV5645)
        Sgm38121->set_channel_status(Cpp_Bus_Driver::Sgm38121::Channel::DVDD_1, Cpp_Bus_Driver::Sgm38121::Status::OFF);
        Sgm38121->set_channel_status(Cpp_Bus_Driver::Sgm38121::Channel::AVDD_1, Cpp_Bus_Driver::Sgm38121::Status::OFF);
        Sgm38121->set_channel_status(Cpp_Bus_Driver::Sgm38121::Channel::AVDD_2, Cpp_Bus_Driver::Sgm38121::Status::OFF);
#else
#error "no macro definition is set"
#endif

        wifi_cleanup();

        ESP_ERROR_CHECK(esp_hosted_deinit());

        Lr1121.sleep();

#if defined USE_SCREEN
        printf("esp_lcd_panel_disp_off\n");

        Screen->set_screen_off(true);
        Screen->set_sleep(true);

#if defined CONFIG_SCREEN_TYPE_HI8561

#elif defined CONFIG_SCREEN_TYPE_RM69A10
        Xl9555->pin_mode(XL9555_TOUCH_INT, Cpp_Bus_Driver::Xl95x5::Mode::OUTPUT);
        Xl9555->pin_write(XL9555_TOUCH_INT, Cpp_Bus_Driver::Xl95x5::Value::LOW);

        Gt9895->set_sleep();
#else
#error "no macro definition is set"
#endif

        Xl9555->pin_write(XL9555_SCREEN_RST, Cpp_Bus_Driver::Xl95x5::Value::LOW);
        Xl9555->pin_write(XL9555_TOUCH_RST, Cpp_Bus_Driver::Xl95x5::Value::LOW);
#endif

        Xl9555->pin_write(XL9555_LR1121_POWER_EN, Cpp_Bus_Driver::Xl95x5::Value::LOW);
        Xl9555->pin_write(XL9555_SD_POWER_EN, Cpp_Bus_Driver::Xl95x5::Value::LOW);
        Xl9555->pin_write(XL9555_ESP32C5_EN, Cpp_Bus_Driver::Xl95x5::Value::LOW);
        Xl9555->pin_write(XL9555_3_3_POWER_EN, Cpp_Bus_Driver::Xl95x5::Value::LOW);
    }
    else
    {
        printf("device sleep close\n");
    }
}

void wifi_event_handler(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        printf("wifi started\n");

        // esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        printf("wifi disconnected / connect failed, retrying...\n");
        esp_wifi_connect();
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        char ip_str[64];
        sprintf(ip_str, "wifi ip:" IPSTR ":", IP2STR(&event->ip_info.ip));
        printf("got ip: %s\n", ip_str);
    }
}

bool Wifi_Init_Connect(void)
{
    // esp_err_t ret = nvs_flash_init();
    // if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    // {
    //     ESP_ERROR_CHECK(nvs_flash_erase());
    //     ret = nvs_flash_init();
    // }
    // ESP_ERROR_CHECK(ret);

    // Initialize TCP/IP network interface aka the esp-netif (should be called only once in application)
    ESP_ERROR_CHECK(esp_netif_init());
    // Create default event loop that running in background
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config =
        {
            .sta =
                {
                    .ssid = WIFI_SSID,
                    .password = WIFI_PASSWORD,
                },
        };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    printf("connecting to wifi: %s\n", WIFI_SSID);
    ESP_ERROR_CHECK(esp_wifi_connect());

    // Wait for connection
    for (int i = 0; i < 10; i++)
    {
        wifi_ap_record_t ap_info;
        // Check connection status
        if ((esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) && (ap_info.ssid[0] != 0))
        {
            printf("wifi connected\n");
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    return false;
}

void camera_video_frame_operation(uint8_t *camera_buf, uint8_t camera_buf_index, uint32_t camera_buf_hes, uint32_t camera_buf_ves, size_t camera_buf_len)
{
    fps_count++;
    if (fps_count == 50)
    {
        int64_t end_time = esp_timer_get_time();
        printf("fps: %f\n", 1000000.0 / ((end_time - start_time) / 50.0));
        start_time = end_time;
        fps_count = 0;

        printf("camera_buf_hes: %lu, camera_buf_ves: %lu, camera_buf_len: %d KB\n", camera_buf_hes, camera_buf_ves, camera_buf_len / 1024);
    }
}

bool App_Video_Init(void)
{
    ppa_client_config_t ppa_srm_config =
        {
            .oper_type = PPA_OPERATION_SRM,
        };
    esp_err_t assert = ppa_register_client(&ppa_srm_config, &ppa_srm_handle);
    if (assert != ESP_OK)
    {
        printf("ppa_register_client fail (error code: %#X)\n", assert);
        return false;
    }
    assert = esp_cache_get_alignment(MALLOC_CAP_SPIRAM, &data_cache_line_size);
    if (assert != ESP_OK)
    {
        printf("esp_cache_get_alignment fail (error code: %#X)\n", assert);
        return false;
    }

    esp_video_init_csi_config_t csi_config =
        {
            .sccb_config = {
                .init_sccb = false,
                .i2c_handle = Sgm38121_Iic_Bus->get_bus_handle(),
                .freq = static_cast<uint32_t>(100000),
            },
            .reset_pin = gpio_num_t ::GPIO_NUM_NC,
            .pwdn_pin = gpio_num_t ::GPIO_NUM_NC,

            .dont_init_ldo = true,
        };

    esp_video_init_config_t cam_config =
        {
            .csi = &csi_config,
        };

    assert = esp_video_init(&cam_config);
    if (assert != ESP_OK)
    {
        printf("esp_video_init fail (error code: %#X)\n", assert);
        return false;
    }

#if (defined CONFIG_CAMERA_TYPE_SC2336) || (defined CONFIG_CAMERA_TYPE_OV2710)
#if defined CONFIG_SCREEN_PIXEL_FORMAT_RGB565
    video_cam_fd0 = app_video_open(ESP_VIDEO_MIPI_CSI_DEVICE_NAME, video_fmt_t::APP_VIDEO_FMT_RGB565);
    if (video_cam_fd0 < 0)
    {
        printf("video cam open fail (video_cam_fd0: %ld)\n", video_cam_fd0);
        return false;
    }
#elif defined CONFIG_SCREEN_PIXEL_FORMAT_RGB888
    video_cam_fd0 = app_video_open(ESP_VIDEO_MIPI_CSI_DEVICE_NAME, video_fmt_t::APP_VIDEO_FMT_RGB888);
    if (video_cam_fd0 < 0)
    {
        printf("video cam open fail (video_cam_fd0: %ld)\n", video_cam_fd0);
        return false;
    }
#else
#error "no macro definition is set"
#endif
#elif defined CONFIG_CAMERA_TYPE_OV5645
    video_cam_fd0 = app_video_open(ESP_VIDEO_MIPI_CSI_DEVICE_NAME, video_fmt_t::APP_VIDEO_FMT_RGB565);
    if (video_cam_fd0 < 0)
    {
        printf("video cam open fail (video_cam_fd0: %ld)\n", video_cam_fd0);
        return false;
    }
#else
#error "no macro definition is set"
#endif

    assert = app_video_set_bufs(video_cam_fd0, CAMERA_BUFFER_COUNT, NULL);
    if (assert != ESP_OK)
    {
        printf("app_video_set_bufs fail (error code: %#X)\n", assert);
        return false;
    }

    assert = app_video_register_frame_operation_cb(camera_video_frame_operation);
    if (assert != ESP_OK)
    {

        printf("app_video_register_frame_operation_cb fail (error code: %#X)\n", assert);
        return false;
    }

    assert = app_video_stream_task_start(video_cam_fd0, 0);
    if (assert != ESP_OK)
    {

        printf("app_video_stream_task_start fail (error code: %#X)\n", assert);
        return false;
    }

    // app_video_stream_task_stop(video_cam_fd0);

    // // Get the initial time for frame rate statistics
    // start_time = esp_timer_get_time();

    return true;
}

void Bhi260ap_Init(void)
{
    printf("Bhi260ap_Init\n");

    Bhi260ap->setFirmware(bosch_firmware_image, bosch_firmware_size, bosch_firmware_type, true);

    Bhi260ap->setBootFromFlash(bosch_firmware_type);

    Wire1.set_freq(1000000);
    if (Bhi260ap->begin(Wire1, BHI260AP_IIC_ADDRESS, BHI260AP_SDA, BHI260AP_SCL) == false)
    {
        printf("Bhi260ap begin fail (error code: %s)\n", Bhi260ap->getError());
    }
    else
    {
        printf("Bhi260ap begin success\n");
    }

    BoschSensorInfo info = Bhi260ap->getSensorInfo();
    info.printInfo();

    Bhi260ap->configure(SensorBHI260AP::ACCEL_PASSTHROUGH, 100.0, 0);
    Bhi260ap->configure(SensorBHI260AP::GYRO_PASSTHROUGH, 100.0, 0);
}

bool Qmc6310n_Init(void)
{
    printf("Qmc6310n_Init\n");

    Wire.set_freq(1000000);
    if (Qmc6310n->begin(Wire, QMC6310N_IIC_ADDRESS, QMC6310N_SDA, QMC6310N_SCL) == false)
    {
        printf("qmc6310n fail\n");

        return false;
    }

    printf("qmc6310n device id: %#X\n", Qmc6310n->getChipID());

    Qmc6310n->configMagnetometer(
        SensorQMC6310::MODE_CONTINUOUS, // 连续模式
        SensorQMC6310::RANGE_8G,
        SensorQMC6310::DATARATE_200HZ,
        SensorQMC6310::OSR_1,
        SensorQMC6310::DSR_1);

    printf("Qmc6310n_Init success\n");

    return true;
}

extern "C" void app_main(void)
{
    // esp_deep_sleep_start();
    printf("Ciallo\n");

    const char *wakeup_reason;
    switch (esp_sleep_get_wakeup_cause())
    {
    case ESP_SLEEP_WAKEUP_TIMER:
        wakeup_reason = "timer";
        break;
    case ESP_SLEEP_WAKEUP_GPIO:
    case ESP_SLEEP_WAKEUP_EXT0:
    case ESP_SLEEP_WAKEUP_EXT1:
        wakeup_reason = "pin";
        example_wait_gpio_inactive();
        break;
    default:
        wakeup_reason = "other";
        break;
    }
    printf("Wake up from deep sleep, reason: %s\n", wakeup_reason);

    Esp32p4->pin_mode(12, Cpp_Bus_Driver::Tool::Pin_Mode::OUTPUT);
    Esp32p4->pin_write(12, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    Esp32p4->pin_write(12, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    Esp32p4->pin_write(12, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    Axp517->begin();

    Xl9555->begin();

#if defined USE_SCREEN
    Xl9555->pin_mode(XL9555_SCREEN_RST, Cpp_Bus_Driver::Xl95x5::Mode::OUTPUT);
    Xl9555->pin_mode(XL9555_TOUCH_RST, Cpp_Bus_Driver::Xl95x5::Mode::OUTPUT);
#endif
    Xl9555->pin_mode(XL9555_3_3_POWER_EN, Cpp_Bus_Driver::Xl95x5::Mode::OUTPUT);
    Xl9555->pin_mode(XL9555_NS4150_EN, Cpp_Bus_Driver::Xl95x5::Mode::OUTPUT);
    Xl9555->pin_mode(XL9555_LR1121_POWER_EN, Cpp_Bus_Driver::Xl95x5::Mode::OUTPUT);
    Xl9555->pin_mode(XL9555_SD_POWER_EN, Cpp_Bus_Driver::Xl95x5::Mode::OUTPUT);
    Xl9555->pin_mode(XL9555_ESP32C5_EN, Cpp_Bus_Driver::Xl95x5::Mode::OUTPUT);
    Xl9555->pin_mode(XL9555_LED_1, Cpp_Bus_Driver::Xl95x5::Mode::OUTPUT);
    Xl9555->pin_mode(XL9555_NRF9151_EN, Cpp_Bus_Driver::Xl95x5::Mode::OUTPUT);
    Xl9555->pin_mode(XL9555_USBPHY_POWER_EN, Cpp_Bus_Driver::Xl95x5::Mode::OUTPUT);

    Xl9555->pin_write(XL9555_NS4150_EN, Cpp_Bus_Driver::Xl95x5::Value::LOW);
    Xl9555->pin_write(XL9555_NRF9151_EN, Cpp_Bus_Driver::Xl95x5::Value::LOW);
    Xl9555->pin_write(XL9555_USBPHY_POWER_EN, Cpp_Bus_Driver::Xl95x5::Value::HIGH);

    Xl9555->pin_write(XL9555_3_3_POWER_EN, Cpp_Bus_Driver::Xl95x5::Value::HIGH);
    // vTaskDelay(pdMS_TO_TICKS(100));
    // Xl9555->pin_write(XL9555_3_3_POWER_EN, Cpp_Bus_Driver::Xl95x5::Value::LOW);
    // vTaskDelay(pdMS_TO_TICKS(100));
    // Xl9555->pin_write(XL9555_3_3_POWER_EN, Cpp_Bus_Driver::Xl95x5::Value::HIGH);
    // vTaskDelay(pdMS_TO_TICKS(100));

    Xl9555->pin_write(XL9555_LED_1, Cpp_Bus_Driver::Xl95x5::Value::LOW);

    Sgm38121_Iic_Bus->set_bus_handle(Axp517_Iic_Bus->get_bus_handle());
    if (Sgm38121->begin() == false)
    {
        printf("sgm38121 init fail\n");
    }

#if defined CONFIG_CAMERA_TYPE_SC2336
    Sgm38121->set_output_voltage(Cpp_Bus_Driver::Sgm38121::Channel::AVDD_1, 1800);
    Sgm38121->set_output_voltage(Cpp_Bus_Driver::Sgm38121::Channel::AVDD_2, 2800);
    Sgm38121->set_channel_status(Cpp_Bus_Driver::Sgm38121::Channel::AVDD_1, Cpp_Bus_Driver::Sgm38121::Status::ON);
    Sgm38121->set_channel_status(Cpp_Bus_Driver::Sgm38121::Channel::AVDD_2, Cpp_Bus_Driver::Sgm38121::Status::ON);
#elif defined CONFIG_CAMERA_TYPE_OV2710
    Sgm38121->set_output_voltage(Cpp_Bus_Driver::Sgm38121::Channel::DVDD_1, 1500);
    Sgm38121->set_output_voltage(Cpp_Bus_Driver::Sgm38121::Channel::AVDD_1, 1700);
    Sgm38121->set_output_voltage(Cpp_Bus_Driver::Sgm38121::Channel::AVDD_2, 3000);
    Sgm38121->set_channel_status(Cpp_Bus_Driver::Sgm38121::Channel::DVDD_1, Cpp_Bus_Driver::Sgm38121::Status::ON);
    Sgm38121->set_channel_status(Cpp_Bus_Driver::Sgm38121::Channel::AVDD_1, Cpp_Bus_Driver::Sgm38121::Status::ON);
    Sgm38121->set_channel_status(Cpp_Bus_Driver::Sgm38121::Channel::AVDD_2, Cpp_Bus_Driver::Sgm38121::Status::ON);
#elif defined CONFIG_CAMERA_TYPE_OV5645
    Sgm38121->set_output_voltage(Cpp_Bus_Driver::Sgm38121::Channel::DVDD_1, 1500);
    Sgm38121->set_output_voltage(Cpp_Bus_Driver::Sgm38121::Channel::AVDD_1, 1800);
    Sgm38121->set_output_voltage(Cpp_Bus_Driver::Sgm38121::Channel::AVDD_2, 2800);
    Sgm38121->set_channel_status(Cpp_Bus_Driver::Sgm38121::Channel::DVDD_1, Cpp_Bus_Driver::Sgm38121::Status::ON);
    Sgm38121->set_channel_status(Cpp_Bus_Driver::Sgm38121::Channel::AVDD_1, Cpp_Bus_Driver::Sgm38121::Status::ON);
    Sgm38121->set_channel_status(Cpp_Bus_Driver::Sgm38121::Channel::AVDD_2, Cpp_Bus_Driver::Sgm38121::Status::ON);
#else
#error "no macro definition is set"
#endif

    Lilygo_Device_Driver::Init_Ldo_Channel_Power(3, 2500);

    Lilygo_Device_Driver::Init_Ldo_Channel_Power(4, 3300);

#if defined USE_SCREEN
    Xl9555->pin_write(XL9555_SCREEN_RST, Cpp_Bus_Driver::Xl95x5::Value::HIGH);
    Xl9555->pin_write(XL9555_TOUCH_RST, Cpp_Bus_Driver::Xl95x5::Value::HIGH);
#endif
    Xl9555->pin_write(XL9555_ESP32C5_EN, Cpp_Bus_Driver::Xl95x5::Value::HIGH);
    Xl9555->pin_write(XL9555_SD_POWER_EN, Cpp_Bus_Driver::Xl95x5::Value::HIGH);
    vTaskDelay(pdMS_TO_TICKS(100));
#if defined USE_SCREEN
    Xl9555->pin_write(XL9555_SCREEN_RST, Cpp_Bus_Driver::Xl95x5::Value::LOW);
    Xl9555->pin_write(XL9555_TOUCH_RST, Cpp_Bus_Driver::Xl95x5::Value::LOW);
#endif
    Xl9555->pin_write(XL9555_ESP32C5_EN, Cpp_Bus_Driver::Xl95x5::Value::LOW);
    Xl9555->pin_write(XL9555_SD_POWER_EN, Cpp_Bus_Driver::Xl95x5::Value::LOW);
    vTaskDelay(pdMS_TO_TICKS(100));
#if defined USE_SCREEN
    Xl9555->pin_write(XL9555_SCREEN_RST, Cpp_Bus_Driver::Xl95x5::Value::HIGH);
    Xl9555->pin_write(XL9555_TOUCH_RST, Cpp_Bus_Driver::Xl95x5::Value::HIGH);
#endif
    Xl9555->pin_write(XL9555_ESP32C5_EN, Cpp_Bus_Driver::Xl95x5::Value::HIGH);
    Xl9555->pin_write(XL9555_SD_POWER_EN, Cpp_Bus_Driver::Xl95x5::Value::HIGH);
    vTaskDelay(pdMS_TO_TICKS(1000));

#if defined USE_SCREEN
#if defined CONFIG_SCREEN_TYPE_HI8561
    // 这个必须放在以太网后面
    Screen->create_pwm(HI8561_SCREEN_BL, ledc_timer_t::LEDC_TIMER_0, ledc_channel_t::LEDC_CHANNEL_0, 2000);

#elif defined CONFIG_SCREEN_TYPE_RM69A10
#else
#error "no macro definition is set"
#endif
#endif

    if (App_Video_Init() == false)
    {
        printf("App_Video_Init fail\n");
    }

#if defined USE_SCREEN

    Screen->begin(SCREEN_MIPI_DSI_DPI_CLK_MHZ, SCREEN_LANE_BIT_RATE_MBPS);

#if defined CONFIG_SCREEN_TYPE_HI8561
    Hi8561_Iic_Touch_Bus->set_bus_handle(Xl9555_Iic_Bus->get_bus_handle());

    Hi8561_Touch->begin();

#elif defined CONFIG_SCREEN_TYPE_RM69A10

    Gt9895_Touch_Iic_Bus->set_bus_handle(Xl9555_Iic_Bus->get_bus_handle());

    Gt9895->begin();

#else
#error "no macro definition is set"
#endif

#endif
    if (Wifi_Init_Connect() == false)
    {
        printf("Wifi_Init_Connect fail\n");
    }

    if (Lilygo_Device_Driver::Sdmmc_Init(SD_BASE_PATH) == false)
    {
        printf("Sdmmc_Init fail\n");
    }

    Aw86224_Iic_Bus->set_bus_handle(Sgm38121_Iic_Bus->get_bus_handle());
    Aw86224->begin(500000);
    // printf("AW86224 input voltage: %.06f V\n", Aw86224->get_input_voltage());

    // RAM播放
    Aw86224->init_ram_mode(Cpp_Bus_Driver::aw862xx_haptic_ram_12k_0809_170, sizeof(Cpp_Bus_Driver::aw862xx_haptic_ram_12k_0809_170));

    Xl9555->pin_mode(XL9555_LR1121_POWER_EN, Cpp_Bus_Driver::Xl95x5::Mode::OUTPUT);
    Xl9555->pin_write(XL9555_LR1121_POWER_EN, Cpp_Bus_Driver::Xl95x5::Value::HIGH);
    vTaskDelay(pdMS_TO_TICKS(100));
    Xl9555->pin_write(XL9555_LR1121_POWER_EN, Cpp_Bus_Driver::Xl95x5::Value::LOW);
    vTaskDelay(pdMS_TO_TICKS(100));
    Xl9555->pin_write(XL9555_LR1121_POWER_EN, Cpp_Bus_Driver::Xl95x5::Value::HIGH);
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

    Lr1121.startReceive();

    Wire1._bus->set_bus_handle(Axp517_Iic_Bus->get_bus_handle());
    Bhi260ap_Init();

    Wire._bus->set_bus_handle(Xl9555_Iic_Bus->get_bus_handle());
    Qmc6310n_Init();

#if defined USE_SCREEN

#if defined CONFIG_SCREEN_PIXEL_FORMAT_RGB565
    // 设置整个屏幕为白色
    size_t screen_size = SCREEN_WIDTH * SCREEN_HEIGHT * 2; // RGB565: 2 bytes per pixel
    size_t data_cache_line_size = 16;                      // 通常16或32，具体可查芯片手册
    void *color_buf = heap_caps_aligned_calloc(data_cache_line_size, 1, screen_size, MALLOC_CAP_SPIRAM);
    if (color_buf)
    {
        uint16_t *p = (uint16_t *)color_buf;
        for (size_t i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; ++i)
        {
            p[i] = 0xFFFF; // RGB565白色
        }
        if (Screen->send_color_stream_coordinate(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, color_buf) == false)
        {
            printf("send_color_stream_coordinate fail\n");
        }
        heap_caps_free(color_buf);
    }
#elif defined CONFIG_SCREEN_PIXEL_FORMAT_RGB888

    // 设置整个屏幕为白色
    size_t screen_size = SCREEN_WIDTH * SCREEN_HEIGHT * 3; // RGB888: 3 bytes per pixel
    size_t data_cache_line_size = 16;                      // 通常16或32，具体可查芯片手册
    void *color_buf = heap_caps_aligned_calloc(data_cache_line_size, 1, screen_size, MALLOC_CAP_SPIRAM);
    if (color_buf)
    {
        uint8_t *p = (uint8_t *)color_buf;
        for (size_t i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; ++i)
        {
            p[i * 3 + 0] = 0xFF; // R
            p[i * 3 + 1] = 0xFF; // G
            p[i * 3 + 2] = 0xFF; // B
        }
        if (Screen->send_color_stream_coordinate(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, color_buf) == false)
        {
            printf("send_color_stream_coordinate fail\n");
        }
        heap_caps_free(color_buf);
    }

#else
#error "no macro definition is set"
#endif

#if defined CONFIG_SCREEN_TYPE_HI8561
    Esp32p4->start_pwm_gradient_time(100, 500);
#elif defined CONFIG_SCREEN_TYPE_RM69A10
    for (uint8_t i = 0; i < 255; i += 5)
    {
        Screen->set_brightness(i);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
#else
#error "no macro definition is set"
#endif

#endif

    vTaskDelay(pdMS_TO_TICKS(1000));

    printf("Device is awake. Will enter deep sleep in 5 seconds...\n");
    vTaskDelay(pdMS_TO_TICKS(5000));

    Device_Sleep_Status(true);

    printf("entering deep sleep\n");
    /* To make sure the complete line is printed before entering sleep mode,
     * need to wait until UART TX FIFO is empty:
     */
    uart_wait_tx_idle_polling((uart_port_t)CONFIG_ESP_CONSOLE_UART_NUM);

    // Disable wakeups for power measurement
    // esp_sleep_enable_timer_wakeup(10 * 1000000);
    // example_register_gpio_wakeup();

    printf("Entering indefinite deep sleep for power measurement...\n");
    esp_deep_sleep_start();
}
