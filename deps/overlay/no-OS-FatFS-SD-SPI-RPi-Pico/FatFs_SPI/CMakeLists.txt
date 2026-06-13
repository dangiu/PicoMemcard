add_library(FatFs_SPI INTERFACE)
target_sources(FatFs_SPI INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/ff15/source/ffsystem.c
    ${CMAKE_CURRENT_LIST_DIR}/ff15/source/ffunicode.c
    ${CMAKE_CURRENT_LIST_DIR}/ff15/source/diskio.c
    ${CMAKE_CURRENT_LIST_DIR}/ff15/source/ff.c
    ${CMAKE_CURRENT_LIST_DIR}/sd_driver/sd_spi.c
    ${CMAKE_CURRENT_LIST_DIR}/sd_driver/demo_logging.c
#    ${CMAKE_CURRENT_LIST_DIR}/sd_driver/hw_config.c
    ${CMAKE_CURRENT_LIST_DIR}/sd_driver/spi.c
    ${CMAKE_CURRENT_LIST_DIR}/sd_driver/sd_card.c
    ${CMAKE_CURRENT_LIST_DIR}/sd_driver/crc.c
    ${CMAKE_CURRENT_LIST_DIR}/src/glue.c
    ${CMAKE_CURRENT_LIST_DIR}/src/f_util.c
    ${CMAKE_CURRENT_LIST_DIR}/src/ff_stdio.c
    ${CMAKE_CURRENT_LIST_DIR}/src/my_debug.c
    ${CMAKE_CURRENT_LIST_DIR}/src/rtc.c
)
target_include_directories(FatFs_SPI INTERFACE
    ff15/source
    sd_driver
    include
)
target_link_libraries(FatFs_SPI INTERFACE
        hardware_spi
        hardware_dma
        hardware_rtc
        pico_stdlib
)
