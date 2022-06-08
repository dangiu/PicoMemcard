#ifndef __MEMORY_CARD_H
#define __MEMORY_CARD_H

#include <stdint.h>
#include <stdbool.h>

#define MEMCARD_FILE_NAME "MEMCARD.MCR"     // name of memcard file inside system
#define MEMCARD_FILE_CONTENT_SIZE 131072	// size of memcard file content in bytes (128KB)

#define MC_SEC_SIZE 128			// size of single sector in bytes
#define MC_SIZE MC_SEC_SIZE * 1024		// size of memory card in bytes
#define MC_MAX_SEC 0x3ff
#define MC_FLAG_BYTE_DEF 0x08	// bit 3 set = new memory card inserted

#define MC_ID1 0x5A
#define MC_ID2 0x5D
#define MC_ACK1 0x5C
#define MC_ACK2 0x5D
#define MC_TEST_SEC 0x3f	// sector 63 is the "write test" sector

#define MC_GOOD 0x47
#define MC_BAD_SEC 0xFF
#define MC_BAD_CHK 0x4E

typedef struct {
	uint8_t flag_byte;
	uint8_t data[MC_SIZE];
	bool out_of_sync;
	uint32_t last_operation_timestamp;
} MemoryCard;

uint32_t memory_card_init(MemoryCard* mc);
bool memory_card_is_sector_valid(MemoryCard* mc, uint32_t sector);
uint8_t* memory_card_get_sector_ptr(MemoryCard* mc, uint32_t sector);
void memory_card_set_sync(MemoryCard* mc, bool out_of_sync);
bool memory_card_get_sync(MemoryCard* mc);
void memory_card_update_timestamp(MemoryCard* mc);
uint32_t memory_card_sync(MemoryCard* mc);
uint32_t memory_card_reset_seen_flag(MemoryCard* mc);

#endif