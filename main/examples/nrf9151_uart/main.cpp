/*
 * @Description: None
 * @Author: LILYGO_L
 * @Date: 2026-03-04 11:26:40
 * @LastEditTime: 2026-03-12 17:13:53
 * @License: GPL 3.0
 */
#include "lilygo_device_driver_library.h"
#include "cpp_bus_driver_library.h"

auto Axp517_Iic_Bus = std::make_shared<Cpp_Bus_Driver::Hardware_Iic_1>(AXP517_SDA, AXP517_SCL, I2C_NUM_0);
auto Xl9535_Iic_Bus = std::make_shared<Cpp_Bus_Driver::Hardware_Iic_1>(XL9535_SDA, XL9535_SCL, I2C_NUM_1);
auto Nrf9151_Uart_Bus = std::make_shared<Cpp_Bus_Driver::Hardware_Uart>(NRF9151_UART_RX, NRF9151_UART_TX,
                                                                        NRF9151_UART_CTS, NRF9151_UART_RTS, UART_NUM_1);

auto Axp517 = std::make_unique<Cpp_Bus_Driver::Axp517>(Axp517_Iic_Bus, AXP517_IIC_ADDRESS);
auto Xl9535 = std::make_unique<Cpp_Bus_Driver::Xl95x5>(Xl9535_Iic_Bus, XL9535_IIC_ADDRESS);

extern "C" void app_main(void)
{
    printf("Ciallo\n");

    uart_driver_install(UART_NUM_0, 2 * 1024, 0, 0, NULL, 0);

    Axp517->begin();
    Xl9535->begin();
    Xl9535->pin_mode(XL9535_3_3_POWER_EN, Cpp_Bus_Driver::Xl95x5::Mode::OUTPUT);
    Xl9535->pin_write(XL9535_3_3_POWER_EN, Cpp_Bus_Driver::Xl95x5::Value::HIGH);

    Xl9535->pin_mode(XL9535_NRF9151_EN, Cpp_Bus_Driver::Xl95x5::Mode::OUTPUT);
    Xl9535->pin_write(XL9535_NRF9151_EN, Cpp_Bus_Driver::Xl95x5::Value::HIGH);

    Nrf9151_Uart_Bus->begin();

    uart_flush_input(UART_NUM_0);

    while (1)
    {
        size_t pc_uart_lenght = 0;
        uart_get_buffered_data_len(UART_NUM_0, &pc_uart_lenght);
        if (pc_uart_lenght > 0)
        {
            // 为了去除串口调试出现的乱码，这里+1预留最后一位'\0'
            auto buffer = std::make_unique<uint8_t[]>(pc_uart_lenght + 1);
            uart_read_bytes(UART_NUM_0, buffer.get(), pc_uart_lenght, pdMS_TO_TICKS(20));
            Nrf9151_Uart_Bus->write(buffer.get(), pc_uart_lenght);
            printf("Nrf9151_Uart_Bus send(size: %d): [%s]\n", pc_uart_lenght, buffer.get());
        }

        size_t nrf9151_uart_length = Nrf9151_Uart_Bus->get_rx_buffer_length();
        if (nrf9151_uart_length > 0)
        {
            // 为了去除串口调试出现的乱码，这里+1预留最后一位'\0'
            auto buffer = std::make_unique<uint8_t[]>(nrf9151_uart_length + 1);
            Nrf9151_Uart_Bus->read(buffer.get(), nrf9151_uart_length);
            printf("Nrf9151_Uart_Bus receive(size: %d): [%s]\n", nrf9151_uart_length, buffer.get());
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
