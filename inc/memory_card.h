#ifndef __MEMORY_CARD_H
#define __MEMORY_CARD_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

#define MC_SEC_SIZE			128		// size of single sector in bytes
#define MC_SEC_COUNT		1024	// number of sector in one memory card
#define MC_SIZE				MC_SEC_SIZE * MC_SEC_COUNT		// size of memory card in bytes
#define MC_FLAG_BYTE_DEF	0x08	// bit 3 set = new memory card inserted

#define MC_ID1 0x5A
#define MC_ID2 0x5D
#define MC_ACK1 0x5C
#define MC_ACK2 0x5D
#define MC_TEST_SEC 0x3f	// sector 63 is the "write test" sector

#define MC_GOOD 0x47
#define MC_BAD_SEC 0xFF
#define MC_BAD_CHK 0x4E

/* Error codes */
#define MC_OK				0
#define MC_FILE_OPEN_ERR	1
#define MC_FILE_READ_ERR	2
#define MC_FILE_WRITE_ERR	3
#define MC_FILE_SIZE_ERR	4
#define MC_NO_INIT			5

typedef struct {
	uint8_t flag_byte;
	uint8_t* data;
} memory_card_t;

typedef uint16_t sector_t;

uint32_t memory_card_init(memory_card_t* mc);
uint32_t memory_card_import(memory_card_t* mc, uint8_t* file_name);
bool memory_card_is_sector_valid(memory_card_t* mc, sector_t sector);
uint8_t* memory_card_get_sector_ptr(memory_card_t* mc, sector_t sector);
void memory_card_reset_seen_flag(memory_card_t* mc);
uint32_t memory_card_sync_sector(memory_card_t* mc, sector_t sector, uint8_t* file_name);

#endif