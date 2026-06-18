/*
 * @Description: screen_lvgl_touch_draw
 * @Author: LILYGO_L
 * @Date: 2025-06-13 11:35:38
 * @LastEditTime: 2026-06-18 17:58:55
 * @License: GPL 3.0
 */
#include "lilygo_device_driver_library.h"

#include <ctime>
#include <vector>

#include "lvgl.h"

namespace {

constexpr int kLvglTickPeriodMs = 1;

size_t g_cycle_time = 0;
std::vector<uint16_t> g_lvgl_draw_x_data;
std::vector<uint16_t> g_lvgl_draw_y_data;

cpp_bus_driver::Hi8561* g_screen = nullptr;
cpp_bus_driver::Hi8561Touch* g_touch = nullptr;
lv_obj_t* g_canvas = nullptr;
lv_layer_t g_layer;
lv_point_t g_point;
time_t g_last_touch_time = 0;
bool g_need_clear_lock_flag = false;

lv_color_format_t LvglColorFormat() {
  auto& driver = lilygo_device_driver::TDisplayP4AirDriver::GetInstance();
  if (driver.screen_info().bits_per_pixel == 24) {
    return lv_color_format_t::LV_COLOR_FORMAT_RGB888;
  }
  return lv_color_format_t::LV_COLOR_FORMAT_RGB565;
}

void LvglUiTask(void* arg) {
  printf("LvglUiTask start\n");

  while (1) {
    time_t current_time = time(NULL);
    if ((current_time - g_last_touch_time > 5) && g_need_clear_lock_flag) {
      lv_canvas_fill_bg(g_canvas, lv_color_hex3(0xCCC), LV_OPA_COVER);
      g_last_touch_time = current_time;
      g_lvgl_draw_x_data.clear();
      g_lvgl_draw_y_data.clear();
      g_need_clear_lock_flag = false;
    }

    lv_timer_handler();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void MyTouchpadRead(lv_indev_t* indev, lv_indev_data_t* data) {
  cpp_bus_driver::Hi8561Touch::TouchPoint tp;
  if (g_touch->GetSingleTouchPoint(tp)) {
    data->state = LV_INDEV_STATE_PR;
    data->point.x = tp.info[0].x;
    data->point.y = tp.info[0].y;
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
}

void DrawPoint(lv_event_t* e) {
  if (lv_event_get_code(e) == LV_EVENT_PRESSING) {
    lv_indev_t* indev = lv_indev_get_act();
    lv_indev_get_point(indev, &g_point);

    g_lvgl_draw_x_data.push_back(g_point.x);
    g_lvgl_draw_y_data.push_back(g_point.y);

    if (g_lvgl_draw_x_data.size() >= 2 && g_lvgl_draw_y_data.size() >= 2) {
      lv_draw_line_dsc_t dsc;
      lv_draw_line_dsc_init(&dsc);
      dsc.color = lv_palette_main(LV_PALETTE_RED);
      dsc.width = 4;
      dsc.round_end = 1;
      dsc.round_start = 1;
      dsc.p1.x = g_lvgl_draw_x_data[0];
      dsc.p1.y = g_lvgl_draw_y_data[0];
      dsc.p2.x = g_lvgl_draw_x_data[1];
      dsc.p2.y = g_lvgl_draw_y_data[1];
      lv_draw_line(&g_layer, &dsc);
      lv_canvas_finish_layer(g_canvas, &g_layer);

      g_lvgl_draw_x_data.erase(g_lvgl_draw_x_data.begin());
      g_lvgl_draw_y_data.erase(g_lvgl_draw_y_data.begin());
    }

    g_last_touch_time = time(NULL);
    g_need_clear_lock_flag = true;
  } else if (lv_event_get_code(e) == LV_EVENT_RELEASED) {
    g_lvgl_draw_x_data.clear();
    g_lvgl_draw_y_data.clear();
  }
}

void CreateCanvas() {
  auto& driver = lilygo_device_driver::TDisplayP4AirDriver::GetInstance();
  const auto& screen_info = driver.screen_info();
  size_t draw_buffer_size =
      screen_info.width * screen_info.height * screen_info.bits_per_pixel / 8;
  void* draw_buffer = heap_caps_malloc(draw_buffer_size, MALLOC_CAP_SPIRAM);

  g_canvas = lv_canvas_create(lv_screen_active());
  lv_canvas_set_buffer(g_canvas, draw_buffer, screen_info.width,
      screen_info.height, LvglColorFormat());
  lv_canvas_fill_bg(g_canvas, lv_color_hex3(0xCCC), LV_OPA_COVER);
  lv_obj_center(g_canvas);

  lv_canvas_init_layer(g_canvas, &g_layer);
  lv_draw_line_dsc_t dsc;
  lv_draw_line_dsc_init(&dsc);
  dsc.color = lv_palette_main(LV_PALETTE_RED);
  dsc.width = 4;
  dsc.round_end = 1;
  dsc.round_start = 1;
  dsc.p1.x = 15;
  dsc.p1.y = 15;
  dsc.p2.x = 35;
  dsc.p2.y = 10;
  lv_draw_line(&g_layer, &dsc);
  lv_canvas_finish_layer(g_canvas, &g_layer);

  lv_obj_add_event_cb(lv_screen_active(), DrawPoint, LV_EVENT_ALL, NULL);
}

void InitLvgl() {
  printf("InitLvgl\n");
  lv_init();

  auto& driver = lilygo_device_driver::TDisplayP4AirDriver::GetInstance();
  const auto& screen_info = driver.screen_info();
  lv_display_t* display = lv_display_create(
      screen_info.width, screen_info.height);
  lv_display_set_user_data(display, g_screen);
  lv_display_set_color_format(display, LvglColorFormat());

  size_t draw_buffer_size =
      screen_info.width * screen_info.height * screen_info.bits_per_pixel / 8;
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
}

}  // namespace

extern "C" void app_main(void) {
  printf("Ciallo\n");

  auto& driver = lilygo_device_driver::TDisplayP4AirDriver::GetInstance();
  driver.Init();
  
  if (!driver.status().hi8561.init_flag ||
      !driver.status().hi8561_touch.init_flag ||
      !driver.status().hi8561_backlight.init_flag) {
    printf("screen or touch init failed\n");
    return;
  }
  g_screen = driver.chip().hi8561.get();
  g_touch = driver.chip().hi8561_touch.get();
  if (g_screen == nullptr || g_touch == nullptr ||
      driver.chip().hi8561_backlight == nullptr) {
    printf("screen or touch driver is null\n");
    return;
  }

  InitLvgl();
  xTaskCreate(LvglUiTask, "LvglUiTask", 4 * 1024, NULL, 2, NULL);
  CreateCanvas();

  driver.chip().hi8561_backlight->StartGradientTime(100, 500);

  while (1) {
    if (esp_log_timestamp() > g_cycle_time) {
      cpp_bus_driver::Hi8561Touch::TouchPoint tp;
      if (g_touch->GetMultipleTouchPoint(tp)) {
        printf("Touch finger: %d edge touch flag: %d\n", tp.finger_count,
            tp.edge_touch_flag);
        for (uint8_t i = 0; i < tp.info.size(); i++) {
          printf("Touch num:[%d] x: %d y: %d p: %d\n", i + 1, tp.info[i].x,
              tp.info[i].y, tp.info[i].pressure_value);
        }
        g_cycle_time = esp_log_timestamp() + 1000;
      }
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
