/*
 * @Description: None
 * @Author: LILYGO_L
 * @Date: 2026-03-02 13:59:47
 * @LastEditTime: 2026-03-12 17:21:27
 * @License: GPL 3.0
 */
#include "lilygo_device_driver_library.h"
#include "cpp_bus_driver_library.h"
#include "st25r3916_driver.h"

volatile bool Interrupt_Flag = false;

auto Axp517_Iic_Bus = std::make_shared<Cpp_Bus_Driver::Hardware_Iic_1>(AXP517_SDA, AXP517_SCL, I2C_NUM_0);
auto Xl9535_Iic_Bus = std::make_shared<Cpp_Bus_Driver::Hardware_Iic_1>(XL9535_SDA, XL9535_SCL, I2C_NUM_1);

auto Axp517 = std::make_unique<Cpp_Bus_Driver::Axp517>(Axp517_Iic_Bus, AXP517_IIC_ADDRESS);
auto Xl9535 = std::make_unique<Cpp_Bus_Driver::Xl95x5>(Xl9535_Iic_Bus, XL9535_IIC_ADDRESS);

auto ESP32P4 = std::make_unique<Cpp_Bus_Driver::Tool>();

size_t Cycle_Time = 0;

extern "C" void app_main(void)
{
    printf("Ciallo\n");

    Axp517->begin();

    Xl9535->begin();

    Xl9535->pin_mode(XL9535_3_3_POWER_EN, Cpp_Bus_Driver::Xl95x5::Mode::OUTPUT);
    Xl9535->pin_write(XL9535_3_3_POWER_EN, Cpp_Bus_Driver::Xl95x5::Value::HIGH);

    Wire._bus->set_bus_handle(Xl9535_Iic_Bus->get_bus_handle());
    St25r3916_Init();

    while (1)
    {
        St25r3916_Loop();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
