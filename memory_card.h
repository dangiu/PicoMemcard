#ifndef __MEMORY_CARD_H
#define __MEMORY_CARD_H

#include <stdint.h>
#include <stdbool.h>

#define MC_SIZE 128*1024		// size of memory card in bytes
#define MC_SEC_SIZE 128			// size of single sector in bytes
#define MC_MAX_SEC 0x3ff
#define MC_FLAG_BYTE_DEF 0x08	// bit 3 set = new memory card inserted

#define MC_ID1 0x5A
#define MC_ID2 0x5D
#define MC_ACK1 0x5C
#define MC_ACK2 0x5D

#define MC_GOOD 0x47

typedef struct {
	uint8_t flag_byte;
	uint8_t data[MC_SIZE];
} MemoryCard;

uint32_t memory_card_init(MemoryCard* mc, const uint8_t data[MC_SIZE]);
bool memory_card_is_sector_valid(uint32_t sector);
uint8_t* memory_card_get_sector_ptr(MemoryCard* mc, uint32_t sector);
void memory_card_reset_seen_flag(MemoryCard* mc);

#endif