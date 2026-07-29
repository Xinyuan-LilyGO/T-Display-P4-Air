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
