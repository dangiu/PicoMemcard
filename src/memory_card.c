#include <string.h>
#include "memory_card.h"
#include "ff.h"
#include "pico/printf.h"

uint32_t memory_card_load_image(MemoryCard* mc, uint8_t bank_number) {
	uint32_t status = 0;
    FIL memcard;
    char file_name[16];

    sprintf(file_name, "MEMCARD%d.MCR", bank_number);

    mc->flag_byte = MC_FLAG_BYTE_DEF;

    FRESULT result = f_open(&memcard, file_name, FA_READ);

    if(result == FR_OK) {
        UINT bytes_read;
        f_read(&memcard, &mc->data, MC_SIZE, &bytes_read);

        if(MC_SIZE != bytes_read) {
            status = 1;
        }

        f_close(&memcard);
    } else if(result == FR_NO_FILE) {
        // We need to create the file
        if(FR_OK == f_open(&memcard, file_name, FA_CREATE_NEW | FA_WRITE)) {
            for(int i = 0; i < MC_SIZE; i++) {
                f_putc(0x00, &memcard);
            }
            f_close(&memcard);
            // Clear the buffer in memory, no point in reading the image since it's empty.
            memset(&mc->data, 0x00, MC_SIZE);
        } else {
            status = 1;
        }
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

uint32_t memory_card_sync_page(uint16_t address, uint8_t* data, uint8_t bank_number) {
	uint32_t status = 0;
    FIL memcard;
    char file_name[16];

    sprintf(file_name, "MEMCARD%d.MCR", bank_number);

    if(FR_OK == f_open(&memcard, file_name, FA_READ | FA_WRITE)) {
        UINT bytes_written;
        f_lseek(&memcard, (address * MC_SEC_SIZE));
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

#ifdef PMC_ENABLE_SYNC_LOG
uint32_t memory_card_sync_page_with_log(uint16_t address, uint8_t* data, uint8_t bank_number, uint8_t queue_level) {
    FIL queue_log;

    if(FR_OK == f_open(&queue_log, "queue.log", FA_OPEN_APPEND | FA_WRITE)) {
        f_printf(&queue_log, "SYNC SECTOR [0x%X] Bank[%d]: Queue depth [%d]\n", address, bank_number, queue_level);
        f_close(&queue_log);
    }

    return memory_card_sync_page(address, data, queue_level);
}
#endif

uint32_t memory_card_reset_seen_flag(MemoryCard* mc) {
	mc->flag_byte &= ~(1 << 3);
	return 0;
}