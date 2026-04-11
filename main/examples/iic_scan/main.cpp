/*
 * @Description: iic_scan
 * @Author: LILYGO_L
 * @Date: 2025-06-13 12:06:14
 * @LastEditTime: 2026-03-12 17:20:29
 * @License: GPL 3.0
 */
#include "lilygo_device_driver_library.h"
#include "cpp_bus_driver_library.h"

auto Iic_Bus = std::make_shared<Cpp_Bus_Driver::Hardware_Iic_1>(IIC_1_SDA, IIC_1_SCL, I2C_NUM_0);

auto Xl9535_Iic_Bus = std::make_shared<Cpp_Bus_Driver::Hardware_Iic_1>(XL9535_SDA, XL9535_SCL, I2C_NUM_1);

auto Xl9535 = std::make_unique<Cpp_Bus_Driver::Xl95x5>(Xl9535_Iic_Bus, XL9535_IIC_ADDRESS);

void Iic_Scan(void)
{
    std::vector<uint8_t> address;
    if (Iic_Bus->scan_7bit_address(&address) == true)
    {
        for (size_t i = 0; i < address.size(); i++)
        {
            printf("discovered iic devices[%u]: %#x\n", i, address[i]);
        }
    }
}

extern "C" void app_main(void)
{
    printf("Ciallo\n");

    Iic_Bus->begin();

    Iic_Scan();

    Xl9535_Iic_Bus->set_bus_handle(Iic_Bus->get_bus_handle());

    // vTaskDelay(pdMS_TO_TICKS(10000));

    Xl9535->begin();

    Xl9535->pin_mode(XL9535_3_3_POWER_EN, Cpp_Bus_Driver::Xl95x5::Mode::OUTPUT);

    Xl9535->pin_write(XL9535_3_3_POWER_EN, Cpp_Bus_Driver::Xl95x5::Value::HIGH);
    vTaskDelay(pdMS_TO_TICKS(100));
    Xl9535->pin_write(XL9535_3_3_POWER_EN, Cpp_Bus_Driver::Xl95x5::Value::LOW);
    vTaskDelay(pdMS_TO_TICKS(100));
    Xl9535->pin_write(XL9535_3_3_POWER_EN, Cpp_Bus_Driver::Xl95x5::Value::HIGH);
    vTaskDelay(pdMS_TO_TICKS(100));

    while (1)
    {
        Iic_Scan();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
