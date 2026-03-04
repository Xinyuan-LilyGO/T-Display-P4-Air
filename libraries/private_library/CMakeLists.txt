set(src_dirs ./)
if(CONFIG_EXAMPLE_BUILD_RADIOLIB_LR1121_SEND_RECEIVE)
    set(requires "cpp_bus_driver" "RadioLib")
endif()
idf_component_register(
        SRC_DIRS 
        ${src_dirs} 
        INCLUDE_DIRS 
        ${src_dirs}
        REQUIRES
        ${requires}
    )

# target_compile_options(
#     ${COMPONENT_LIB}
#     PRIVATE -Wno-error=unused-const-variable -Wno-error=delete-non-virtual-dtor
#             -Wno-error=implicit-function-declaration -fpermissive)