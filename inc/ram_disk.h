#ifndef __RAM_DISK_H__
#define __RAM_DISK_H__

#include <stdint.h>
#include "diskio.h"

#define VOLUME_LABEL "Pico MC"

#define DISK_BLOCK_NUM 357	// results in ~128KB ram disk (some blocks are lost to FAT overhead, some due to Windows Storage Service)
#define DISK_BLOCK_SIZE 512
#define SECTOR_NUM DISK_BLOCK_NUM
#define SECTOR_SIZE DISK_BLOCK_SIZE

/* FatFS Functions */
uint32_t RAM_disk_status();
uint32_t RAM_disk_initialize();
uint32_t RAM_disk_deinitialize();
uint32_t RAM_disk_read(uint8_t* buff, uint32_t sector, uint32_t count);
uint32_t RAM_disk_write(const uint8_t* buff, uint32_t sector, uint32_t count);
uint32_t RAM_disk_ioctl(uint8_t cmd, void* buff);

uint32_t RAM_disk_import_lfs_memcard();
uint32_t RAM_disk_export_lfs_memcard();

#endif