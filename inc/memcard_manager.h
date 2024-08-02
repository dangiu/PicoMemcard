#ifndef __MEMCARD_MANAGER_H__
#define __MEMCARD_MANAGER_H__

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>


/* Error codes */
#define MM_OK					0
#define MM_ALLOC_FAIL			1
#define MM_INDEX_OUT_OF_BOUNDS	2
#define MM_NO_ENTRY				3
#define MM_BAD_PARAM			4
#define MM_NAME_CONFLICT		5
#define MM_FILE_OPEN_ERR		6
#define MM_FILE_WRITE_ERR		7

bool memcard_manager_exist(uint8_t* filename);
uint32_t memcard_manager_count();
uint32_t memcard_manager_get(uint32_t index, uint8_t* out_filename);
#define memcard_manager_get_initial(out_filename) memcard_manager_get(memcard_manager_get_prev_loaded_memcard_index(), (out_filename))
uint32_t update_prev_loaded_memcard_index(uint32_t index);
uint32_t memcard_manager_get_prev_loaded_memcard_index();
uint32_t memcard_manager_get_next(uint8_t* filename, uint8_t* out_nextfile, bool skip);
uint32_t memcard_manager_get_prev(uint8_t* filename, uint8_t* out_prevfile, bool skip);
uint32_t memcard_manager_create(uint8_t* out_filename);
uint32_t create_index(uint8_t *vec, uint8_t size, uint8_t *out_filename);
#endif