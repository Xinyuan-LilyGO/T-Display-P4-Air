/*
 * @Description: None
 * @Author: LILYGO_L
 * @Date: 2026-02-28 16:21:31
 * @LastEditTime: 2026-03-02 13:46:56
 * @License: GPL 3.0
 */

#include "sdkconfig.h"
#include "lilygo_device_driver_library.h"
#include "cpp_bus_driver_library.h"
#include "RadioLib.h"
#include "radiolib_bridge_driver.h"

uint8_t Send_Package[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};

bool Lr1121_Irq_flag = false;

auto Axp517_Iic_Bus = std::make_shared<Cpp_Bus_Driver::Hardware_Iic_1>(AXP517_SDA, AXP517_SCL, I2C_NUM_0);
auto Xl9555_Iic_Bus = std::make_shared<Cpp_Bus_Driver::Hardware_Iic_1>(XL9555_SDA, XL9555_SCL, I2C_NUM_1);

auto Spi_Bus = std::make_shared<Cpp_Bus_Driver::Hardware_Spi>(LR1121_MOSI, LR1121_SCLK, LR1121_MISO, SPI2_HOST, 0);

auto Axp517 = std::make_unique<Cpp_Bus_Driver::Axp517>(Axp517_Iic_Bus, AXP517_IIC_ADDRESS);
auto Xl9555 = std::make_unique<Cpp_Bus_Driver::Xl95x5>(Xl9555_Iic_Bus, XL9555_IIC_ADDRESS);

RadioLibHal *Radiolib_Hal = new Radiolib_Cpp_Bus_Driver_Hal(Spi_Bus, 10000000, LR1121_CS);
LR1121 Lr1121 = new Module(Radiolib_Hal, static_cast<uint32_t>(RADIOLIB_NC), LR1121_INT, LR1121_RST, LR1121_BUSY);

auto ESP32P4 = std::make_unique<Cpp_Bus_Driver::Tool>();

extern "C" void app_main(void)
{
    printf("Ciallo\n");

    Axp517->begin();

    Xl9555->begin();

    Xl9555->pin_mode(XL9555_3_3_POWER_EN, Cpp_Bus_Driver::Xl95x5::Mode::OUTPUT);
    Xl9555->pin_write(XL9555_3_3_POWER_EN, Cpp_Bus_Driver::Xl95x5::Value::HIGH);

    ESP32P4->pin_mode(ESP32P4_BOOT, Cpp_Bus_Driver::Tool::Pin_Mode::INPUT);

    ESP32P4->pin_mode(LR1121_BUSY, Cpp_Bus_Driver::Tool::Pin_Mode::INPUT, Cpp_Bus_Driver::Tool::Pin_Status ::PULLDOWN);

    // int16_t status = Lr1121.begin(915.0, 125.0, 12, 7, RADIOLIB_LR11X0_LORA_SYNC_WORD_PRIVATE, 22, 8);
    int16_t status = Lr1121.begin(2450.0, 406.25, 12, 7, RADIOLIB_LR11X0_LORA_SYNC_WORD_PRIVATE, 13, 8);
    // int16_t status = Lr1121.beginFSK(850.0, 200.0, 10, 467.0, 22, 16);
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

    Lr1121.setIrqAction(
        []()
        {
            Lr1121_Irq_flag = true;
        });

    Lr1121.startReceive();

    while (1)
    {
        if (ESP32P4->pin_read(ESP32P4_BOOT) == 0)
        {
            vTaskDelay(pdMS_TO_TICKS(300));

            printf("lr1121 send package\n");

            status = Lr1121.transmit(Send_Package, 9);
            if (status != RADIOLIB_ERR_NONE)
            {
                printf("transmit fail (error code: %d)\n", status);
            }

            status = Lr1121.startReceive();
            if (status != RADIOLIB_ERR_NONE)
            {
                printf("startReceive fail (error code: %d)\n", status);
            }

            Lr1121_Irq_flag = false;
        }

        if (Lr1121_Irq_flag == true) // 接收完成中断
        {
            uint8_t receive_package[255] = {0};
            if (Lr1121.readData(receive_package, 9) == RADIOLIB_ERR_NONE)
            {
                printf("lr1121 rssi: %.2f dbm, snr: %.2f db\n", Lr1121.getRSSI(), Lr1121.getSNR());

                for (uint8_t i = 0; i < 9; i++)
                {
                    printf("get lr1121 data[%d]: %d\n", i, receive_package[i]);
                }
            }

            Lr1121_Irq_flag = false;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
