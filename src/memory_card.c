#include "memory_card.h"
#include <stdlib.h>
#include "config.h"
#include "ff.h"
#include "pico/stdlib.h"
#include "sd_config.h"

uint32_t memory_card_init(memory_card_t* mc) {
	if(!mc)
		return MC_NO_INIT;
	mc->flag_byte = MC_FLAG_BYTE_DEF;
	mc->data = (uint8_t*) malloc(sizeof(uint8_t) * MC_SIZE);
	if(!mc->data)
		return MC_NO_INIT;	// malloc failed
	return MC_OK;
}

uint32_t memory_card_import(memory_card_t* mc, uint8_t* file_name) {
	uint32_t status = MC_OK;
	FIL memcard;

	if(mc) {
		mc->flag_byte = MC_FLAG_BYTE_DEF;
		sd_card_t *p_sd = sd_get_by_num(0);
		if(FR_OK == f_mount(&p_sd->fatfs, "", 1)) {
			if(FR_OK == f_open(&memcard, MEMCARD_FILE_NAME, FA_READ)) {
				UINT bytes_read;
				if(FR_OK == f_read(&memcard, mc->data, MC_SIZE, &bytes_read)) {
					if(MC_SIZE != bytes_read) {
						status = MC_FILE_READ_ERR;
					}
				} else {
					status = MC_FILE_SIZE_ERR;
				}
				f_close(&memcard);
			} else {
				status = MC_FILE_OPEN_ERR;
			}
		} else {
			status = MC_MOUNT_ERR;
		}
	} else {
		status = MC_NO_INIT;
	}

	return status;
}

bool memory_card_is_sector_valid(memory_card_t* mc, sector_t sector) {
	(void) mc;
	if(sector < 0 || sector >= MC_SEC_COUNT)
		return false;
	return true;
}

uint8_t* memory_card_get_sector_ptr(memory_card_t* mc, sector_t sector) {
	if(mc)
		return &mc->data[sector * MC_SEC_SIZE];
	return NULL;
}

void memory_card_reset_seen_flag(memory_card_t* mc) {
	if(mc)
		mc->flag_byte &= ~(1 << 3);
}

/***
 *	Sync memory card modified sectors back into flash storage.
 *	Does not create concurrency problem as it only reads from the in-RAM copy.
 * 	If a sector is being synced while the in-RAM copy is being modified,
 * 	then there is a transient loss of consistency. Consistency is eventually
 * 	resolved since there will be another entry further down the queue
 * 	enforcing the sync for that same sector to occurr once again.
 */
uint32_t memory_card_sync_sector(memory_card_t* mc, sector_t sector) {
	uint32_t status = MC_OK;
	FIL memcard;

	if(FR_OK == f_open(&memcard, MEMCARD_FILE_NAME, FA_READ | FA_WRITE)) {
		UINT bytes_written;
		f_lseek(&memcard, (sector * MC_SEC_SIZE));
		if(FR_OK == f_write(&memcard, &mc->data[sector * MC_SEC_SIZE], MC_SEC_SIZE, &bytes_written)) {
			if(MC_SEC_SIZE != bytes_written) {
				status = MC_FILE_SIZE_ERR;
			}
		} else {
			status = MC_FILE_WRITE_ERR;
		}

		f_close(&memcard);
	} else {
		status = MC_FILE_OPEN_ERR;
	}

	return status;
}