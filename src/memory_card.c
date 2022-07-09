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

uint32_t memory_card_sync_page(MemoryCard *mc, uint16_t address, uint8_t* data) {
	uint32_t status = 0;
    FIL memcard;

    if(FR_OK == f_open(&memcard, MEMCARD_FILE_NAME, FA_READ | FA_WRITE)) {
        UINT bytes_written;
        f_seek(&memcard, address);
        f_write(&memcard, data, MC_SEC_SIZE, &bytes_written);

        if(MC_SEC_SIZE != bytes_written) {
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