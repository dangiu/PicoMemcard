#ifndef __SD_DISK_H__
#define __SD_DISK_H__

#include <stdint.h>
#include <stdbool.h>

#include "diskio.h"

/* FatFS Functions */
uint32_t SD_disk_status();
uint32_t SD_disk_initialize();
uint32_t SD_disk_deinitialize();
uint32_t SD_disk_read(uint8_t* buff, uint32_t sector, uint32_t count);
uint32_t SD_disk_write(const uint8_t* buff, uint32_t sector, uint32_t count);
uint32_t SD_disk_ioctl(uint8_t cmd, void* buff);


#endif