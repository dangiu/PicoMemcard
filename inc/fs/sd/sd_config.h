#pragma once

#include "ff.h"
#include "sd_card.h"    
#include "config.h"
    
#ifdef __cplusplus
extern "C" {
#endif
    void sd_init(uint8_t num);
    uint32_t sd_mount(uint8_t num);
    bool sd_startstop(bool start, bool load_eject);
    bool sd_ready(void *dev);

    size_t sd_get_num();
    sd_card_t *sd_get_by_num(size_t num);
    void *sd_ext_get_by_num(uint8_t num);
    
    size_t spi_get_num();
    spi_t *spi_get_by_num(size_t num);

    uint32_t sd_get_sectors(uint8_t lun);
    uint16_t sd_get_block_size(uint8_t lun);

    uint32_t sd_read(uint8_t* data, uint32_t *size, uint8_t* file_name, uint32_t max_size);
    uint32_t sd_read_block(void *dev, uint8_t* buff, uint32_t sector, uint32_t count);
    uint32_t sd_write(uint8_t* data, uint32_t size, uint8_t* file_name, uint32_t *written);
    uint32_t sd_write_at(uint8_t* data, uint32_t size, uint32_t offset, uint8_t* file_name, uint32_t *written);
    uint32_t sd_write_block(void *dev, uint8_t* buff, uint32_t sector, uint32_t count);
    void sd_dir_read(void (*callback)(unsigned char *filename, uint32_t size));

#ifdef __cplusplus
}
#endif
