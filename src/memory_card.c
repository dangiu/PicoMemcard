#include "memory_card.h"
#include <stdlib.h>
#include "config.h"
#include "ff.h"
#include "fs/fs.h"
#include "pico/stdlib.h"

uint32_t memory_card_init(memory_card_t* mc) {
	if(!mc)
		return MC_NO_INIT;
	mc->flag_byte = MC_FLAG_BYTE_DEF;
	mc->data = ram_disk; // Avoid to allocate a big region with malloc. Use a static buffer instead
	return MC_OK;
}

uint32_t memory_card_import(memory_card_t* mc, uint8_t* file_name) {
	uint32_t status;
	uint32_t size;
	if(mc) {
		mc->flag_byte = MC_FLAG_BYTE_DEF;
		status = fs_manager.read(mc->data, &size, file_name, MC_SIZE);
				
		if(status == FR_OK) {
			if(size != MC_SIZE) {
				status = MC_FILE_READ_ERR;
			}
		} else {
			status = MC_FILE_SIZE_ERR;
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
uint32_t memory_card_sync_sector(memory_card_t* mc, sector_t sector, uint8_t* file_name) {
	uint32_t status = MC_OK;
	FIL memcard;
	uint32_t bytes_written;
	status = fs_manager.write_at(memory_card_get_sector_ptr(mc, sector), MC_SEC_SIZE, (sector * MC_SEC_SIZE), file_name, &bytes_written);
	if(FR_OK == status) {
		if(MC_SEC_SIZE != bytes_written) {
			status = MC_FILE_SIZE_ERR;
		}
	} else {
		status = MC_FILE_WRITE_ERR;
	}	

	return status;
}
