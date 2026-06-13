#include <inttypes.h>
#include "ff.h"
#include "fs/sd/sd_disk.h"
#include "fs/sd/sd_config.h"



/* FatFS Functions */
uint32_t SD_disk_status() {
    sd_card_t *sd = sd_get_by_num(0);
    if (!sd)
        return STA_NOINIT;

    return sd->m_Status;
}

uint32_t SD_disk_initialize() {
    sd_card_t *sd = sd_get_by_num(0);
    if (!sd)
        return STA_NOINIT;

    return sd->init(sd);
}

uint32_t SD_disk_deinitialize() {
	return  0;
}

uint32_t SD_disk_read(uint8_t* buff, uint32_t sector, uint32_t count) {
    sd_card_t *sd = sd_get_by_num(0);
    if (!sd)
        return RES_ERROR;

    return sd->read_blocks(sd, buff, sector, count)
               ? RES_ERROR
               : RES_OK;
}

uint32_t SD_disk_write(const uint8_t* buff, uint32_t sector, uint32_t count) {
    sd_card_t *sd = sd_get_by_num(0);
    if (!sd)
        return RES_ERROR;

    return sd->write_blocks(sd, buff, sector, count)
               ? RES_ERROR
               : RES_OK;
}

uint32_t SD_disk_ioctl(uint8_t cmd, void* buff) {
    sd_card_t *sd = sd_get_by_num(0);
    if (!sd)
        return RES_ERROR;

    switch (cmd) {
		case CTRL_SYNC:
			return RES_OK;
		case GET_SECTOR_COUNT:
			*(LBA_t *)buff = sd->sectors;
			return RES_OK;
		case GET_SECTOR_SIZE:
			*(WORD *)buff = 1;
			return RES_OK;
		case GET_BLOCK_SIZE:
			*(DWORD *)buff = BLOCK_SIZE;
			return RES_OK;
		default:
			return RES_PARERR;
    }
}

