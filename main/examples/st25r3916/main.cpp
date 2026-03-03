/*
 * @Description: None
 * @Author: LILYGO_L
 * @Date: 2026-03-02 13:59:47
 * @LastEditTime: 2026-03-03 10:21:44
 * @License: GPL 3.0
 */
#include "sdkconfig.h"
#include "lilygo_device_driver_library.h"
#include "cpp_bus_driver_library.h"
#include "st25r3916_driver.h"

volatile bool Interrupt_Flag = false;

auto Axp517_Iic_Bus = std::make_shared<Cpp_Bus_Driver::Hardware_Iic_1>(AXP517_SDA, AXP517_SCL, I2C_NUM_0);
auto Xl9555_Iic_Bus = std::make_shared<Cpp_Bus_Driver::Hardware_Iic_1>(XL9555_SDA, XL9555_SCL, I2C_NUM_1);

auto Axp517 = std::make_unique<Cpp_Bus_Driver::Axp517>(Axp517_Iic_Bus, AXP517_IIC_ADDRESS);
auto Xl9555 = std::make_unique<Cpp_Bus_Driver::Xl95x5>(Xl9555_Iic_Bus, XL9555_IIC_ADDRESS);

auto ESP32P4 = std::make_unique<Cpp_Bus_Driver::Tool>();

size_t Cycle_Time = 0;

extern "C" void app_main(void)
{
    printf("Ciallo\n");

    Axp517->begin();

    Xl9555->begin();

    Xl9555->pin_mode(XL9555_3_3_POWER_EN, Cpp_Bus_Driver::Xl95x5::Mode::OUTPUT);
    Xl9555->pin_write(XL9555_3_3_POWER_EN, Cpp_Bus_Driver::Xl95x5::Value::HIGH);

    Wire._bus->set_bus_handle(Xl9555_Iic_Bus->get_bus_handle());
    St25r3916_Init();

    while (1)
    {
        St25r3916_Loop();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
