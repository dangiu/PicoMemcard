#include "memory_card.h"

uint32_t memory_card_init(MemoryCard* mc, const uint8_t data[MC_SIZE]) {
	mc->flag_byte = MC_FLAG_BYTE_DEF;
	for(uint32_t i = 0; i < MC_SIZE; ++i) {
		mc->data[i] = data[i];
	}
}

bool memory_card_is_sector_valid(uint32_t sector) {
	if(sector < 0 || sector > MC_MAX_SEC) {
		return false;
	}
	return true;
}

uint8_t* memory_card_get_sector_ptr(MemoryCard* mc, uint32_t sector) {
	return &mc->data[sector * MC_SEC_SIZE];
}

void memory_card_reset_seen_flag(MemoryCard* mc) {
	mc->flag_byte &= ~(1 << 3);
}