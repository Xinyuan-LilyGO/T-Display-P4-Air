/*
 * @Description: deep_sleep
 * @Author: LILYGO_L
 * @Date: 2026-03-31 09:24:53
 * @LastEditTime: 2026-05-24 16:30:00
 * @License: GPL 3.0
 */
#include "../common/t_display_p4_air_example.h"

#include "driver/uart.h"
#include "esp_sleep.h"
#include "light_sleep_example.h"

namespace {

constexpr bool kEnableTimerWakeup = false;
constexpr bool kEnableGpioWakeup = false;
constexpr uint64_t kTimerWakeupUs = 10ULL * 1000ULL * 1000ULL;

const char* WakeupReasonName(esp_sleep_wakeup_cause_t cause) {
  switch (cause) {
    case ESP_SLEEP_WAKEUP_TIMER:
      return "timer";
    case ESP_SLEEP_WAKEUP_GPIO:
    case ESP_SLEEP_WAKEUP_EXT0:
    case ESP_SLEEP_WAKEUP_EXT1:
      return "pin";
    default:
      return "other";
  }
}

void PrintWakeupReason() {
  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  printf("Wake up from deep sleep, reason: %s\n", WakeupReasonName(cause));

  if (cause == ESP_SLEEP_WAKEUP_GPIO || cause == ESP_SLEEP_WAKEUP_EXT0 ||
      cause == ESP_SLEEP_WAKEUP_EXT1) {
    example_wait_gpio_inactive();
  }
}

void RegisterWakeupSource() {
  if (kEnableTimerWakeup) {
    esp_err_t result = esp_sleep_enable_timer_wakeup(kTimerWakeupUs);
    if (result != ESP_OK) {
      printf("esp_sleep_enable_timer_wakeup failed: %s\n",
          esp_err_to_name(result));
    }
  }

  if (kEnableGpioWakeup) {
    esp_err_t result = example_register_gpio_wakeup();
    if (result != ESP_OK) {
      printf("example_register_gpio_wakeup failed: %s\n",
          esp_err_to_name(result));
    }
  }
}

void PrepareOptionalDevices(t_display_p4_air_example::Driver& driver) {
  driver.InitLr1121();
  driver.InitNrf9151();

  if (!driver.InitSdmmc(t_display_p4_air_example::device::sd::kBasePath)) {
    printf("InitSdmmc failed\n");
  }
}

}  // namespace

extern "C" void app_main(void) {
  printf("Ciallo\n");
  PrintWakeupReason();

  auto& driver = t_display_p4_air_example::InitDriver();
  PrepareOptionalDevices(driver);

  printf("Device is awake. Will enter deep sleep in 5 seconds...\n");
  vTaskDelay(pdMS_TO_TICKS(5000));

  printf("device sleep start\n");
  if (!driver.SetSleep(
          t_display_p4_air_example::Driver::SleepLevel::kPowerOff, true)) {
    printf("SetSleep failed\n");
  }

  RegisterWakeupSource();

  printf("Entering deep sleep for power measurement...\n");
  uart_wait_tx_idle_polling((uart_port_t)CONFIG_ESP_CONSOLE_UART_NUM);
  esp_deep_sleep_start();
}
