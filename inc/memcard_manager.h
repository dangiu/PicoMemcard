#ifndef __MEMCARD_MANAGER_H__
#define __MEMCARD_MANAGER_H__

#include <stdint.h>
#include <stdbool.h>

/* Error codes */
#define MM_OK					0
#define MM_ALLOC_FAIL			1
#define MM_INDEX_OUT_OF_BOUNDS	2
#define MM_NO_ENTRY				3

bool memcard_manager_exist(uint8_t* filename);
uint32_t memcard_manager_count();
uint32_t memcard_manager_get(uint32_t index, uint8_t* out_filename);
#define memcard_manager_get_first(out_filename) memcard_manager_get(0, (out_filename))
uint32_t memcard_manager_get_next(uint8_t* filename, uint8_t* out_nextfile);
uint32_t memcard_manager_get_prev(uint8_t* filename, uint8_t* out_prevfile);
uint32_t memcard_manager_create();

#endif