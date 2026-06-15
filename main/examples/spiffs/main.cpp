/*
 * @Description: spiffs
 * @Author: LILYGO_L
 * @Date: 2026-02-02 17:40:30
 * @LastEditTime: 2026-05-24 16:30:00
 * @License: GPL 3.0
 */
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>

#include "lilygo_device_driver_library.h"
#include "esp_spiffs.h"

extern "C" void app_main(void) {
  printf("Ciallo\n");

  auto& driver = lilygo_device_driver::TDisplayP4AirDriver::GetInstance();
  driver.Init();

  printf("Initializing spiffs\n");
  esp_vfs_spiffs_conf_t conf;
  if (!driver.InitSpiffs(
          lilygo_device_driver::t_display_p4_air::device::spiffs::kBasePath, conf)) {
    printf("InitSpiffs failed\n");
    return;
  }

  printf("Opening file\n");
  FILE* file = fopen("/spiffs/hello.txt", "w");
  if (file == NULL) {
    printf("Failed to open file for writing\n");
    return;
  }
  fprintf(file, "Hello World!\n");
  fclose(file);
  printf("File written\n");

  struct stat st;
  if (stat("/spiffs/foo.txt", &st) == 0) {
    unlink("/spiffs/foo.txt");
  }

  printf("Renaming file\n");
  if (rename("/spiffs/hello.txt", "/spiffs/foo.txt") != 0) {
    printf("Rename failed\n");
    return;
  }

  printf("Reading file\n");
  file = fopen("/spiffs/foo.txt", "r");
  if (file == NULL) {
    printf("Failed to open file for reading\n");
    return;
  }

  char line[64] = {};
  fgets(line, sizeof(line), file);
  fclose(file);

  char* pos = strchr(line, '\n');
  if (pos != nullptr) {
    *pos = '\0';
  }
  printf("Read from file: '%s'\n", line);

  esp_vfs_spiffs_unregister(conf.partition_label);
  printf("Spiffs unmounted\n");

  while (1) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
