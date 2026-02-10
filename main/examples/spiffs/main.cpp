/*
 * @Description: None
 * @Author: LILYGO_L
 * @Date: 2026-02-02 17:40:30
 * @LastEditTime: 2026-02-10 15:53:59
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

    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true};

    // Use settings defined above to initialize and mount SPIFFS filesystem.
    // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            printf("failed to mount or format filesystem (error code: %#x)\n", ret);
        }
        else if (ret == ESP_ERR_NOT_FOUND)
        {
            printf("failed to find spiffs partition (error code: %#x)\n", ret);
        }
        else
        {
            printf("failed to initialize spiffs (error code: %#x)\n", ret);
        }
        return;
    }

#ifdef CONFIG_EXAMPLE_SPIFFS_CHECK_ON_START
    printf("performing spiffs_check().\n");
    ret = esp_spiffs_check(conf.partition_label);
    if (ret != ESP_OK)
    {
        printf("spiffs_check() failed (error code: %#x)\n", ret);
        return;
    }
    else
    {
        printf("spiffs_check() successful\n");
    }
#endif

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK)
    {
        printf("failed to get spiffs partition information (error code: %#x). formatting...\n", ret);
        esp_spiffs_format(conf.partition_label);
        return;
    }
    else
    {
        printf("partition size: total: %d, used: %d\n", total, used);
    }

    // Check consistency of reported partition size info.
    if (used > total)
    {
        printf("number of used bytes cannot be larger than total. performing spiffs_check().\n");
        ret = esp_spiffs_check(conf.partition_label);
        // Could be also used to mend broken files, to clean unreferenced pages, etc.
        // More info at https://github.com/pellepl/spiffs/wiki/FAQ#powerlosses-contd-when-should-i-run-spiffs_check
        if (ret != ESP_OK)
        {
            printf("spiffs_check() failed (error code: %#x)\n", ret);
            return;
        }
        else
        {
            printf("spiffs_check() successful\n");
        }
    }

    // Use POSIX and C standard library functions to work with files.
    // First create a file.
    printf("opening file\n");
    FILE *f = fopen("/spiffs/hello.txt", "w");
    if (f == NULL)
    {
        printf("failed to open file for writing (error code: %#x)\n", ret);
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
        printf("rename failed (error code: %#x)\n", ret);
        return;
    }

    // Open renamed file for reading
    printf("reading file\n");
    f = fopen("/spiffs/foo.txt", "r");
    if (f == NULL)
    {
        printf("failed to open file for reading (error code: %#x)\n", ret);
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