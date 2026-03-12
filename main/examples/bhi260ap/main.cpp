/*
 * @Description: None
 * @Author: LILYGO_L
 * @Date: 2026-02-27 09:07:56
 * @LastEditTime: 2026-03-12 17:19:42
 * @License: GPL 3.0
 */
#include "lilygo_device_driver_library.h"
#include "cpp_bus_driver_library.h"
#include "SensorBHI260AP.hpp"
#include <math.h>

#define BOSCH_APP30_SHUTTLE_BHI260_FW
#include "BoschFirmware.h"

float Accel_Data[3] = {0.0f, 0.0f, 0.0f}; // x, y, z
float Gyro_Data[3] = {0.0f, 0.0f, 0.0f};  // x, y, z

bool Accel_Data_Ready_Flag = false;
bool Gyro_Data_Ready_Flag = false;

size_t Cycle_Time = 0;

auto Axp517_Iic_Bus = std::make_shared<Cpp_Bus_Driver::Hardware_Iic_1>(AXP517_SDA, AXP517_SCL, I2C_NUM_0);
auto Xl9555_Iic_Bus = std::make_shared<Cpp_Bus_Driver::Hardware_Iic_1>(XL9555_SDA, XL9555_SCL, I2C_NUM_1);

auto Axp517 = std::make_unique<Cpp_Bus_Driver::Axp517>(Axp517_Iic_Bus, AXP517_IIC_ADDRESS);
auto Xl9555 = std::make_unique<Cpp_Bus_Driver::Xl95x5>(Xl9555_Iic_Bus, XL9555_IIC_ADDRESS);

auto Bhi260ap = std::make_unique<SensorBHI260AP>();

auto ESP32P4 = std::make_unique<Cpp_Bus_Driver::Tool>();

// 计算欧拉角（俯仰pitch, 翻滚roll, 方位yaw）
void Calculate_Orientation(const float accel[3], const float gyro[3],
                           float *pitch, float *roll, float *yaw)
{
    // 使用加速度数据计算俯仰和翻滚角
    *pitch = atan2f(-accel[0], sqrtf(accel[1] * accel[1] + accel[2] * accel[2])) * 180.0f / M_PI;
    *roll = atan2f(accel[1], accel[2]) * 180.0f / M_PI;

    // 方位角（yaw）通常需要磁力计，这里用陀螺仪积分近似
    static float last_yaw = 0.0f;
    static uint32_t last_time_ms = 0;
    uint32_t now_ms = ESP32P4->get_system_time_ms();
    float dt = (last_time_ms == 0) ? 0.01f : (now_ms - last_time_ms) / 1000.0f;
    last_time_ms = now_ms;

    last_yaw += gyro[2] * dt; // 假设gyro[2]为z轴角速度（度/秒）
    *yaw = last_yaw;
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

    Bhi260ap->onResultEvent(SensorBHI260AP::ACCEL_PASSTHROUGH,
                            [](uint8_t sensor_id, uint8_t *data_ptr, uint32_t len, uint64_t *timestamp, void *user_data)
                            {
                                static size_t cycle_time = 0;

                                struct bhy2_data_xyz data;
                                float scaling_factor = Bhi260ap->getScaling(sensor_id);
                                bhy2_parse_xyz(data_ptr, &data);

                                Accel_Data[0] = data.x * scaling_factor;
                                Accel_Data[1] = data.y * scaling_factor;
                                Accel_Data[2] = data.z * scaling_factor;

                                Accel_Data_Ready_Flag = true;

                                // if (ESP32P4->get_system_time_ms() > cycle_time)
                                // {
                                //     printf("%s x: %.3f, y: %.3f, z: %.3f;\n",
                                //            Bhi260ap->getSensorName(sensor_id), data.x * scaling_factor, data.y * scaling_factor, data.z * scaling_factor);

                                //     cycle_time = ESP32P4->get_system_time_ms() + 1000;
                                // }
                            });

    Bhi260ap->onResultEvent(SensorBHI260AP::GYRO_PASSTHROUGH, [](uint8_t sensor_id, uint8_t *data_ptr, uint32_t len, uint64_t *timestamp, void *user_data)
                            {
                                static size_t cycle_time = 0;

                                struct bhy2_data_xyz data;
                                float scaling_factor = Bhi260ap->getScaling(sensor_id);
                                bhy2_parse_xyz(data_ptr, &data);

                                Gyro_Data[0] = data.x * scaling_factor;
                                Gyro_Data[1] = data.y * scaling_factor;
                                Gyro_Data[2] = data.z * scaling_factor;

                                Gyro_Data_Ready_Flag = true;

                                // if (ESP32P4->get_system_time_ms() > cycle_time)
                                // {
                                //     printf("%s x: %.3f, y: %.3f, z: %.3f;\n",
                                //            Bhi260ap->getSensorName(sensor_id), data.x * scaling_factor, data.y * scaling_factor, data.z * scaling_factor);

                                //     cycle_time = ESP32P4->get_system_time_ms() + 1000;
                                // }
                            });
}

extern "C" void app_main(void)
{
    printf("Ciallo\n");

    Axp517->begin();

    Xl9555->begin();
    Xl9555->pin_mode(XL9555_3_3_POWER_EN, Cpp_Bus_Driver::Xl95x5::Mode::OUTPUT);
    Xl9555->pin_write(XL9555_3_3_POWER_EN, Cpp_Bus_Driver::Xl95x5::Value::HIGH);

    Xl9555->pin_mode(XL9555_BHI260AP_RST, Cpp_Bus_Driver::Xl95x5::Mode::OUTPUT);
    Xl9555->pin_write(XL9555_BHI260AP_RST, Cpp_Bus_Driver::Xl95x5::Value::HIGH);
    vTaskDelay(pdMS_TO_TICKS(100));
    Xl9555->pin_write(XL9555_BHI260AP_RST, Cpp_Bus_Driver::Xl95x5::Value::LOW);
    vTaskDelay(pdMS_TO_TICKS(100));
    Xl9555->pin_write(XL9555_BHI260AP_RST, Cpp_Bus_Driver::Xl95x5::Value::HIGH);
    vTaskDelay(pdMS_TO_TICKS(100));

    Wire1._bus->set_bus_handle(Axp517_Iic_Bus->get_bus_handle());
    Bhi260ap_Init();

    while (1)
    {
        Bhi260ap->update();

        if (ESP32P4->get_system_time_ms() > Cycle_Time)
        {
            if ((Accel_Data_Ready_Flag == true) && (Gyro_Data_Ready_Flag == true))
            {

                float pitch = 0.0f, roll = 0.0f, yaw = 0.0f;
                Calculate_Orientation(Accel_Data, Gyro_Data, &pitch, &roll, &yaw);
                printf("orientation: pitch=%.2f, roll=%.2f, yaw=%.2f\n", pitch, roll, yaw);

                Accel_Data_Ready_Flag = false;
                Gyro_Data_Ready_Flag = false;
            }

            Cycle_Time = ESP32P4->get_system_time_ms() + 1000;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
