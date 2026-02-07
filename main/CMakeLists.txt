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
endif()
        
idf_component_register(SRC_DIRS ${project_path} 
                        INCLUDE_DIRS ${project_path})

# target_compile_options(
#     ${COMPONENT_LIB}
#     PRIVATE -Wno-error=unused-const-variable -Wno-error=delete-non-virtual-dtor
#             -Wno-error=implicit-function-declaration -fpermissive)