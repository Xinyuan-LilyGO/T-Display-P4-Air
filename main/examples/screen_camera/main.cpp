/*
 * @Description: screen_camera
 * @Author: LILYGO_L
 * @Date: 2026-02-28 09:19:50
 * @LastEditTime: 2026-05-24 16:30:00
 * @License: GPL 3.0
 */
#include "../common/t_display_p4_air_example.h"

#include <cstring>
#include <memory>

#include "app_video.h"
#include "driver/ppa.h"
#include "esp_private/esp_cache_private.h"
#include "esp_video_init.h"

namespace {

ppa_client_handle_t g_ppa_srm_handle = nullptr;
size_t g_data_cache_line_size = 0;
int32_t g_video_cam_fd0 = -1;
int64_t g_start_time = 0;
bool g_camera_init_flag = false;
cpp_bus_driver::Hi8561* g_screen = nullptr;

size_t AlignUp(size_t num, size_t align) {
  return (num + (align - 1)) & ~(align - 1);
}

ppa_srm_color_mode_t PpaColorModeFromBits(int bits_per_pixel) {
  if (bits_per_pixel == 24) {
    return ppa_srm_color_mode_t::PPA_SRM_COLOR_MODE_RGB888;
  }
  return ppa_srm_color_mode_t::PPA_SRM_COLOR_MODE_RGB565;
}

ppa_srm_color_mode_t CameraPpaColorMode() {
  if (t_display_p4_air_example::device::camera::kType ==
      t_display_p4_air_example::device::CameraType::kOv5645) {
    return ppa_srm_color_mode_t::PPA_SRM_COLOR_MODE_RGB565;
  }
  return PpaColorModeFromBits(
      t_display_p4_air_example::device::camera::kBitsPerPixel);
}

video_fmt_t CameraVideoFormat() {
  if (t_display_p4_air_example::device::camera::kType ==
      t_display_p4_air_example::device::CameraType::kOv5645) {
    return video_fmt_t::APP_VIDEO_FMT_RGB565;
  }
  if (t_display_p4_air_example::device::camera::kBitsPerPixel == 24) {
    return video_fmt_t::APP_VIDEO_FMT_RGB888;
  }
  return video_fmt_t::APP_VIDEO_FMT_RGB565;
}

void FillWhiteScreen() {
  size_t screen_size = t_display_p4_air_example::kScreenWidth *
                       t_display_p4_air_example::kScreenHeight *
                       t_display_p4_air_example::kScreenBitsPerPixel / 8;
  void* white_buffer =
      heap_caps_aligned_calloc(16, 1, screen_size, MALLOC_CAP_SPIRAM);
  if (white_buffer == nullptr) {
    return;
  }

  if (t_display_p4_air_example::kScreenBitsPerPixel == 16) {
    auto* pixels = static_cast<uint16_t*>(white_buffer);
    for (size_t i = 0;
         i < t_display_p4_air_example::kScreenWidth *
                 t_display_p4_air_example::kScreenHeight;
         i++) {
      pixels[i] = 0xFFFF;
    }
  } else {
    memset(white_buffer, 0xFF, screen_size);
  }

  if (!g_screen->SendColorStreamCoordinate(0, 0,
          t_display_p4_air_example::kScreenWidth,
          t_display_p4_air_example::kScreenHeight, white_buffer)) {
    printf("SendColorStreamCoordinate white failed\n");
  }
  heap_caps_free(white_buffer);
}

void CameraVideoFrameOperation(uint8_t* camera_buf, uint8_t camera_buf_index,
    uint32_t camera_buf_hes, uint32_t camera_buf_ves, size_t camera_buf_len) {
  static int32_t fps_count = 0;
  fps_count++;
  if (fps_count == 50) {
    int64_t end_time = esp_timer_get_time();
    printf("fps: %f\n", 1000000.0 / ((end_time - g_start_time) / 50.0));
    g_start_time = end_time;
    fps_count = 0;
    printf("camera_buf_hes: %lu, camera_buf_ves: %lu, camera_buf_len: %zu KB\n",
        static_cast<unsigned long>(camera_buf_hes),
        static_cast<unsigned long>(camera_buf_ves), camera_buf_len / 1024);
  }

  auto& driver = lilygo_device_driver::TDisplayP4AirDriver::GetInstance();
  if (!driver.status().hi8561.init_flag) {
    return;
  }

  uint32_t input_img_block_width =
      camera_buf_hes > t_display_p4_air_example::kScreenWidth
          ? (camera_buf_hes - t_display_p4_air_example::kScreenWidth) / 2
          : 0;
  uint32_t input_img_block_height = 0;
  uint32_t input_img_width = t_display_p4_air_example::kScreenWidth;
  uint32_t input_img_height = camera_buf_ves;
  uint32_t output_img_width = input_img_width;
  uint32_t output_img_height = input_img_height;

  size_t output_buffer_size = output_img_width * output_img_height *
                              (t_display_p4_air_example::kScreenBitsPerPixel /
                                  8);
  auto output_buffer = std::unique_ptr<uint8_t[], void (*)(uint8_t*)>(
      static_cast<uint8_t*>(heap_caps_aligned_calloc(g_data_cache_line_size, 1,
          output_buffer_size, MALLOC_CAP_SPIRAM)),
      [](uint8_t* data) { heap_caps_free(data); });
  if (output_buffer == nullptr) {
    printf("heap_caps_aligned_calloc failed\n");
    return;
  }

  ppa_srm_oper_config_t srm_config = {
      .in =
          {
              .buffer = camera_buf,
              .pic_w = camera_buf_hes,
              .pic_h = camera_buf_ves,
              .block_w = input_img_width,
              .block_h = input_img_height,
              .block_offset_x = input_img_block_width,
              .block_offset_y = input_img_block_height,
              .srm_cm = CameraPpaColorMode(),
          },
      .out =
          {
              .buffer = output_buffer.get(),
              .buffer_size =
                  AlignUp(output_buffer_size, g_data_cache_line_size),
              .pic_w = output_img_width,
              .pic_h = output_img_height,
              .block_offset_x = 0,
              .block_offset_y = 0,
              .srm_cm = PpaColorModeFromBits(
                  t_display_p4_air_example::kScreenBitsPerPixel),
          },
      .rotation_angle = PPA_SRM_ROTATION_ANGLE_0,
      .scale_x = 1,
      .scale_y = 1,
      .mirror_x = false,
      .mirror_y = t_display_p4_air_example::kScreenRotationDirection == 0,
      .rgb_swap = false,
      .byte_swap = false,
      .mode = PPA_TRANS_MODE_BLOCKING,
  };

  esp_err_t result = ppa_do_scale_rotate_mirror(g_ppa_srm_handle, &srm_config);
  if (result != ESP_OK) {
    printf("ppa_do_scale_rotate_mirror failed (error code: %#X)\n", result);
    return;
  }

  int y_start =
      (t_display_p4_air_example::kScreenHeight - output_img_height) / 2;
  if (!g_screen->SendColorStreamCoordinate(0, y_start, output_img_width,
          output_img_height + y_start, output_buffer.get())) {
    printf("SendColorStreamCoordinate failed\n");
  }
}

bool AppVideoInit() {
  ppa_client_config_t ppa_srm_config = {
      .oper_type = PPA_OPERATION_SRM,
  };
  esp_err_t result = ppa_register_client(&ppa_srm_config, &g_ppa_srm_handle);
  if (result != ESP_OK) {
    printf("ppa_register_client failed (error code: %#X)\n", result);
    return false;
  }

  result = esp_cache_get_alignment(MALLOC_CAP_SPIRAM, &g_data_cache_line_size);
  if (result != ESP_OK) {
    printf("esp_cache_get_alignment failed (error code: %#X)\n", result);
    return false;
  }

  auto& driver = lilygo_device_driver::TDisplayP4AirDriver::GetInstance();
  esp_video_init_csi_config_t csi_config = {
      .sccb_config =
          {
              .init_sccb = false,
              .i2c_handle = driver.bus().sgm38121_i2c_bus->bus_handle(),
              .freq = static_cast<uint32_t>(100000),
          },
      .reset_pin = gpio_num_t::GPIO_NUM_NC,
      .pwdn_pin = gpio_num_t::GPIO_NUM_NC,
      .dont_init_ldo = true,
  };
  esp_video_init_config_t cam_config = {
      .csi = &csi_config,
  };

  result = esp_video_init(&cam_config);
  if (result != ESP_OK) {
    printf("esp_video_init failed (error code: %#X)\n", result);
    return false;
  }

  g_video_cam_fd0 =
      app_video_open(ESP_VIDEO_MIPI_CSI_DEVICE_NAME, CameraVideoFormat());
  if (g_video_cam_fd0 < 0) {
    printf("video cam open failed (g_video_cam_fd0: %ld)\n",
        static_cast<long>(g_video_cam_fd0));
    return false;
  }

  result = app_video_set_bufs(
      g_video_cam_fd0, t_display_p4_air_example::kCameraBufferCount, NULL);
  if (result != ESP_OK) {
    printf("app_video_set_bufs failed (error code: %#X)\n", result);
    return false;
  }

  result = app_video_register_frame_operation_cb(CameraVideoFrameOperation);
  if (result != ESP_OK) {
    printf("app_video_register_frame_operation_cb failed: %#X\n", result);
    return false;
  }

  result = app_video_stream_task_start(g_video_cam_fd0, 0);
  if (result != ESP_OK) {
    printf("app_video_stream_task_start failed (error code: %#X)\n", result);
    return false;
  }
  app_video_stream_task_stop(g_video_cam_fd0);

  return true;
}

}  // namespace

extern "C" void app_main(void) {
  printf("Ciallo\n");

  auto& driver = t_display_p4_air_example::InitDriver();
  g_screen = driver.chip().hi8561.get();

  g_camera_init_flag = AppVideoInit();
  printf("AppVideoInit %s\n", g_camera_init_flag ? "success" : "failed");

  FillWhiteScreen();
  driver.chip().hi8561_backlight->StartGradientTime(100, 500);

  if (g_camera_init_flag) {
    esp_err_t result = app_video_set_bufs(
        g_video_cam_fd0, t_display_p4_air_example::kCameraBufferCount, NULL);
    if (result != ESP_OK) {
      printf("app_video_set_bufs failed (error code: %#X)\n", result);
    }

    result = app_video_stream_task_start(g_video_cam_fd0, 0);
    if (result != ESP_OK) {
      printf("app_video_stream_task_start failed (error code: %#X)\n", result);
    } else {
      g_start_time = esp_timer_get_time();
    }
  }

  while (1) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
