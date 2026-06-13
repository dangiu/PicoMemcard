#include "memcard_manager.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "fs/sd/sd_config.h"
#include "fs/fs.h"
#include "memory_card.h"

/* extension for memcard files */
static const char memcard_file_ext[] = ".MCR";

/* filename to store previously loaded memcard index */
static const char memcard_lastmemcardindex_filename[] = "LastMemcardIndex.dat";

size_t valid_images = 0;
size_t images = 0;
size_t cimage = 0;
uint8_t* image_names;

bool is_name_valid(uint8_t* filename) {
	if(!filename)
		return false;
	filename = strupr(filename);	// convert to upper case
	/* check .MCR extension */
	uint8_t* ext = strrchr(filename, '.');
	if(!ext || strcmp(ext, memcard_file_ext))
		return false;
	/* check that filename (excluding extension) is only digits */
	uint32_t digit_char_count = strspn(filename, "0123456789");
	if(digit_char_count != strlen(filename) - strlen(memcard_file_ext))
		return false;
	return true;
}

bool is_image_valid(uint8_t* filename, uint32_t fsize) {
	if(!filename)
		return false;
	filename = strupr(filename);	// convert to upper case
	if(!is_name_valid(filename))
		return false;
	
	if(fsize != MC_SIZE)	// check that memory card image has correct size
		return false;
	return true;
}


void count_valid_images(uint8_t* filename, uint32_t fsize) {
	if(is_image_valid(filename, fsize)) {
		valid_images++;
	}
	if(is_name_valid(filename)) {
		images++;
	}
}

void set_images_names(uint8_t* filename, uint32_t fsize) {
	if(is_image_valid(filename, fsize)) {
		strcpy(&image_names[(MAX_MC_FILENAME_LEN + 1) * cimage], filename);
		cimage++;
	}
}

uint32_t update_prev_loaded_memcard_index(uint32_t index) {
	/* update the previously loaded memcard index stored on the SD card */
	uint32_t retVal = MM_FILE_WRITE_ERR;
	uint32_t buff_size = 100, bytes_written;
	char str_index[buff_size];
	int index_len = sprintf(str_index, "%d", index);

	uint32_t status = fs_manager.write(str_index, index_len, memcard_lastmemcardindex_filename, &bytes_written);	
	if(status == FR_OK && bytes_written >= index_len) {
		retVal == MM_OK;
	}

	return retVal;
}

bool memcard_manager_exist(uint8_t* filename) {
	if(!filename)
		return false;
	return is_image_valid(filename, MC_SIZE);
}

uint32_t memcard_manager_count() {
	valid_images = 0;
	images = 0;
	fs_manager.dir_read(count_valid_images);

	return valid_images;
}

uint32_t memcard_manager_count_with_err_size() {
	memcard_manager_count();

	return images;
}

uint32_t memcard_manager_get(uint32_t index, uint8_t* out_filename) {
	if(!out_filename)
		return MM_BAD_PARAM;
	if(index < 0 || index > MAX_MC_IMAGES)
		return MM_INDEX_OUT_OF_BOUNDS;
	uint32_t count = memcard_manager_count();
	if(index >= count)
		return MM_INDEX_OUT_OF_BOUNDS;
	image_names = malloc(((MAX_MC_FILENAME_LEN + 1) * count));	// allocate space for image names
	if(!image_names)
		return MM_ALLOC_FAIL; // malloc failed
	/* retrieve images names */
	cimage = 0;
	fs_manager.dir_read(set_images_names);

	/* sort names alphabetically */
	qsort(image_names, count, (MAX_MC_FILENAME_LEN + 1), (__compar_fn_t) strcmp);
	strcpy(out_filename, &image_names[(MAX_MC_FILENAME_LEN + 1) * index]);
	free(image_names);	// free allocated memory
	return MM_OK;
}

