#include "memory_card.h"
#include "pico/time.h"
#include "sd_config.h"
#include "sd_card.h"
#include "ff.h"

uint32_t memory_card_init(MemoryCard* mc) {
	uint32_t status = 0;
    FIL memcard;

    mc->flag_byte = MC_FLAG_BYTE_DEF;
    if(FR_OK == f_open(&memcard, MEMCARD_FILE_NAME, FA_READ)) {
        UINT bytes_read;
        f_read(&memcard, &mc->data, MC_SIZE, &bytes_read);

        if(MC_SIZE != bytes_read) {
            status = 1;
        }
        f_close(&memcard);
    } else {
        status = 1;
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
    FIL memcard;

    if(FR_OK == f_open(&memcard, MEMCARD_FILE_NAME, FA_READ | FA_WRITE)) {
        UINT bytes_written;
        f_write(&memcard, &mc->data, MC_SIZE, &bytes_written);

        if(MC_SIZE != bytes_written) {
            status = 1;
        }
        f_close(&memcard);
    } else {
        status = 1;
    }
    
	return status;
}

uint32_t memory_card_reset_seen_flag(MemoryCard* mc) {
	mc->flag_byte &= ~(1 << 3);
	return 0;
}