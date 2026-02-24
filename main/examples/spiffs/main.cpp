/*
 * @Description: None
 * @Author: LILYGO_L
 * @Date: 2026-02-02 17:40:30
 * @LastEditTime: 2026-02-24 16:34:09
 * @License: GPL 3.0
 */
#include "sdkconfig.h"
#include "lilygo_device_driver_library.h"
#include "cpp_bus_driver_library.h"
#include "esp_spiffs.h"
#include <sys/stat.h>

auto Axp517_Iic_Bus = std::make_shared<Cpp_Bus_Driver::Hardware_Iic_1>(AXP517_SDA, AXP517_SCL, I2C_NUM_0);

auto Axp517 = std::make_unique<Cpp_Bus_Driver::Axp517>(Axp517_Iic_Bus, AXP517_IIC_ADDRESS);

extern "C" void app_main(void)
{
    printf("Ciallo\n");

    Axp517->begin();

    printf("initializing spiffs\n");

    esp_vfs_spiffs_conf_t conf;
    Lilygo_Device_Driver::Spiffs_Init(SPIFFS_BASE_PATH, conf);

    // Use POSIX and C standard library functions to work with files.
    // First create a file.
    printf("opening file\n");
    FILE *f = fopen("/spiffs/hello.txt", "w");
    if (f == NULL)
    {
        printf("fail to open file for writing\n");
        return;
    }
    fprintf(f, "Hello World!\n");
    fclose(f);
    printf("file written\n");

    // Check if destination file exists before renaming
    struct stat st;
    if (stat("/spiffs/foo.txt", &st) == 0)
    {
        // Delete it if it exists
        unlink("/spiffs/foo.txt");
    }

    // Rename original file
    printf("renaming file\n");
    if (rename("/spiffs/hello.txt", "/spiffs/foo.txt") != 0)
    {
        printf("rename fail\n");
        return;
    }

    // Open renamed file for reading
    printf("reading file\n");
    f = fopen("/spiffs/foo.txt", "r");
    if (f == NULL)
    {
        printf("fail to open file for reading\n");
        return;
    }
    char line[64];
    fgets(line, sizeof(line), f);
    fclose(f);
    // strip newline
    char *pos = strchr(line, '\n');
    if (pos)
    {
        *pos = '\0';
    }
    printf("read from file: '%s'\n", line);

    // All done, unmount partition and disable SPIFFS
    esp_vfs_spiffs_unregister(conf.partition_label);
    printf("spiffs unmounted\n");

    while (1)
    {
        printf("Ciallo\n");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}