uint32_t memcard_manager_get_prev_loaded_memcard_index() {
	/* read which memcard to load from last session from FS */
	uint32_t buff_size = 100, size;
	char line[buff_size];
	uint32_t index = 0;

	uint32_t status = fs_manager.read(line, &size, memcard_lastmemcardindex_filename, buff_size);			
	if(status == FR_OK) {
		/* string to int (base 10) */
		index = (uint32_t)strtol(line, (char**)NULL, 10);
	}
	return index;
}

uint32_t memcard_manager_get_next(uint8_t* filename, uint8_t* out_nextfile) {
	if(!filename || !out_nextfile)
		return MM_BAD_PARAM;
	uint32_t count = memcard_manager_count();
	uint32_t buff_size = (MAX_MC_FILENAME_LEN + 1) * count;
	uint8_t* image_names = malloc(buff_size);	// allocate space for image names
	if(!image_names)
		return MM_ALLOC_FAIL; // malloc failed
	/* retrieve images names */
	cimage = 0;
	fs_manager.dir_read(set_images_names);
	/* sort names alphabetically */
	qsort(image_names, count, (MAX_MC_FILENAME_LEN + 1), (__compar_fn_t) strcmp);
	/* find current and return following one */
	bool found = false;
	for(uint32_t i = 0; i < buff_size; i = i + (MAX_MC_FILENAME_LEN + 1)) {
		if(!strcmp(filename, &image_names[i])) {
			int32_t next_i = i + (MAX_MC_FILENAME_LEN + 1);
			if(next_i < buff_size) {
				int32_t new_index = next_i / ((MAX_MC_FILENAME_LEN + 1));
				update_prev_loaded_memcard_index(new_index);
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
	/* retrieve images names */
	cimage = 0;
	fs_manager.dir_read(set_images_names);

	/* sort names alphabetically */
	qsort(image_names, count, (MAX_MC_FILENAME_LEN + 1), (__compar_fn_t) strcmp);
	/* find current and return prior one */
	bool found = false;
	for(uint32_t i = 0; i < buff_size; i = i + (MAX_MC_FILENAME_LEN + 1)) {
		if(!strcmp(filename, &image_names[i])) {
			int32_t prev_i = i - (MAX_MC_FILENAME_LEN + 1);
			if(prev_i >= 0) {
				int32_t new_index = prev_i / (MAX_MC_FILENAME_LEN + 1);
				update_prev_loaded_memcard_index(new_index);
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

#include "led.h"
uint32_t memcard_manager_create(uint8_t* out_filename) {
	if(!out_filename)
		return MM_BAD_PARAM;

	uint8_t name[MAX_MC_FILENAME_LEN + 1];
	uint8_t memcard_n = memcard_manager_count_with_err_size();
	int f_res;
	
	if(memcard_n<0||memcard_n>=MAX_MC_IMAGES) {
		return MM_INDEX_OUT_OF_BOUNDS;
	}

	snprintf(name, sizeof(name), "%d.MCR", memcard_n+1); // Names start at 1.MCR, 2.MCR, etc
	strcpy(out_filename, name); // We have a valid name, copy it to out_filename

	uint32_t bytes_written = 0;
	uint32_t total_bytes_written = 0;
	uint8_t *buffer     = &(ram_disk[MC_SIZE]);
	uint8_t *cur_sector = &(ram_disk[MC_SIZE]);
	uint8_t xor;
	/*********** Block 0 *************/
	/* header frame (block 0, sec 0) */
	cur_sector[0] = 'M';
	cur_sector[1] = 'C';
	xor = cur_sector[0] ^ cur_sector[1];
	for(int i = 2; i < MC_SEC_SIZE - 1; i++) {
		cur_sector[i] = 0;
		xor = xor ^ cur_sector[i];
	}
	cur_sector[MC_SEC_SIZE - 1] = xor;
	cur_sector += MC_SEC_SIZE;

	/* directory frames (block 0, sec 1..15) */
	cur_sector[0] = 0xa0;	// free block
	xor = cur_sector[0];
	for(int i = 1; i < 8; i++) {
		cur_sector[i] = 0;
		xor = xor ^ cur_sector[i];
	}
	cur_sector[8] = cur_sector[9] = 0xff;	// no next block
	xor = xor ^ cur_sector[8] ^ cur_sector[9];
	for(int i = 10; i < MC_SEC_SIZE - 1; i++) {
		cur_sector[i] = 0;
		xor = xor ^ cur_sector[i];
	}
	cur_sector[MC_SEC_SIZE - 1] = xor;

	for(int i = 0; i < 14; i++) {
		memcpy(&(cur_sector[MC_SEC_SIZE*(i+1)]), cur_sector, MC_SEC_SIZE);
	}
	cur_sector += 15*MC_SEC_SIZE;

	/* broken sector list (block 0, sec 16..35) */
	cur_sector[0] = cur_sector[1] = cur_sector[2] = cur_sector[3] = 0xff;	// no broken sector
	xor = cur_sector[0] ^ cur_sector[1] ^ cur_sector[2] ^ cur_sector[3];
	cur_sector[4] = cur_sector[5] = cur_sector[6] = cur_sector[7] = 0x00;	// 0 fill
	xor = xor ^ cur_sector[4] ^ cur_sector[5] ^ cur_sector[6] ^ cur_sector[7];
	cur_sector[8] = cur_sector[9] = 0xff;	// 1 fill
	xor = xor ^ cur_sector[8] ^ cur_sector[9];
	for(int i = 10; i < MC_SEC_SIZE - 1; i++) {
		cur_sector[i] = 0x00;
		xor = xor ^ cur_sector[i];
	}
	cur_sector[MC_SEC_SIZE - 1] = xor;
	for(int i = 0; i < 19; i++) {
		memcpy(&(cur_sector[MC_SEC_SIZE*(i+1)]), cur_sector, MC_SEC_SIZE);
	}
	cur_sector += 20*MC_SEC_SIZE;

	/* broken sector replacement data (block 0, sec 36..55) and unused frames (block 0, sec 56..62) */
	memset(cur_sector, 0, 27*MC_SEC_SIZE);
	cur_sector += 27*MC_SEC_SIZE;

	/* test write sector (block 0, sec 63) */
	cur_sector[0] = 'M';
	cur_sector[1] = 'C';
	xor = cur_sector[0] ^ cur_sector[1];
	for(int i = 2; i < MC_SEC_SIZE - 1; i++) {
		cur_sector[i] = 0;
		xor = xor ^ cur_sector[i];
	}
	cur_sector[MC_SEC_SIZE - 1] = xor;
	
	f_res = fs_manager.write_at(buffer, MC_SLOT_SIZE, 0, out_filename, &bytes_written);
	if(f_res != FR_OK || bytes_written != MC_SLOT_SIZE) {
		return MM_FILE_WRITE_ERR;
	}
	total_bytes_written+=bytes_written;

	/******* End of Block 0 **********/

	/***** Block 1 - Block 15 ********/
	cur_sector = buffer;
	/* fill remaining 15 blocks with zeros */
	memset(cur_sector, 0, MC_SLOT_SIZE);
	for(int i = 1; i <= MC_SEC_COUNT/64 - 1; i++) {	// 64 are the number of sectors written already (forming block 0)
		f_res = fs_manager.write_at(buffer, MC_SLOT_SIZE, i*MC_SLOT_SIZE, out_filename, &bytes_written);
		if(f_res != FR_OK || bytes_written != MC_SLOT_SIZE) {
			return MM_FILE_WRITE_ERR;
		}
		total_bytes_written+=bytes_written;
	
		if(i%4==0) {led_output_new_mc();}
	}
	update_prev_loaded_memcard_index(memcard_n);
	return MM_OK;
}
