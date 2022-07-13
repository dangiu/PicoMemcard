add_library(ssd1331-driver INTERFACE)
target_sources(ssd1331-driver INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/src/ssd1331.c
)
target_include_directories(ssd1331-driver INTERFACE
        include
)
target_link_libraries(ssd1331-driver INTERFACE
        hardware_spi
        pico_time
        pico_stdlib
)
