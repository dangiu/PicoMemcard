cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(picomemcard_project C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
add_definitions(-DPICO_OPTIMIZED_DEBUG=1)
pico_sdk_init()

add_executable(PicoMemcard)

pico_set_linker_script(PicoMemcard ${CMAKE_SOURCE_DIR}/memmap.ld)
pico_generate_pio_header(PicoMemcard ${CMAKE_CURRENT_LIST_DIR}/psxSPI.pio)

add_subdirectory(no-OS-FatFS-SD-SPI-RPi-Pico/FatFs_SPI)
add_subdirectory(ssd1331-driver)

# Example source
target_sources(PicoMemcard PUBLIC
    ${CMAKE_SOURCE_DIR}/src/main.c
    ${CMAKE_SOURCE_DIR}/src/sd_config.c
    ${CMAKE_SOURCE_DIR}/src/memcard_simulator.c
    ${CMAKE_SOURCE_DIR}/src/memory_card.c
    ${CMAKE_SOURCE_DIR}/src/lcd.c
)

# Example include
target_include_directories(PicoMemcard PUBLIC
    ${CMAKE_SOURCE_DIR}/inc
)

pico_enable_stdio_uart(PicoMemcard 1)	# enable only UART stdio

add_definitions(-DPMC_ENABLE_SYNC_LOG)
target_link_libraries(PicoMemcard pico_multicore pico_stdlib pico_time hardware_pio FatFs_SPI ssd1331-driver)

pico_add_extra_outputs(PicoMemcard)