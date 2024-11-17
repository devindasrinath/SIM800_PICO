add_library( Sim800
    Sim800.h
    Sim800.c
    )

target_link_libraries(Sim800 pico_stdlib hardware_uart pico_time)
target_include_directories(Sim800 PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")