/*
 * @Description: axp517
 * @Author: LILYGO_L
 * @Date: 2026-02-04 17:03:13
 * @LastEditTime: 2026-05-24 16:30:00
 * @License: GPL 3.0
 */
#include "../common/t_display_p4_air_example.h"

const char* ChargeStatusName(cpp_bus_driver::Axp517::ChargeStatus status) {
  switch (status) {
    case cpp_bus_driver::Axp517::ChargeStatus::kTrickleCharge:
      return "trickle charge";
    case cpp_bus_driver::Axp517::ChargeStatus::kPrecharge:
      return "precharge";
    case cpp_bus_driver::Axp517::ChargeStatus::kConstantCurrent:
      return "constant current";
    case cpp_bus_driver::Axp517::ChargeStatus::kConstantVoltage:
      return "constant voltage";
    case cpp_bus_driver::Axp517::ChargeStatus::kChargeDone:
      return "charge done";
    case cpp_bus_driver::Axp517::ChargeStatus::kNotCharging:
      return "not charging";
    default:
      return "invalid";
  }
}

const char* BatteryCurrentDirectionName(
    cpp_bus_driver::Axp517::BatteryCurrentDirection direction) {
  switch (direction) {
    case cpp_bus_driver::Axp517::BatteryCurrentDirection::kStandby:
      return "standby";
    case cpp_bus_driver::Axp517::BatteryCurrentDirection::kCharge:
      return "charge";
    case cpp_bus_driver::Axp517::BatteryCurrentDirection::kDischarge:
      return "discharge";
    default:
      return "invalid";
  }
}

void PrintPowerInfo(cpp_bus_driver::Axp517& axp517) {
  printf("device id: %#X\n", axp517.GetDeviceId());

  cpp_bus_driver::Axp517::ChipStatus0 status0;
  if (axp517.GetChipStatus0(status0)) {
    printf("  battery present: %d\n", status0.battery_present_status);
    printf("  vbus good: %d\n", status0.vbus_good_indication);
    printf("  thermal regulation: %d\n", status0.thermal_regulation_status);
  }

  cpp_bus_driver::Axp517::ChipStatus1 status1;
  if (axp517.GetChipStatus1(status1)) {
    printf("  charging status: %s\n",
        ChargeStatusName(status1.charging_status));
    printf("  battery current direction: %s\n",
        BatteryCurrentDirectionName(status1.battery_current_direction));
  }

  printf("  battery level: %u %%\n", axp517.GetBatteryLevel());
  printf("  battery health: %u %%\n", axp517.GetBatteryHealth());
  printf("  battery temperature: %d C\n",
      axp517.GetBatteryTemperatureCelsius());
  printf("  vbus voltage: %u mv\n", axp517.GetVbusVoltage());
  printf("  vbus current: %u ma\n", axp517.GetVbusCurrent());
  printf("  ts voltage: %.2f mv\n", axp517.GetTsVoltage());
  printf("  battery voltage: %u mv\n", axp517.GetBatteryVoltage());
  printf("  battery current: %.2f ma\n", axp517.GetBatteryCurrent());

  axp517.SetAdcDataSelect(cpp_bus_driver::Axp517::AdcData::kSystemVoltage);
  printf("  system voltage: %u mv\n", axp517.GetSystemVoltage());

  axp517.SetAdcDataSelect(
      cpp_bus_driver::Axp517::AdcData::kChipTemperatureCelsius);
  printf("  chip die junction temperature: %.2f C\n",
      axp517.GetChipDieJunctionTemperatureCelsius());

  axp517.SetAdcDataSelect(cpp_bus_driver::Axp517::AdcData::kChargingCurrent);
  printf("  charging current: %.2f ma\n", axp517.GetChargingCurrent());

  axp517.SetAdcDataSelect(cpp_bus_driver::Axp517::AdcData::kDischargeCurrent);
  printf("  discharging current: %.2f ma\n", axp517.GetDischargingCurrent());
}

extern "C" void app_main(void) {
  printf("Ciallo\n");

  auto& driver = t_display_p4_air_example::InitDriver();
  auto& axp517 = driver.chip().axp517;

  cpp_bus_driver::Axp517::AdcChannel adc_channel = {
      .vbus_current_measure = true,
      .battery_discharge_current_measure = true,
      .battery_charge_current_measure = true,
      .chip_temperature_measure = true,
      .system_voltage_measure = true,
      .vbus_voltage_measure = true,
      .ts_value_measure = true,
      .battery_voltage_measure = true,
  };
  axp517->SetAdcChannel(adc_channel);

  while (1) {
    PrintPowerInfo(*axp517);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
