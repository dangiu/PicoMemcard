#include <inttypes.h>
#include "ff.h"
#include "fs/flash/ram_disk.h"
#include "fs/flash/lfs_flash_handler.h"
#include "fs/flash/lfs_disk.h"
#include "memory_card.h"
#include "config.h"
#include "debug.h"

uint8_t ram_disk[SIZE_RAM_BUFFER] __attribute__((section(".ram")));

/* FatFS Functions */
uint32_t RAM_disk_status() {
	return 0;
}

uint32_t RAM_disk_initialize() {
	return RAM_disk_status();
}

uint32_t RAM_disk_deinitialize() {
	return  RAM_disk_status();
}

uint32_t RAM_disk_read(uint8_t* buff, uint32_t sector, uint32_t count) {
	if(sector < 0 || sector >= SECTOR_NUM) {
		return RES_PARERR;
	}
	/* copy data to buffer */
	uint32_t buff_index = 0;
	for(uint32_t i = 0; i < count; ++i) {
		for(uint32_t j = 0; j < SECTOR_SIZE; ++j) {
			buff[buff_index] = ram_disk[((sector + i) * SECTOR_SIZE) + j];
			++buff_index;
		}
	}
	return RES_OK;
}

uint32_t RAM_disk_write(const uint8_t* buff, uint32_t sector, uint32_t count) {
	if(sector < 0 || sector >= SECTOR_NUM) {
		return RES_PARERR;
	}
	/* copy data to buffer */
	uint32_t buff_index = 0;
	for(uint32_t i = 0; i < count; ++i) {
		for(uint32_t j = 0; j < SECTOR_SIZE; ++j) {
			ram_disk[((sector + i) * SECTOR_SIZE) + j] = buff[buff_index];
			++buff_index;
		}
	}
	return RES_OK;
}

uint32_t RAM_disk_ioctl(uint8_t cmd, void* buff) {
	switch(cmd) {
		case CTRL_SYNC:
			return RES_OK;	// no cache, no need to sync
		case GET_SECTOR_COUNT:
			*(LBA_t*) buff = SECTOR_NUM;
			return RES_OK;
		case GET_SECTOR_SIZE:
			*(WORD*) buff = SECTOR_SIZE;
			return RES_OK;
		case GET_BLOCK_SIZE:
			*(DWORD*) buff = 1;	// not a flash storage device, can erase each sector individualy
			return RES_OK;
		case CTRL_TRIM:
			return RES_OK;	// not a flash storage device, we don't need to do anything
		default:
			return RES_PARERR;	// no other commands are supported
	}
}

