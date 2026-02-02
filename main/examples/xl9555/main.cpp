/*
 * @Description: None
 * @Author: LILYGO_L
 * @Date: 2026-02-02 17:40:30
 * @LastEditTime: 2026-02-02 18:03:44
 * @License: GPL 3.0
 */
#include "sdkconfig.h"
#include "lilygo_device_driver_library.h"
#include "cpp_bus_driver_library.h"

auto Iic_Bus = std::make_shared<Cpp_Bus_Driver::Hardware_Iic_1>(XL9555_SDA, XL9555_SCL, I2C_NUM_0);

auto Xl9555 = std::make_unique<Cpp_Bus_Driver::Xl95x5>(Iic_Bus, XL9555_IIC_ADDRESS);

extern "C" void app_main(void)
{
    printf("Ciallo\n");

    Xl9555->begin();
    Xl9555->pin_mode(Cpp_Bus_Driver::Xl95x5::Pin::IO6, Cpp_Bus_Driver::Xl95x5::Mode::OUTPUT);
    Xl9555->pin_mode(Cpp_Bus_Driver::Xl95x5::Pin::IO7, Cpp_Bus_Driver::Xl95x5::Mode::INPUT);

    while (1)
    {
        Xl9555->pin_write(Cpp_Bus_Driver::Xl95x5::Pin::IO6, Cpp_Bus_Driver::Xl95x5::Value::HIGH);
        printf("xl9555 io7: %d\n", Xl9555->pin_read(Cpp_Bus_Driver::Xl95x5::Pin::IO7));
        vTaskDelay(pdMS_TO_TICKS(1000));
        Xl9555->pin_write(Cpp_Bus_Driver::Xl95x5::Pin::IO6, Cpp_Bus_Driver::Xl95x5::Value::LOW);
        printf("xl9555 io7: %d\n", Xl9555->pin_read(Cpp_Bus_Driver::Xl95x5::Pin::IO7));
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
