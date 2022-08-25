#include "memcard_manager.h"
#include <stdlib.h>
#include <string.h>
#include "sd_config.h"
#include "memory_card.h"

bool is_name_valid(uint8_t* filename) {
	if(!filename)
		return false;
	filename = strupr(filename);	// convert to upper case
	/* check .MCR extension */
	uint8_t* ext = strrchr(filename, '.');
	if(!ext || strcmp(ext, ".MCR"))
		return false;
	/* check that filename (excluding extension) is only digits */
	uint32_t digit_char_count = strspn(filename, "0123456789");
	if(digit_char_count != strlen(filename) - strlen(".MCR"))
		return false;
	return true;
}

bool is_image_valid(uint8_t* filename) {
	if(!filename)
		return false;
	filename = strupr(filename);	// convert to upper case
	if(!is_name_valid(filename))
		return false;
	FILINFO f_info;
	FRESULT f_res = f_stat(filename, &f_info);
	if(f_res != FR_OK)
		return false;
	if(f_info.fsize != MC_SIZE)	// check that memory card image has correct size
		return false;
	return true;
}

bool memcard_manager_exist(uint8_t* filename) {
	if(!filename)
		return false;
	return is_image_valid(filename);
}

uint32_t memcard_manager_count() {
	FRESULT res;
	DIR root;
	FILINFO f_info;
	res = f_opendir(&root, "");	// open root directory
	uint32_t count = 0;
	if(res == FR_OK) {
		while(true) {
			res = f_readdir(&root, &f_info);
			if(res != FR_OK || f_info.fname[0] == 0) break;
			if(!(f_info.fattrib & AM_DIR)) {	// not a directory
				if(is_image_valid(f_info.fname))
					++count;
			}
		}
	}
	return count;
}

uint32_t memcard_manager_get(uint32_t index, uint8_t* out_filename) {
	if(index < 0 || index > MAX_MC_IMAGES)
		return MM_INDEX_OUT_OF_BOUNDS;
	uint32_t count = memcard_manager_count();
	if(index >= count)
		return MM_INDEX_OUT_OF_BOUNDS;
	uint8_t* image_names = malloc(((MAX_MC_FILENAME_LEN + 1) * count));	// allocate space for image names
	if(!image_names)
		return MM_ALLOC_FAIL; // malloc failed
	/* retrive images names */
	FRESULT res;
	DIR root;
	FILINFO f_info;
	res = f_opendir(&root, "");	// open root directory
	uint32_t i = 0;
	if(res == FR_OK) {
		while(true) {
			res = f_readdir(&root, &f_info);
			if(res != FR_OK || f_info.fname[0] == 0) break;
			if(!(f_info.fattrib & AM_DIR)) {	// not a directory
				if(is_image_valid(f_info.fname)) {
					strcpy(&image_names[(MAX_MC_FILENAME_LEN + 1) * i], f_info.fname);
					++i;
				}
			}
		}
	}
	/* sort names alphabetically */
	qsort(image_names, count, (MAX_MC_FILENAME_LEN + 1), (__compar_fn_t) strcmp);
	strcpy(out_filename, &image_names[(MAX_MC_FILENAME_LEN + 1) * index]);
	free(image_names);	// free allocated memory
	return MM_OK;
}

uint32_t memcard_manager_get_next(uint8_t* filename, uint8_t* out_nextfile) {
	uint32_t count = memcard_manager_count();
	uint32_t buff_size = (MAX_MC_FILENAME_LEN + 1) * count;
	uint8_t* image_names = malloc(buff_size);	// allocate space for image names
	if(!image_names)
		return MM_ALLOC_FAIL; // malloc failed
	/* retrive images names */
	FRESULT res;
	DIR root;
	FILINFO f_info;
	res = f_opendir(&root, "");	// open root directory
	uint32_t i = 0;
	if(res == FR_OK) {
		while(true) {
			res = f_readdir(&root, &f_info);
			if(res != FR_OK || f_info.fname[0] == 0) break;
			if(!(f_info.fattrib & AM_DIR)) {	// not a directory
				if(is_image_valid(f_info.fname)) {
					strcpy(&image_names[(MAX_MC_FILENAME_LEN + 1) * i], f_info.fname);
					++i;
				}
			}
		}
	}
	/* sort names alphabetically */
	qsort(image_names, count, (MAX_MC_FILENAME_LEN + 1), (__compar_fn_t) strcmp);
	/* find current and return following one */
	bool found = false;
	for(uint32_t i = 0; i < buff_size; i = i + (MAX_MC_FILENAME_LEN + 1)) {
		if(!strcmp(filename, &image_names[i])) {
			int32_t next_i = i + (MAX_MC_FILENAME_LEN + 1);
			if(next_i < buff_size) {
				strcpy(out_nextfile, &image_names[next_i]);
				found = true;
				break;
			}
		}
	}
	free(image_names);	// free allocated memory
	/* return */
	if(found)
		return MM_OK;
	else
		return MM_NO_ENTRY;
}

uint32_t memcard_manager_get_prev(uint8_t* filename, uint8_t* out_prevfile) {
	uint32_t count = memcard_manager_count();
	uint32_t buff_size = (MAX_MC_FILENAME_LEN + 1) * count;
	uint8_t* image_names = malloc(buff_size);	// allocate space for image names
	if(!image_names)
		return MM_ALLOC_FAIL; // malloc failed
	/* retrive images names */
	FRESULT res;
	DIR root;
	FILINFO f_info;
	res = f_opendir(&root, "");	// open root directory
	uint32_t i = 0;
	if(res == FR_OK) {
		while(true) {
			res = f_readdir(&root, &f_info);
			if(res != FR_OK || f_info.fname[0] == 0) break;
			if(!(f_info.fattrib & AM_DIR)) {	// not a directory
				if(is_image_valid(f_info.fname)) {
					strcpy(&image_names[(MAX_MC_FILENAME_LEN + 1) * i], f_info.fname);
					++i;
				}
			}
		}
	}
	/* sort names alphabetically */
	qsort(image_names, count, (MAX_MC_FILENAME_LEN + 1), (__compar_fn_t) strcmp);
	/* find current and return prior one */
	bool found = false;
	for(uint32_t i = 0; i < buff_size; i = i + (MAX_MC_FILENAME_LEN + 1)) {
		if(!strcmp(filename, &image_names[i])) {
			int32_t prev_i = i - (MAX_MC_FILENAME_LEN + 1);
			if(prev_i >= 0) {
				strcpy(out_prevfile, &image_names[prev_i]);
				found = true;
				break;
			}
		}
	}
	free(image_names);	// free allocated memory
	/* return */
	if(found)
		return MM_OK;
	else
		return MM_NO_ENTRY;
}

uint32_t memcard_manager_create() {

}