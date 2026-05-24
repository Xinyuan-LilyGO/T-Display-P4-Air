/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "../common/t_display_p4_air_example.h"

namespace {

// 使用 BOOT 按键作为低电平唤醒源
constexpr gpio_num_t kGpioWakeupNum =
    static_cast<gpio_num_t>(t_display_p4_air_example::gpio::kChipBoot);
constexpr int kGpioWakeupLevel = 0;

static const char* TAG = "gpio_wakeup";

}  // namespace

void example_wait_gpio_inactive(void) {
  printf("Waiting for GPIO%d to go high...\n",
      static_cast<int>(kGpioWakeupNum));
  while (gpio_get_level(kGpioWakeupNum) == kGpioWakeupLevel) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

esp_err_t example_register_gpio_wakeup(void) {
  gpio_config_t config = {
      .pin_bit_mask = 1ULL << static_cast<int>(kGpioWakeupNum),
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
#if SOC_GPIO_SUPPORT_PIN_HYS_FILTER
      .hys_ctrl_mode = GPIO_HYS_SOFT_ENABLE,
#endif
  };
  ESP_RETURN_ON_ERROR(gpio_config(&config), TAG, "Initialize GPIO%d failed",
                      static_cast<int>(kGpioWakeupNum));

  ESP_RETURN_ON_ERROR(gpio_wakeup_enable(
                          kGpioWakeupNum,
                          kGpioWakeupLevel == 0 ? GPIO_INTR_LOW_LEVEL
                                                : GPIO_INTR_HIGH_LEVEL),
                      TAG, "Enable gpio wakeup failed");
  ESP_RETURN_ON_ERROR(esp_sleep_enable_gpio_wakeup(), TAG,
                      "Configure gpio as wakeup source failed");

  // 等待按键松开，避免配置完成后立即被再次唤醒
  example_wait_gpio_inactive();
  ESP_LOGI(TAG, "gpio wakeup source is ready");

  return ESP_OK;
}
