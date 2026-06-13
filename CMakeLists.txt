cmake_minimum_required(VERSION 3.13)
# cmake -DUSE_SDCARD=ON -DUSE_SDCARD=OFF ...
option(MULTICORE "Use Multicore capabilities" OFF)
option(USE_SDCARD "Use SD Card design" ON)
option(USE_16MBFLASH "SPI Flash of 16MB" OFF)

# cmake -DRP2040_BOARD=promicro ..
set(RP2040_BOARD "pico" CACHE STRING "Board type")

set_property(CACHE RP2040_BOARD PROPERTY STRINGS
    pico
    zero
    promicro
)

# cmake -DDBG_LEVEL=2 ..
# Use debugging level (0=No debug; 1=info messages; 2=verbose messages)
set(DBG_LEVEL 2)


if(USE_16MBFLASH)
  message("16MB Flash")

  add_compile_definitions(
    PICO_FLASH_SIZE_BYTES=16777216
  )

  add_definitions(-DUSE_16MBFLASH)
  set(FLASH_SIZE 16384k)
else()
  set(FLASH_SIZE 2048k)
endif()

set(PICO_PLATFORM rp2040)
if(RP2040_BOARD STREQUAL "pico")
  set(PICO_BOARD pico_w)

  message("RP2040-Pico Configuration")
  add_definitions(-DPICO)
  set(PIN_DAT 5)
  set(PIN_CMD 6)
  set(PIN_SEL 7)
  set(PIN_CLK 8)
  set(PIN_ACK 9)
  add_compile_definitions(
    PIN_MISO=16
    PIN_MOSI=19
    PIN_SCK=18
    PIN_SS=17
  )
  add_compile_definitions(
    LED_PIN=25
    LED_STATE_ON=1
  )
elseif(RP2040_BOARD STREQUAL "zero")
  set(PICO_BOARD waveshare_rp2040_zero)

  message("RP2040-zero configuration")
  add_definitions(-DRP2040ZERO)
  set(PIN_DAT 9)
  set(PIN_CMD 10)
  set(PIN_SEL 11)
  set(PIN_CLK 12)
  set(PIN_ACK 13)

  add_compile_definitions(
    PIN_MISO=0
    PIN_MOSI=3
    PIN_SCK=2
    PIN_SS=1
  )
  add_compile_definitions(
    PICO_DEFAULT_WS2812_PIN=16
    LED_STATE_ON=1
  )
elseif(RP2040_BOARD STREQUAL "promicro")
  #set(PICO_BOARD sparkfun_promicro)
  set(PICO_BOARD pico)

  message("RP2040-Pro Micro configuration")
  add_definitions(-DRP2040PROMICRO)
  set(PIN_DAT 26)
  set(PIN_CMD 22)
  set(PIN_SEL 20)
  set(PIN_CLK 23)
  set(PIN_ACK 21)

  add_compile_definitions(
    PIN_MISO=4
    PIN_MOSI=7
    PIN_SCK=6
    PIN_SS=5
  )
  
  add_compile_definitions(
    LED_PIN=17
    LED_STATE_ON=1
    PICO_DEFAULT_SPI_SCK_PIN=6
    PICO_DEFAULT_SPI_TX_PIN=3
    PICO_DEFAULT_SPI_RX_PIN=4
    PICO_DEFAULT_SPI_CSN_PIN=5
    PICO_DEFAULT_I2C_SCL_PIN=13
    PICO_DEFAULT_I2C_SDA_PIN=12
  )
endif()


include(pico_sdk_import.cmake)

project(picomemcard_project C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)


pico_sdk_init()

add_executable(PicoMemcard)
target_compile_definitions(PicoMemcard PUBLIC DBG_LEVEL=${DBG_LEVEL})

if(MULTICORE)
  add_definitions(-DMULTICORE)
  message("Multicore system")
else()
  message("Monocore system")
endif()

if(USE_SDCARD)
  add_definitions(-DUSE_SDCARD)
  message("Using the design with MicroSD")
else()
  message("Using the design without MicroSD")
endif()

configure_file(${CMAKE_SOURCE_DIR}/memmap.ld.in ${CMAKE_SOURCE_DIR}/memmap.ld @ONLY)
pico_set_linker_script(PicoMemcard ${CMAKE_SOURCE_DIR}/memmap.ld)

configure_file(${CMAKE_CURRENT_LIST_DIR}/psxSPI.pio.in ${CMAKE_CURRENT_LIST_DIR}/psxSPI.pio @ONLY)
pico_generate_pio_header(PicoMemcard ${CMAKE_CURRENT_LIST_DIR}/psxSPI.pio)
pico_generate_pio_header(PicoMemcard ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio)

add_definitions(-DNO_PICO_LED)  # prevent SD SPI library from using LED

file(COPY
    ${CMAKE_SOURCE_DIR}/deps/overlay/
    DESTINATION ${CMAKE_SOURCE_DIR}/deps/
)
add_subdirectory(${CMAKE_SOURCE_DIR}/deps/no-OS-FatFS-SD-SPI-RPi-Pico/FatFs_SPI PicoMemcard)
# add_subdirectory(poc_examples)

# Example source
target_sources(PicoMemcard PUBLIC
    ${CMAKE_SOURCE_DIR}/src/led.c
    ${CMAKE_SOURCE_DIR}/src/debug.c
    ${CMAKE_SOURCE_DIR}/src/main.c
    ${CMAKE_SOURCE_DIR}/src/memcard_manager.c
    ${CMAKE_SOURCE_DIR}/src/memcard_simulator.c
    ${CMAKE_SOURCE_DIR}/src/memory_card.c
    ${CMAKE_SOURCE_DIR}/src/msc_handler.c
    ${CMAKE_SOURCE_DIR}/src/usb_descriptors.c
    ${CMAKE_SOURCE_DIR}/src/fs/fs.c
    ${CMAKE_SOURCE_DIR}/src/fs/sd/sd_disk.c
    ${CMAKE_SOURCE_DIR}/src/fs/sd/sd_config.c
    ${CMAKE_SOURCE_DIR}/src/fs/sd/sd_wrapper.c
    ${CMAKE_SOURCE_DIR}/src/fs/flash/ram_disk.c
    ${CMAKE_SOURCE_DIR}/src/fs/flash/lfs_disk.c
    ${CMAKE_SOURCE_DIR}/src/fs/flash/lfs_flash_handler.c
    ${CMAKE_SOURCE_DIR}/src/fs/flash/flash_wrapper.c
    ${CMAKE_SOURCE_DIR}/deps/littlefs/lfs.c
    ${CMAKE_SOURCE_DIR}/deps/littlefs/lfs_util.c
)

# Example include
target_include_directories(PicoMemcard PUBLIC
    ${CMAKE_SOURCE_DIR}/inc
    ${CMAKE_SOURCE_DIR}/deps/littlefs
)

pico_enable_stdio_usb(PicoMemcard 1)	# enable usb stdio
pico_enable_stdio_uart(PicoMemcard 0)	# enable UART stdio

target_link_libraries(
  PicoMemcard
  pico_stdlib
  pico_multicore
  pico_time
  hardware_pio
  tinyusb_device
  tinyusb_board
  FatFs_SPI

  hardware_adc
  hardware_structs
)

if(RP2040_BOARD STREQUAL "pico")
  target_link_libraries(
    PicoMemcard
    pico_cyw43_arch_none # If you do not need the TCP/IP stack but wish to use the on-board LED on a Pico W
  )
endif()

pico_add_extra_outputs(PicoMemcard)
