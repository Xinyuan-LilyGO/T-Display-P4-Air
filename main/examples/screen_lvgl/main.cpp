/*
 * @Description: screen_lvgl
 * @Author: LILYGO_L
 * @Date: 2025-06-13 11:31:49
 * @LastEditTime: 2026-05-24 16:30:00
 * @License: GPL 3.0
 */
#include "../common/t_display_p4_air_example.h"

#include "lvgl.h"

namespace {

constexpr int kLvglTickPeriodMs = 1;

static _lock_t g_lvgl_api_lock;
cpp_bus_driver::Hi8561* g_screen = nullptr;

extern "C" void example_lvgl_demo_ui(lv_display_t* disp);

lv_color_format_t LvglColorFormat() {
  if (t_display_p4_air_example::kScreenBitsPerPixel == 24) {
    return lv_color_format_t::LV_COLOR_FORMAT_RGB888;
  }
  return lv_color_format_t::LV_COLOR_FORMAT_RGB565;
}

uint16_t Rgb888ToRgb565(uint8_t red, uint8_t green, uint8_t blue) {
  return ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3);
}

void ColorBarBuffer(void* buffer, int width, int height) {
  const uint8_t colors[][3] = {
      {0xFF, 0x00, 0x00}, {0xFF, 0x7F, 0x00}, {0xFF, 0xFF, 0x00},
      {0x00, 0xFF, 0x00}, {0x00, 0xFF, 0xFF}, {0x00, 0x00, 0xFF},
      {0x80, 0x00, 0xFF}, {0xFF, 0xFF, 0xFF},
  };
  constexpr int kColorCount = sizeof(colors) / sizeof(colors[0]);
  int block_height = height / kColorCount;
  if (block_height < 1) {
    block_height = 1;
  }

  if (t_display_p4_air_example::kScreenBitsPerPixel == 16) {
    auto* pixels = static_cast<uint16_t*>(buffer);
    for (int y = 0; y < height; y++) {
      int index = y / block_height;
      if (index >= kColorCount) {
        index = kColorCount - 1;
      }
      uint16_t color =
          Rgb888ToRgb565(colors[index][0], colors[index][1], colors[index][2]);
      for (int x = 0; x < width; x++) {
        pixels[y * width + x] = color;
      }
    }
  } else {
    auto* pixels = static_cast<uint8_t*>(buffer);
    for (int y = 0; y < height; y++) {
      int index = y / block_height;
      if (index >= kColorCount) {
        index = kColorCount - 1;
      }
      for (int x = 0; x < width; x++) {
        size_t offset = (y * width + x) * 3;
        pixels[offset + 0] = colors[index][0];
        pixels[offset + 1] = colors[index][1];
        pixels[offset + 2] = colors[index][2];
      }
    }
  }
}

void MyTouchpadRead(lv_indev_t* indev, lv_indev_data_t* data) {
  data->state = LV_INDEV_STATE_REL;
}

void LvglUiTask(void* arg) {
  printf("LvglUiTask start\n");

  while (1) {
    _lock_acquire(&g_lvgl_api_lock);
    uint32_t time_till_next_ms = lv_timer_handler();
    _lock_release(&g_lvgl_api_lock);

    if (time_till_next_ms < 10) {
      time_till_next_ms = 10;
    }
    usleep(1000 * time_till_next_ms);
  }
}

