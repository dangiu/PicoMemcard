cmake_minimum_required(VERSION 3.13)

include(${CMAKE_SOURCE_DIR}/pico_sdk_import.cmake)

project(controller_simulator C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_executable(controller_simulator)

pico_generate_pio_header(${PROJECT_NAME} ${CMAKE_SOURCE_DIR}/psxSPI.pio)

target_sources(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/controller_simulator.c
)

pico_enable_stdio_uart(${PROJECT_NAME} 1)

target_link_libraries(${PROJECT_NAME} pico_stdlib hardware_pio)

pico_add_extra_outputs(${PROJECT_NAME})
