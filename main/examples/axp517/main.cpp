/*
 * @Description: axp517
 * @Author: LILYGO_L
 * @Date: 2026-02-04 17:03:13
 * @LastEditTime: 2026-05-24 16:30:00
 * @License: GPL 3.0
 */
#include "lilygo_device_driver_library.h"

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

const char* BcDetectResultName(cpp_bus_driver::Axp517::BcDetectResult result) {
  switch (result) {
    case cpp_bus_driver::Axp517::BcDetectResult::kSdp:
      return "SDP";
    case cpp_bus_driver::Axp517::BcDetectResult::kCdp:
      return "CDP";
    case cpp_bus_driver::Axp517::BcDetectResult::kDcp:
      return "DCP";
    default:
      return "invalid";
  }
}

const char* YesNo(bool value) {
  return value ? "yes" : "no";
}

void PrintChipStatus(cpp_bus_driver::Axp517& axp517) {
  cpp_bus_driver::Axp517::ChipStatus0 status0;
  if (axp517.GetChipStatus0(status0)) {
    printf("chip status0:\n");
    printf("  current limit: %s\n", YesNo(status0.current_limit_status));
    printf("  thermal regulation: %s\n",
        YesNo(status0.thermal_regulation_status));
    printf("  battery active mode: %s\n",
        YesNo(status0.battery_in_active_mode));
    printf("  battery present: %s\n",
        YesNo(status0.battery_present_status));
    printf("  batfet on: %s\n", YesNo(status0.batfet_status));
    printf("  vbus good: %s\n", YesNo(status0.vbus_good_indication));
  } else {
    printf("chip status0: read failed\n");
  }

  cpp_bus_driver::Axp517::ChipStatus1 status1;
  if (axp517.GetChipStatus1(status1)) {
    printf("chip status1:\n");
    printf("  charging status: %s\n",
        ChargeStatusName(status1.charging_status));
    printf("  vindpm status: %s\n", YesNo(status1.vindpm_status));
    printf("  system status indication: %s\n",
        YesNo(status1.system_status_indication));
    printf("  battery current direction: %s\n",
        BatteryCurrentDirectionName(status1.battery_current_direction));
  } else {
    printf("chip status1: read failed\n");
  }
}

void PrintIrqStatus(cpp_bus_driver::Axp517& axp517) {
  cpp_bus_driver::Axp517::IrqStatus0 irq0;
  cpp_bus_driver::Axp517::IrqStatus1 irq1;
  cpp_bus_driver::Axp517::IrqStatus2 irq2;
  cpp_bus_driver::Axp517::IrqStatus3 irq3;
  if (!axp517.GetIrqStatus(irq0, irq1, irq2, irq3)) {
    printf("latched irq status: read failed\n");
    return;
  }

  printf("latched irq status0:\n");
  printf("  vbus fault: %s\n", YesNo(irq0.vbus_fault_flag));
  printf("  vbus over voltage: %s\n",
      YesNo(irq0.vbus_over_voltage_flag));
  printf("  boost over voltage: %s\n",
      YesNo(irq0.boost_over_voltage_flag));
  printf("  charge to normal: %s\n", YesNo(irq0.charge_to_normal_flag));
  printf("  gauge new soc: %s\n", YesNo(irq0.gauge_new_soc_flag));
  printf("  soc drop to shutdown level: %s\n",
      YesNo(irq0.soc_drop_to_shutdown_level_flag));
  printf("  soc drop to warning level: %s\n",
      YesNo(irq0.soc_drop_to_warning_level_flag));

  printf("latched irq status1:\n");
  printf("  power key positive edge: %s\n",
      YesNo(irq1.pwr_on_positive_edge_flag));
  printf("  power key negative edge: %s\n",
      YesNo(irq1.pwr_on_negative_edge_flag));
  printf("  power key long press: %s\n",
      YesNo(irq1.pwr_on_long_press_flag));
  printf("  power key short press: %s\n",
      YesNo(irq1.pwr_on_short_press_flag));
  printf("  battery removed: %s\n", YesNo(irq1.battery_remove_flag));
  printf("  battery inserted: %s\n", YesNo(irq1.battery_insert_flag));
  printf("  vbus removed: %s\n", YesNo(irq1.vbus_remove_flag));
  printf("  vbus inserted: %s\n", YesNo(irq1.vbus_insert_flag));

  printf("latched irq status2:\n");
  printf("  battery over voltage: %s\n",
      YesNo(irq2.battery_over_voltage_flag));
  printf("  charger safety timer expired: %s\n",
      YesNo(irq2.charger_safety_timer_expire_flag));
  printf("  die over temperature level1: %s\n",
      YesNo(irq2.die_over_temperature_level1_flag));
  printf("  charger started: %s\n", YesNo(irq2.charger_start_flag));
  printf("  battery charge done: %s\n",
      YesNo(irq2.battery_charge_done_flag));
  printf("  batfet over current: %s\n",
      YesNo(irq2.batfet_over_current_flag));
  printf("  watchdog expired: %s\n", YesNo(irq2.watchdog_expire_flag));

  printf("latched irq status3:\n");
  printf("  battery under temperature work: %s\n",
      YesNo(irq3.battery_under_temperature_work_flag));
  printf("  battery over temperature work: %s\n",
      YesNo(irq3.battery_over_temperature_work_flag));
  printf("  battery under temperature charge: %s\n",
      YesNo(irq3.battery_under_temperature_charge_flag));
  printf("  battery over temperature charge: %s\n",
      YesNo(irq3.battery_over_temperature_charge_flag));
  printf("  battery over temperature quit: %s\n",
      YesNo(irq3.battery_over_temperature_quit_flag));
  printf("  bc1.2 result changed: %s\n",
      YesNo(irq3.bc1_2_detect_result_change_flag));
  printf("  bc1.2 detect finished: %s\n",
      YesNo(irq3.bc1_2_detect_finished_flag));

  if (!axp517.ClearAllIrq()) {
    printf("  clear latched irq failed\n");
  }
}

