/*
 * @Description: None
 * @Author: LILYGO_L
 * @Date: 2026-02-27 09:48:55
 * @LastEditTime: 2026-02-27 10:29:57
 * @License: GPL 3.0
 */
#include "sdkconfig.h"
#include "lilygo_device_driver_library.h"
#include "cpp_bus_driver_library.h"
#include "SensorQMC6310.hpp"
#include <math.h>

float Mag_Data[3] = {0.0f, 0.0f, 0.0f}; // x, y, z (单位 μT)

bool Mag_Data_Ready_Flag = false;

size_t Cycle_Time = 0;

auto Axp517_Iic_Bus = std::make_shared<Cpp_Bus_Driver::Hardware_Iic_1>(AXP517_SDA, AXP517_SCL, I2C_NUM_0);
auto Xl9555_Iic_Bus = std::make_shared<Cpp_Bus_Driver::Hardware_Iic_1>(XL9555_SDA, XL9555_SCL, I2C_NUM_1);

auto Axp517 = std::make_unique<Cpp_Bus_Driver::Axp517>(Axp517_Iic_Bus, AXP517_IIC_ADDRESS);
auto Xl9555 = std::make_unique<Cpp_Bus_Driver::Xl95x5>(Xl9555_Iic_Bus, XL9555_IIC_ADDRESS);

auto Qmc6310n = std::make_unique<SensorQMC6310>();

auto ESP32P4 = std::make_unique<Cpp_Bus_Driver::Tool>();

bool Qmc6310n_Init(void)
{
    printf("Qmc6310n_Init\n");

    Wire1.set_freq(1000000);
    if (Qmc6310n->begin(Wire1, QMC6310N_IIC_ADDRESS, QMC6310N_SDA, QMC6310N_SCL) == false)
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
    printf("Ciallo\n");

    Axp517->begin();

    Xl9555->begin();

    Xl9555->pin_mode(XL9555_3_3_POWER_EN, Cpp_Bus_Driver::Xl95x5::Mode::OUTPUT);
    Xl9555->pin_write(XL9555_3_3_POWER_EN, Cpp_Bus_Driver::Xl95x5::Value::HIGH);

    Wire1._bus->set_bus_handle(Xl9555_Iic_Bus->get_bus_handle());

    Qmc6310n_Init();

    while (1)
    {
        if (Qmc6310n->isDataReady())
        {
            Qmc6310n->readData();

            Mag_Data[0] = Qmc6310n->getX(); // μT
            Mag_Data[1] = Qmc6310n->getY();
            Mag_Data[2] = Qmc6310n->getZ();

            Mag_Data_Ready_Flag = true;
        }

        if (ESP32P4->get_system_time_ms() > Cycle_Time)
        {
            if (Mag_Data_Ready_Flag)
            {
                printf("Mag:  X: %8.2f  Y: %8.2f  Z: %8.2f   μT\n",
                       Mag_Data[0], Mag_Data[1], Mag_Data[2]);

                Mag_Data_Ready_Flag = false;
            }

            Cycle_Time = ESP32P4->get_system_time_ms() + 1000; // 每秒印一次
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}