void InitLvgl() {
  printf("InitLvgl\n");
  lv_init();

  lv_display_t* display = lv_display_create(
      t_display_p4_air_example::kScreenWidth,
      t_display_p4_air_example::kScreenHeight);
  lv_display_set_user_data(display, g_screen);
  lv_display_set_color_format(display, LvglColorFormat());

  size_t draw_buffer_size = t_display_p4_air_example::kScreenWidth *
                            t_display_p4_air_example::kScreenHeight *
                            sizeof(lv_color_t);
  void* buffer = heap_caps_malloc(draw_buffer_size, MALLOC_CAP_SPIRAM);
  assert(buffer);
  lv_display_set_buffers(
      display, buffer, NULL, draw_buffer_size, LV_DISPLAY_RENDER_MODE_PARTIAL);

  lv_display_set_flush_cb(
      display, [](lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
        auto* screen =
            static_cast<cpp_bus_driver::Hi8561*>(lv_display_get_user_data(disp));
        screen->SendColorStreamCoordinate(area->x1, area->y1, area->x2 + 1,
            area->y2 + 1, px_map);
      });

  lv_indev_t* indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, MyTouchpadRead);

  esp_lcd_dpi_panel_event_callbacks_t cbs = {
      .on_color_trans_done =
          [](esp_lcd_panel_handle_t panel,
              esp_lcd_dpi_panel_event_data_t* edata,
              void* user_ctx) -> bool {
        lv_display_flush_ready(static_cast<lv_display_t*>(user_ctx));
        return false;
      },
      .on_refresh_done =
          [](esp_lcd_panel_handle_t panel,
              esp_lcd_dpi_panel_event_data_t* edata,
              void* user_ctx) -> bool { return false; },
  };

  auto& driver = lilygo_device_driver::TDisplayP4AirDriver::GetInstance();
  esp_err_t result = esp_lcd_dpi_panel_register_event_callbacks(
      driver.bus().screen_mipi_bus->device_handle(), &cbs, display);
  if (result != ESP_OK) {
    printf("esp_lcd_dpi_panel_register_event_callbacks failed: %#X\n", result);
  }

  const esp_timer_create_args_t lvgl_tick_timer_args = {
      .callback = [](void* arg) { lv_tick_inc(kLvglTickPeriodMs); },
      .arg = nullptr,
      .dispatch_method = ESP_TIMER_TASK,
      .name = "lvgl_tick",
      .skip_unhandled_events = false,
  };
  esp_timer_handle_t lvgl_tick_timer = nullptr;
  result = esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer);
  if (result == ESP_OK) {
    result = esp_timer_start_periodic(
        lvgl_tick_timer, kLvglTickPeriodMs * 1000);
  }
  if (result != ESP_OK) {
    printf("lvgl tick timer init failed: %#X\n", result);
  }

  _lock_acquire(&g_lvgl_api_lock);
  example_lvgl_demo_ui(display);
  _lock_release(&g_lvgl_api_lock);
}

}  // namespace

extern "C" void app_main(void) {
  printf("Ciallo\n");

  auto& driver = t_display_p4_air_example::InitDriver();
  g_screen = driver.chip().hi8561.get();

  size_t screen_size = t_display_p4_air_example::kScreenWidth *
                       t_display_p4_air_example::kScreenHeight *
                       t_display_p4_air_example::kScreenBitsPerPixel / 8;
  void* color_buffer =
      heap_caps_aligned_calloc(16, 1, screen_size, MALLOC_CAP_SPIRAM);
  if (color_buffer != nullptr) {
    ColorBarBuffer(color_buffer, t_display_p4_air_example::kScreenWidth,
        t_display_p4_air_example::kScreenHeight);
    if (!g_screen->SendColorStreamCoordinate(0, 0,
            t_display_p4_air_example::kScreenWidth,
            t_display_p4_air_example::kScreenHeight, color_buffer)) {
      printf("SendColorStreamCoordinate failed\n");
    }
    heap_caps_free(color_buffer);
  }

  driver.chip().hi8561_backlight->StartGradientTime(100, 500);
  vTaskDelay(pdMS_TO_TICKS(3000));

  InitLvgl();
  xTaskCreate(LvglUiTask, "LvglUiTask", 100 * 1024, NULL, 1, NULL);

  vTaskDelay(pdMS_TO_TICKS(1000));
  g_screen->SetMirror(cpp_bus_driver::Hi8561::MirrorMode::kHorizontal);
  vTaskDelay(pdMS_TO_TICKS(1000));
  g_screen->SetMirror(cpp_bus_driver::Hi8561::MirrorMode::kVertical);
  vTaskDelay(pdMS_TO_TICKS(1000));
  g_screen->SetMirror(cpp_bus_driver::Hi8561::MirrorMode::kHorizontalVertical);
  vTaskDelay(pdMS_TO_TICKS(1000));
  g_screen->SetMirror(cpp_bus_driver::Hi8561::MirrorMode::kOff);
  vTaskDelay(pdMS_TO_TICKS(1000));

  g_screen->SetColorOrder(cpp_bus_driver::Hi8561::ColorOrder::kBgr);
  vTaskDelay(pdMS_TO_TICKS(1000));
  g_screen->SetColorOrder(cpp_bus_driver::Hi8561::ColorOrder::kRgb);
  vTaskDelay(pdMS_TO_TICKS(1000));

  g_screen->SetInversion(true);
  vTaskDelay(pdMS_TO_TICKS(1000));
  g_screen->SetInversion(false);
  vTaskDelay(pdMS_TO_TICKS(1000));

  g_screen->SetScreenOff(true);
  g_screen->SetSleep(true);
  vTaskDelay(pdMS_TO_TICKS(1000));
  g_screen->SetSleep(false);
  g_screen->SetScreenOff(false);
}
