#include "memory_card.h"
#include "lfs_disk.h"
#include "lfs.h"
#include "pico/malloc.h"
#include "pico/time.h"
#include "pico/multicore.h"
#include "led.h"
#include "config.h"

uint32_t memory_card_init(memory_card_t* mc) {
	if(!mc)
		return MC_NO_INIT;
	mc->flag_byte = MC_FLAG_BYTE_DEF;
	mc->data = (uint8_t*) malloc(sizeof(uint8_t) * MC_SIZE);
	if(!mc->data)
		return MC_NO_INIT;	// malloc failed
	mc->out_of_sync = false;
	mc->last_operation_timestamp = 0;
	return MC_OK;
}

uint32_t memory_card_import(memory_card_t* mc, uint8_t* file_name) {
	uint32_t status = 0;
	if(mc) {
		memory_card_set_sync(mc, false);
		mc->flag_byte = MC_FLAG_BYTE_DEF;
		lfs_t lfs;
		lfs_file_t memcard;
		if(LFS_ERR_OK == lfs_mount(&lfs, &LFS_CFG)) {
			if(LFS_ERR_OK == lfs_file_open(&lfs, &memcard, file_name, LFS_O_RDONLY)) {
				lfs_ssize_t size = lfs_file_read(&lfs, &memcard, mc->data, MC_SIZE);
				if(size < 0) {
					status = MC_FILE_READ_ERR;
				} else if(size != MC_SIZE) {
					status = MC_FILE_SIZE_ERR;
				}
				lfs_file_close(&lfs, &memcard);
			} else  {
				status = MC_FILE_OPEN_ERR;
			}
			lfs_unmount(&lfs);
		} else {
			status = MC_MOUNT_ERR;
		}
	} else {
		status = MC_NO_INIT;
	}
	return status;
}

bool memory_card_is_sector_valid(memory_card_t* mc, uint32_t sector) {
	(void) mc;
	if(sector < 0 || sector >= MC_SEC_COUNT)
		return false;
	return true;
}

uint8_t* memory_card_get_sector_ptr(memory_card_t* mc, uint32_t sector) {
	if(mc) {
		return &mc->data[sector * MC_SEC_SIZE];
	}
	return NULL;
}

void memory_card_set_sync(memory_card_t* mc, bool out_of_sync) {
	if(mc) {
		led_output_sync_status(out_of_sync);
		mc->out_of_sync = out_of_sync;
	}
}

bool memory_card_get_sync(memory_card_t* mc) {
	if(mc) {

		return mc->out_of_sync;
	}
}

void memory_card_update_timestamp(memory_card_t* mc) {
	if(mc) {
		mc->last_operation_timestamp = to_ms_since_boot(get_absolute_time());
	}
}

uint32_t memory_card_sync(memory_card_t* mc) {
	uint32_t status = 0;
	multicore_lockout_start_blocking();
	if(mc) {
		lfs_t lfs;
		lfs_file_t memcard;
		if(LFS_ERR_OK == lfs_mount(&lfs, &LFS_CFG)) {
			if(LFS_ERR_OK == lfs_file_open(&lfs, &memcard, MEMCARD_FILE_NAME, LFS_O_RDWR)) {
				if(MC_SIZE == lfs_file_write(&lfs, &memcard, mc->data, MC_SIZE)) {
					memory_card_set_sync(mc, false);
				} else {
					status = MC_FILE_WRITE_ERR;
				}
				lfs_file_close(&lfs, &memcard);
			} else  {
				status = MC_FILE_OPEN_ERR;
			}
			lfs_unmount(&lfs);
		} else {
			status = MC_MOUNT_ERR;
		}
	} else {
		status = MC_NO_INIT;
	}
	multicore_lockout_end_blocking();
	return status;
}

void memory_card_reset_seen_flag(memory_card_t* mc) {
	if(mc) {
		mc->flag_byte &= ~(1 << 3);
	}
}