#include "memcard_manager.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
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
	if(!out_filename)
		return MM_BAD_PARAM;
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
	if(!filename || !out_nextfile)
		return MM_BAD_PARAM;
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
	if(!filename || !out_prevfile)
		return MM_BAD_PARAM;
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

uint32_t memcard_manager_create(uint8_t* out_filename) {
	if(!out_filename)
		return MM_BAD_PARAM;
	uint8_t name[MAX_MC_FILENAME_LEN + 1];
	uint32_t count = memcard_manager_count();
	uint32_t status = memcard_manager_get(count - 1, name);	// get name of last (alphabetically) memory card image
	uint32_t memcard_n = atoi(name);	// convert to integer
	snprintf(name, MAX_MC_FILENAME_LEN + 1, "%d.MCR", ++memcard_n);	// generate new name by incrementing by 1
	if(memcard_manager_exist(name))	// check that generated name does not exist
		return MM_NAME_CONFLICT;
	strcpy(out_filename, name);
	/* generate image file */
	FIL memcard_image;
	FRESULT f_res = f_open(&memcard_image, name, FA_CREATE_NEW | FA_WRITE);
	if(f_res == FR_OK) {
		UINT bytes_written = 0;
		uint8_t buffer[MC_SEC_SIZE];
		uint8_t xor;
		/* header frame (block 0, sec 0) */
		buffer[0] = 'M';
		buffer[1] = 'C';
		xor = buffer[0] ^ buffer[1];
		for(int i = 2; i < MC_SEC_SIZE - 1; i++) {
			buffer[i] = 0;
			xor = xor ^ buffer[i];
		}
		buffer[MC_SEC_SIZE - 1] = xor;
		f_res = f_write(&memcard_image, buffer, MC_SEC_SIZE, &bytes_written);
		if(f_res != FR_OK || bytes_written != MC_SEC_SIZE) {
			f_close(&memcard_image);
			return MM_FILE_WRITE_ERR;
		}
		/* directory frames (block 0, sec 1..15) */
		buffer[0] = 0xa0;	// free block
		xor = buffer[0];
		for(int i = 1; i < 8; i++) {
			buffer[i] = 0;
			xor = xor ^ buffer[i];
		}
		buffer[8] = buffer[9] = 0xff;	// no next block
		xor = xor ^ buffer[8] ^ buffer[9];
		for(int i = 10; i < MC_SEC_SIZE - 1; i++) {
			buffer[i] = 0;
			xor = xor ^ buffer[i];
		}
		buffer[MC_SEC_SIZE - 1] = xor;
		for(int i = 0; i < 15; i++) {
			f_res = f_write(&memcard_image, buffer, MC_SEC_SIZE, &bytes_written);
			if(f_res != FR_OK || bytes_written != MC_SEC_SIZE) {
				f_close(&memcard_image);
				return MM_FILE_WRITE_ERR;
			}
		}
		/* broken sector list (block 0, sec 16..35) */
		buffer[0] = buffer[1] = buffer[2] = buffer[3] = 0xff;	// no broken sector
		xor = buffer[0] ^ buffer[1] ^ buffer[2] ^ buffer[3];
		buffer[4] = buffer[5] = buffer[6] = buffer[7] = 0x00;	// 0 fill
		xor = xor ^ buffer[4] ^ buffer[5] ^ buffer[6] ^ buffer[7];
		buffer[8] = buffer[9] = 0xff;	// 1 fill
		xor = xor ^ buffer[8] ^ buffer[9];
		for(int i = 10; i < MC_SEC_SIZE - 1; i++) {
			buffer[i] = 0x00;
			xor = xor ^ buffer[i];
		}
		buffer[MC_SEC_SIZE - 1] = xor;
		for(int i = 0; i < 20; i++) {
			f_res = f_write(&memcard_image, buffer, MC_SEC_SIZE, &bytes_written);
			if(f_res != FR_OK || bytes_written != MC_SEC_SIZE) {
				f_close(&memcard_image);
				return MM_FILE_WRITE_ERR;
			}
		}
		/* broken sector replacement data (block 0, sec 36..55) and unused frames (block 0, sec 56..62) */
		for(int i = 0; i < MC_SEC_SIZE; i++) {
			buffer[i] = 0x00;
		}
		for(int i = 0; i < 27; i++) {
			f_res = f_write(&memcard_image, buffer, MC_SEC_SIZE, &bytes_written);
			if(f_res != FR_OK || bytes_written != MC_SEC_SIZE) {
				f_close(&memcard_image);
				return MM_FILE_WRITE_ERR;
			}
		}
		/* test write sector (block 0, sec 63) */
		buffer[0] = 'M';
		buffer[1] = 'C';
		xor = buffer[0] ^ buffer[1];
		for(int i = 2; i < MC_SEC_SIZE - 1; i++) {
			buffer[i] = 0;
			xor = xor ^ buffer[i];
		}
		buffer[MC_SEC_SIZE - 1] = xor;
		f_res = f_write(&memcard_image, buffer, MC_SEC_SIZE, &bytes_written);
		if(f_res != FR_OK || bytes_written != MC_SEC_SIZE) {
			f_close(&memcard_image);
			return MM_FILE_WRITE_ERR;
		}
		/* fill remaining 15 blocks with zeros */
		for(int i = 0; i < MC_SEC_SIZE; i++) {
			buffer[i] = 0;
		}
		for(int i = 0; i < MC_SEC_COUNT - 64; i++) {	// 64 are the number of sectors written already (forming block 0)
			f_res = f_write(&memcard_image, buffer, MC_SEC_SIZE, &bytes_written);
			if(f_res != FR_OK || bytes_written != MC_SEC_SIZE) {
				f_close(&memcard_image);
				return MM_FILE_WRITE_ERR;
			}
		}
		f_close(&memcard_image);
	} else {
		return MM_FILE_OPEN_ERR;
	}
	return MM_OK;
}