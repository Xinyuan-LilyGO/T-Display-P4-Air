/*
 * @Description: None
 * @Author: LILYGO_L
 * @Date: 2026-02-02 17:40:30
 * @LastEditTime: 2026-03-12 17:21:32
 * @License: GPL 3.0
 */
#include "lilygo_device_driver_library.h"
#include "cpp_bus_driver_library.h"

auto Iic_Bus = std::make_shared<Cpp_Bus_Driver::Hardware_Iic_1>(XL9535_SDA, XL9535_SCL, I2C_NUM_0);

auto Xl9535 = std::make_unique<Cpp_Bus_Driver::Xl95x5>(Iic_Bus, XL9535_IIC_ADDRESS);

extern "C" void app_main(void)
{
    printf("Ciallo\n");

    Xl9535->begin();
    Xl9535->pin_mode(Cpp_Bus_Driver::Xl95x5::Pin::IO6, Cpp_Bus_Driver::Xl95x5::Mode::OUTPUT);
    Xl9535->pin_mode(XL9535_LED_1, Cpp_Bus_Driver::Xl95x5::Mode::OUTPUT);
    Xl9535->pin_mode(Cpp_Bus_Driver::Xl95x5::Pin::IO7, Cpp_Bus_Driver::Xl95x5::Mode::INPUT);

    Xl9535->pin_mode(XL9535_ESP32C5_EN, Cpp_Bus_Driver::Xl95x5::Mode::OUTPUT);

    Xl9535->pin_write(XL9535_ESP32C5_EN, Cpp_Bus_Driver::Xl95x5::Value::HIGH);

    Xl9535->pin_mode(XL9535_ESP32P4_ESP32C5_UART_SWITCH, Cpp_Bus_Driver::Xl95x5::Mode::OUTPUT);

    Xl9535->pin_write(XL9535_ESP32P4_ESP32C5_UART_SWITCH, Cpp_Bus_Driver::Xl95x5::Value::HIGH);

    while (1)
    {
        Xl9535->pin_write(Cpp_Bus_Driver::Xl95x5::Pin::IO6, Cpp_Bus_Driver::Xl95x5::Value::HIGH);
        Xl9535->pin_write(XL9535_LED_1, Cpp_Bus_Driver::Xl95x5::Value::HIGH);
        printf("xl9555 io7: %d\n", Xl9535->pin_read(Cpp_Bus_Driver::Xl95x5::Pin::IO7));
        vTaskDelay(pdMS_TO_TICKS(1000));
        Xl9535->pin_write(Cpp_Bus_Driver::Xl95x5::Pin::IO6, Cpp_Bus_Driver::Xl95x5::Value::LOW);
        Xl9535->pin_write(XL9535_LED_1, Cpp_Bus_Driver::Xl95x5::Value::LOW);
        printf("xl9555 io7: %d\n", Xl9535->pin_read(Cpp_Bus_Driver::Xl95x5::Pin::IO7));
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
