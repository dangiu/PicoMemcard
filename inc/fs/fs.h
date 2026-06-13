#pragma once

#ifndef __FS_H__
#define __FS_H__


#include <stdint.h>

#include "config.h"
#include "pico/stdlib.h"

// "Class" representing SD Cards/QSPI memory
struct fs_manager_t {
    void (*init)(uint8_t num);
    uint32_t (*mount)(uint8_t lun);
    void (*umount)(uint8_t lun);
    bool (*startstop)(bool start, bool load_eject);
    bool (*ready)(void *dev);
    void *(*get_by_num)(uint8_t lun);
    uint32_t (*get_sectors)(uint8_t lun);
    uint16_t (*get_block_size)(uint8_t lun);
    uint32_t (*read)(uint8_t* data, uint32_t *size, uint8_t* file_name, uint32_t max_size);
    uint32_t (*write)(uint8_t* data, uint32_t size, uint8_t* file_name, uint32_t *written);
    uint32_t (*write_at)(uint8_t* data, uint32_t size, uint32_t offset, uint8_t* file_name, uint32_t *written);
    void (*try_flush)(uint8_t* file_name);
    void (*dir_read)(void (*callback)(unsigned char *filename, uint32_t size));

};

extern struct fs_manager_t fs_manager;

#endif