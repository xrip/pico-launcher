add_library(tv INTERFACE)

target_sources(tv INTERFACE ${CMAKE_CURRENT_LIST_DIR}/tv.c)

target_link_libraries(tv INTERFACE hardware_adc hardware_pwm)

target_include_directories(tv INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}
)

