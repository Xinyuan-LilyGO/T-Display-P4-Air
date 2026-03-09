if(CONFIG_EXAMPLE_BUILD_ES8388)
    set(src_dirs "./")
endif()
idf_component_register(
        SRC_DIRS 
        ${src_dirs} 
        INCLUDE_DIRS 
        ${src_dirs}
        REQUIRES
        ${requires}
    )