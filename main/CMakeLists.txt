#这里选择你需要编译的项目
if(CONFIG_EXAMPLE_BUILD_IIC_SCAN)
    set(project_path "examples/iic_scan/")
elseif(CONFIG_EXAMPLE_BUILD_SCREEN_LVGL)
    set(project_path "examples/screen_lvgl/")
elseif(CONFIG_EXAMPLE_BUILD_XL9555)
    set(project_path "examples/xl9555/")
elseif(CONFIG_EXAMPLE_BUILD_AXP517)
    set(project_path "examples/axp517/")
elseif(CONFIG_EXAMPLE_BUILD_AW86224)
    set(project_path "examples/aw86224/")
elseif(CONFIG_EXAMPLE_BUILD_ES8388)
    set(project_path "examples/es8388/")
elseif(CONFIG_EXAMPLE_BUILD_SCREEN_LVGL_TOUCH_DRAW)
    set(project_path "examples/screen_lvgl_touch_draw/")
elseif(CONFIG_EXAMPLE_BUILD_ESP_HOSTED_MCU_SDIO_WIFI)
    set(project_path "examples/esp_hosted_mcu_sdio_wifi/")
elseif(CONFIG_EXAMPLE_BUILD_IPERF)
    set(project_path "examples/iperf/")
elseif(CONFIG_EXAMPLE_BUILD_ESP_HOSTED_MCU_SDIO_WIFI_HTTP_DOWNLOAD_FILE)
    set(project_path "examples/esp_hosted_mcu_sdio_wifi_http_download_file/")
elseif(CONFIG_EXAMPLE_BUILD_SPIFFS)
    set(project_path "examples/spiffs/")
elseif(CONFIG_EXAMPLE_BUILD_ES8388_SPIFFS_MP3)
    set(project_path "examples/es8388_spiffs_mp3/")
endif()
        
idf_component_register(SRC_DIRS ${project_path} 
                        INCLUDE_DIRS ${project_path})

if(CONFIG_EXAMPLE_BUILD_SPIFFS)
    # 配置 SPIFFS 自动烧录
    spiffs_create_partition_image(storage ../components/private_library/spiffs FLASH_IN_PROJECT)
elseif(CONFIG_EXAMPLE_BUILD_ES8388_SPIFFS_MP3)
    spiffs_create_partition_image(storage ../components/private_library/spiffs FLASH_IN_PROJECT)
endif()

# target_compile_options(
#     ${COMPONENT_LIB}
#     PRIVATE -Wno-error=unused-const-variable -Wno-error=delete-non-virtual-dtor
#             -Wno-error=implicit-function-declaration -fpermissive)