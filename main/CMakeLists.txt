#这里选择你需要编译的项目
if(CONFIG_EXAMPLE_BUILD_IPERF)
    set(project_path "examples/iperf/")
elseif(CONFIG_EXAMPLE_BUILD_IR_NEC_TRANSCEIVER)
    set(project_path "examples/ir_nec_transceiver/")
elseif(CONFIG_EXAMPLE_BUILD_NRF9151_UART)
    set(project_path "examples/nrf9151_uart/")
elseif(CONFIG_EXAMPLE_BUILD_RF_TEST)
    set(project_path "examples/rf_test/")
endif()
        
idf_component_register(SRC_DIRS ${project_path} 
                        INCLUDE_DIRS ${project_path})

if(CONFIG_EXAMPLE_BUILD_IR_NEC_TRANSCEIVER)
    idf_component_optional_requires(PRIVATE
        esp_driver_rmt
    )
endif()

if(CONFIG_EXAMPLE_BUILD_NRF9151_UART)
    idf_component_optional_requires(PRIVATE
        esp_driver_uart
    )
endif()

if(CONFIG_EXAMPLE_BUILD_IPERF)
    idf_component_optional_requires(PRIVATE
        console
        esp_wifi
        nvs_flash
    )
endif()

if(CONFIG_EXAMPLE_BUILD_RF_TEST)
    idf_component_optional_requires(PRIVATE
        esp_event
        esp_netif
        esp_wifi
    )
endif()
