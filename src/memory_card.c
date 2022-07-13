#include "memory_card.h"
#include "lfs_disk.h"
#include "lfs.h"
#include "pico/time.h"

uint32_t memory_card_init(MemoryCard* mc) {
	uint32_t status = 0;
	lfs_t lfs;
	lfs_file_t memcard;

	mc->flag_byte = MC_FLAG_BYTE_DEF;
	if(LFS_ERR_OK == lfs_mount(&lfs, &LFS_CFG)) {
		if(LFS_ERR_OK == lfs_file_open(&lfs, &memcard, MEMCARD_FILE_NAME, LFS_O_RDONLY)) {
			lfs_ssize_t size = lfs_file_read(&lfs, &memcard, mc->data, MC_SIZE);
			
			if(size < 0) {
				status = 3; // failed to read memcard file
			} else if(size == 0) {
				status = 4; // memcard file is empty
			} else if(size != MC_SIZE) {
				status = 5; // memcard file size is incorrect
			}

			lfs_file_close(&lfs, &memcard);
		} else  {
			status = 2;	// failed to open memcard file
		}
		lfs_unmount(&lfs);
	} else {
		status = 1;	// failed to mount filesystem
	}
	return status;
}

bool memory_card_is_sector_valid(MemoryCard* mc, uint32_t sector) {
	(void) mc;
	if(sector < 0 || sector > MC_MAX_SEC)
		return false;
	return true;
}

uint8_t* memory_card_get_sector_ptr(MemoryCard* mc, uint32_t sector) {
	return &mc->data[sector * MC_SEC_SIZE];
}

void memory_card_set_sync(MemoryCard* mc, bool out_of_sync) {
	mc->out_of_sync = out_of_sync;
}

bool memory_card_get_sync(MemoryCard* mc) {
	return mc->out_of_sync;
}

void memory_card_update_timestamp(MemoryCard* mc) {
	mc->last_operation_timestamp = to_ms_since_boot(get_absolute_time());
}

uint32_t memory_card_sync(MemoryCard* mc) {
	uint32_t status = 0;
	lfs_t lfs;
	lfs_file_t memcard;

	if(LFS_ERR_OK == lfs_mount(&lfs, &LFS_CFG)) {
		if(LFS_ERR_OK == lfs_file_open(&lfs, &memcard, MEMCARD_FILE_NAME, LFS_O_RDWR)) {
			if(MC_SIZE != lfs_file_write(&lfs, &memcard, mc->data, MC_SIZE)) {
				status = 1;	// failed to write-back memory card image
			}
			lfs_file_close(&lfs, &memcard);
		} else  {
			status = 1;	// failed to open memcard file
		}
		lfs_unmount(&lfs);
	} else {
		status = 1;	// failed to mount filesystem
	}
	return status;
}

uint32_t memory_card_reset_seen_flag(MemoryCard* mc) {
	mc->flag_byte &= ~(1 << 3);
	return 0;
}