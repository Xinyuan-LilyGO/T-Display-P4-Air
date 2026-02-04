/*
 * @Description: None
 * @Author: LILYGO_L
 * @Date: 2026-02-04 11:01:39
 * @LastEditTime: 2026-02-04 16:56:20
 * @License: GPL 3.0
 */
#include "sdkconfig.h"
#include "lilygo_device_driver_library.h"
#include "cpp_bus_driver_library.h"

auto Iic_Bus = std::make_shared<Cpp_Bus_Driver::Hardware_Iic_1>(AXP517_SDA, AXP517_SCL, I2C_NUM_0);
auto Axp517 = std::make_unique<Cpp_Bus_Driver::Axp517>(Iic_Bus, AXP517_IIC_ADDRESS);

void print_axp517_status()
{
    printf("---------- axp517 ----------\n");

    // device info
    printf("device id: %#X\n", Axp517->get_device_id());

    // chip status 0 (flags)
    Cpp_Bus_Driver::Axp517::Chip_Status_0 status0;
    if (Axp517->get_chip_status_0(status0))
    {
        printf("chip status 0:\n");
        printf("  vbus good: %d\n", status0.vbus_good_indication);
        printf("  battery present: %d\n", status0.battery_present_status);
        printf("  battery active mode: %d\n", status0.battery_in_active_mode);
        printf("  batfet status: %d\n", status0.batfet_status);
        printf("  current limit status: %d\n", status0.current_limit_status);
        printf("  thermal regulation status: %d\n", status0.thermal_regulation_status);
    }

    // chip status 1 (charge & direction)
    Cpp_Bus_Driver::Axp517::Chip_Status_1 status1;
    if (Axp517->get_chip_status_1(status1))
    {
        printf("chip status 1:\n");

        printf("  charge status: ");
        switch (status1.charging_status)
        {
        case Cpp_Bus_Driver::Axp517::Charge_Status::TRICKLE_CHARGE:
            printf("trickle charge\n");
            break;
        case Cpp_Bus_Driver::Axp517::Charge_Status::PRECHARGE:
            printf("pre-charge\n");
            break;
        case Cpp_Bus_Driver::Axp517::Charge_Status::CONSTANT_CURRENT:
            printf("constant current\n");
            break;
        case Cpp_Bus_Driver::Axp517::Charge_Status::CONSTANT_VOLTAGE:
            printf("constant voltage\n");
            break;
        case Cpp_Bus_Driver::Axp517::Charge_Status::CHARGE_DONE:
            printf("charge done\n");
            break;
        case Cpp_Bus_Driver::Axp517::Charge_Status::NOT_CHARGING:
            printf("not charging\n");
            break;
        default:
            printf("unknown\n");
            break;
        }

        printf("  battery current direction: ");
        switch (status1.battery_current_direction)
        {
        case Cpp_Bus_Driver::Axp517::Battery_Current_Direction::STANDBY:
            printf("standby\n");
            break;
        case Cpp_Bus_Driver::Axp517::Battery_Current_Direction::CHARGE:
            printf("charging\n");
            break;
        case Cpp_Bus_Driver::Axp517::Battery_Current_Direction::DISCHARGE:
            printf("discharging\n");
            break;
        default:
            printf("unknown\n");
            break;
        }
        printf("  vindpm status: %d\n", status1.vindpm_status);

        printf("  system status indication: %d\n", status1.system_status_indication);
    }

    // battery & measurement
    printf("measurements:\n");
    printf("  battery level: %d %%\n", Axp517->get_battery_level());
    printf("  battery health: %d %%\n", Axp517->get_battery_health());
    printf("  battery temperature: %d ^C\n", Axp517->get_battery_temperature_celsius());
    printf("  vbus voltage: %u mv\n", Axp517->get_vbus_voltage());
    printf("  vbus current: %u ma\n", Axp517->get_vbus_current());
    printf("  ts voltage: %f\n", Axp517->get_ts_voltage());
    printf("  battery voltage: %u mv\n", Axp517->get_battery_voltage());
    printf("  battery current: %f ma\n", Axp517->get_battery_current());

    // data select items
    Axp517->set_adc_data_select(Cpp_Bus_Driver::Axp517::Adc_Data::SYSTEM_VOLTAGE);
    printf("  system voltage: %u mv\n", Axp517->get_system_voltage());

    Axp517->set_adc_data_select(Cpp_Bus_Driver::Axp517::Adc_Data::CHIP_TEMPERATURE_CELSIUS);
    printf("  chip die junction temperature: %f ^C\n", Axp517->get_chip_die_junction_temperature_celsius());

    Axp517->set_adc_data_select(Cpp_Bus_Driver::Axp517::Adc_Data::CHARGING_CURRENT);
    printf("  charging current: %f ma\n", Axp517->get_charging_current());

    Axp517->set_adc_data_select(Cpp_Bus_Driver::Axp517::Adc_Data::DISCHARGE_CURRENT);
    printf("  discharging current: %f ma\n", Axp517->get_discharging_current());

    printf("\n");
}

extern "C" void app_main(void)
{
    printf("Ciallo\n");

    Axp517->begin();

    // 开启所有 ADC 通道以确保数据可读
    Cpp_Bus_Driver::Axp517::Adc_Channel adc_cfg =
        {
            .vbus_current_measure = true,
            .battery_discharge_current_measure = true,
            .battery_charge_current_measure = true,
            .chip_temperature_measure = true,
            .system_voltage_measure = true,
            .vbus_voltage_measure = true,
            .ts_value_measure = true,
            .battery_voltage_measure = true,
        };

    Axp517->set_adc_channel(adc_cfg);

    // Axp517->set_charge_enable(false);
    // Axp517->set_charge_current(256);

    // Axp517->set_gpio_source(Cpp_Bus_Driver::Axp517::Gpio_Source::BY_REG);
    // Axp517->set_gpio_mode(Cpp_Bus_Driver::Axp517::Gpio_Mode::OUTPUT);
    // Axp517->gpio_write(Cpp_Bus_Driver::Axp517::Gpio_Status::HIGH);

    // Axp517->set_boost_enable(true);

    // vTaskDelay(pdMS_TO_TICKS(5000));
    // Axp517->set_force_batfet_mode(Cpp_Bus_Driver::Axp517::Force_Batfet::OFF);

    // vTaskDelay(pdMS_TO_TICKS(5000));
    // Axp517->set_shipping_mode_enable(true);

    while (1)
    {
        print_axp517_status();
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}