void PrintBatteryGauge(cpp_bus_driver::Axp517& axp517) {
  printf("battery gauge:\n");
  printf("  level: %u %%\n", axp517.GetBatteryLevel());
  printf("  health: %u %%\n", axp517.GetBatteryHealth());
  printf("  temperature: %d C\n",
      axp517.GetBatteryTemperatureCelsius());
}

void PrintAdcInfo(cpp_bus_driver::Axp517& axp517,
    cpp_bus_driver::Axp517::BatteryCurrentDirection direction) {
  printf("adc data:\n");
  printf("  battery voltage: %u mV\n", axp517.GetBatteryVoltage());
  printf("  battery current: %.2f mA\n", axp517.GetBatteryCurrent());
  printf("  ts voltage: %.2f mV\n", axp517.GetTsVoltage());
  printf("  vbus voltage: %u mV\n", axp517.GetVbusVoltage());
  printf("  vbus current: %u mA\n", axp517.GetVbusCurrent());

  if (axp517.SetAdcDataSelect(
          cpp_bus_driver::Axp517::AdcData::kSystemVoltage)) {
    printf("  system voltage: %u mV\n", axp517.GetSystemVoltage());
  } else {
    printf("  system voltage: select failed\n");
  }

  if (axp517.SetAdcDataSelect(
          cpp_bus_driver::Axp517::AdcData::kChipTemperatureCelsius)) {
    printf("  chip die junction temperature: %.2f C\n",
        axp517.GetChipDieJunctionTemperatureCelsius());
  } else {
    printf("  chip die junction temperature: select failed\n");
  }

  switch (direction) {
    case cpp_bus_driver::Axp517::BatteryCurrentDirection::kCharge:
      if (axp517.SetAdcDataSelect(
              cpp_bus_driver::Axp517::AdcData::kChargingCurrent)) {
        printf("  charging current: %.2f mA\n", axp517.GetChargingCurrent());
      } else {
        printf("  charging current: select failed\n");
      }
      break;
    case cpp_bus_driver::Axp517::BatteryCurrentDirection::kDischarge:
      if (axp517.SetAdcDataSelect(
              cpp_bus_driver::Axp517::AdcData::kDischargeCurrent)) {
        printf("  discharging current: %.2f mA\n",
            axp517.GetDischargingCurrent());
      } else {
        printf("  discharging current: select failed\n");
      }
      break;
    case cpp_bus_driver::Axp517::BatteryCurrentDirection::kStandby:
      printf("  charge/discharge current: standby\n");
      break;
    default:
      printf("  charge/discharge current: invalid direction\n");
      break;
  }
}

void PrintBc12Info(cpp_bus_driver::Axp517& axp517) {
  cpp_bus_driver::Axp517::BcDetectResult result;
  if (axp517.GetBc12DetectResult(result)) {
    printf("bc1.2 detect result: %s\n", BcDetectResultName(result));
  } else {
    printf("bc1.2 detect result: invalid or read failed\n");
  }
}

void PrintPowerInfo(cpp_bus_driver::Axp517& axp517) {
  printf("\n========== AXP517 power info ==========\n");
  printf("device id: %#X\n", axp517.GetDeviceId());
  PrintChipStatus(axp517);
  PrintBatteryGauge(axp517);

  cpp_bus_driver::Axp517::ChipStatus1 status1;
  if (axp517.GetChipStatus1(status1)) {
    PrintAdcInfo(axp517, status1.battery_current_direction);
  } else {
    PrintAdcInfo(
        axp517, cpp_bus_driver::Axp517::BatteryCurrentDirection::kInvalid);
  }

  PrintBc12Info(axp517);
  PrintIrqStatus(axp517);
}

extern "C" void app_main(void) {
  printf("Ciallo\n");

  auto& driver = lilygo_device_driver::TDisplayP4AirDriver::GetInstance();
  driver.Init();
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
  axp517->SetBc12DetectEnable(true);
  axp517->ClearAllIrq();

  while (1) {
    PrintPowerInfo(*axp517);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
