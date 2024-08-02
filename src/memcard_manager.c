#include "memcard_manager.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "sd_config.h"
#include "memory_card.h"

/* extension for memcard files */
static const char memcard_file_ext[] = ".MCR";

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

uint32_t update_prev_loaded_memcard_index(uint32_t index) {
	/* update the previously loaded memcard index stored on the SD card */
	uint32_t retVal = MM_OK;
	FIL data_file;
	uint32_t buff_size = 100;
	char line[buff_size];
	char buff[128];
	char finalname[MAX_MC_FILENAME_LEN + 13];
	memset(finalname, '\0', MAX_MC_FILENAME_LEN + 13);
	memset(buff, '\0', 128);
	FRESULT res = f_open(&data_file, "index.txt", FA_OPEN_ALWAYS | FA_READ | FA_WRITE);
	if (res == FR_OK) {
		f_gets(line, sizeof(line), &data_file);
		printf("line be like: %s\n", line);
		if(strncmp(line, "LASTMEMCARD:", 11) != 0){
			printf("LASTMEMCARD NOT PRESENT IN INDEX FILE\n");
			retVal = MM_FILE_OPEN_ERR;
		}else{
			f_rewind(&data_file);
			strcat(finalname, "LASTMEMCARD:");
			sprintf(buff,"%03d",index);
			strcat(finalname, buff);
			strcat(finalname, ".MCR\n");
			printf("%s\n", finalname);
			f_puts(finalname, &data_file);
		}
	}
	f_close(&data_file);
	return retVal;
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
	/* retrive images names */
	FRESULT res;
	FILINFO f_info;
	char finalname[MAX_MC_FILENAME_LEN + 1];
	memset(finalname, '\0', MAX_MC_FILENAME_LEN + 1);
	sprintf(finalname,"%d",index);
	strcat(finalname, ".MCR");
	res = f_stat(finalname, &f_info);
	if(res == FR_NO_FILE){
		printf("THE MCR DOES NOT EXISTS\n");
		return MM_FILE_OPEN_ERR;
	}
	strcpy(out_filename, finalname);
	printf("got: %s\n", out_filename);
	return MM_OK;
}

uint32_t memcard_manager_get_prev_loaded_memcard_index() {
	/* read which memcard to load from last session from SD card */
	uint32_t index = 0;
	char finalname[MAX_MC_FILENAME_LEN + 1];
	memset(finalname, '\0', MAX_MC_FILENAME_LEN + 1);
	FIL data_file;
	uint32_t buff_size = 100;
	FRESULT res = f_open(&data_file, "index.txt", FA_OPEN_ALWAYS | FA_READ | FA_WRITE);
	char line[buff_size];
	if (res == FR_OK) {
		f_gets(line, sizeof(line), &data_file);
		if(strncmp(line, "LASTMEMCARD:", 11) != 0){
			char lastmem[MAX_MC_FILENAME_LEN + 13];
			memset(lastmem, '\0', MAX_MC_FILENAME_LEN + 13);
			strcat(lastmem, "LASTMEMCARD:\n");
			printf("LASTMEMCARD NOT PRESENT IN INDEX FILE, WRITING...\n");
			f_puts(lastmem, &data_file);
		}else{
			int j = 0;
			for(int i = 12; i < MAX_MC_FILENAME_LEN + 1; i++){
				if(line[i] == '\n'){
					break;
				}else{
					finalname[j] = line[i];
					j++;
				}
			}
			index = atoi(finalname);
		}
	}
	f_close(&data_file);
	return index;
}

uint32_t memcard_manager_get_next(uint8_t* filename, uint8_t* out_nextfile, bool skip) {
	if(!filename || !out_nextfile)
		return MM_BAD_PARAM;
	/* retrive images names */
	FRESULT res;
	DIR root;
	FILINFO f_info;
	//res = f_opendir(&root, "");	// open root directory
	char finalname[MAX_MC_FILENAME_LEN + 1];
	memset(finalname, '\0', MAX_MC_FILENAME_LEN + 1);
	uint32_t index = atoi(filename);
	if(skip){
		index += 5;
	}else{
		index++;
	}
	sprintf(finalname,"%d",index);
	strcat(finalname, ".MCR");
	res = f_stat(finalname, &f_info);
	if(res == FR_NO_FILE){
		while(res == FR_NO_FILE){
			printf("THE MCR DOES NOT EXISTS\n");
			memset(finalname, '\0', MAX_MC_FILENAME_LEN + 1);
			index--;
			if(index < atoi(filename)){
				return MM_NO_ENTRY;
			}
			sprintf(finalname,"%d",index);
			strcat(finalname, ".MCR");
			res = f_stat(finalname, &f_info);
		}
	}
	strcpy(out_nextfile, finalname);
	printf("next mc is: %s", out_nextfile);
	/* return */
	return MM_OK;
}

uint32_t memcard_manager_get_prev(uint8_t* filename, uint8_t* out_prevfile, bool skip) {
	if(!filename || !out_prevfile)
		return MM_BAD_PARAM;
	/* retrive images names */
	FRESULT res;
	FILINFO f_info;
	//res = f_opendir(&root, "");	// open root directory
	char finalname[MAX_MC_FILENAME_LEN + 1];
	memset(finalname, '\0', MAX_MC_FILENAME_LEN + 1);
	uint32_t index = atoi(filename);
	if(skip){
		index -= 5;
	}else{
		index--;
	}
	if(index < 0){
		index = 0;
	}
	sprintf(finalname,"%d",index);
	strcat(finalname, ".MCR");
	res = f_stat(finalname, &f_info);
	if(res == FR_NO_FILE){
		printf("THE MCR DOES NOT EXISTS\n");
		return MM_NO_ENTRY;
	}
	strcpy(out_prevfile, finalname);
	printf("prev mc is: %s", out_prevfile);
	/* return */
	return MM_OK;
}

uint32_t memcard_manager_create(uint8_t* out_filename) {
	if(!out_filename)
		return MM_BAD_PARAM;

  uint8_t name[MAX_MC_FILENAME_LEN + 1];
  FIL memcard_image;

  uint8_t memcard_n = 0;
  FRESULT f_res;
  do {
    snprintf(name, MAX_MC_FILENAME_LEN + 1, "%d.MCR", memcard_n++); // Set name to %d.MCR
    f_res = f_open(&memcard_image, name, FA_CREATE_NEW | FA_WRITE); // Open new file for writing
  } while (f_res == FR_EXIST); // Repeat if file exists.

  strcpy(out_filename, name); // We have a valid name, copy it to out_filename

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
	update_prev_loaded_memcard_index(memcard_n - 1);
	return MM_OK;
}

uint32_t create_index(uint8_t *vec, uint8_t size, uint8_t *out_filename){
	uint8_t new_name[MAX_MC_FILENAME_LEN + 1];

	FIL fptr;

	FRESULT fr;
	fr = f_open(&fptr, "index.txt", FA_OPEN_ALWAYS | FA_READ |FA_WRITE);

	if (fr != FR_OK) {
		printf("error opening file. Skill issue\n");
		return FR_DISK_ERR;
	}
	else {
		printf("The file is created Successfully.\n");
	}

	char data[128];
	char n_data[128];
	int ind = 0;
	--size;

	char lastmem[MAX_MC_FILENAME_LEN + 1];

	//we init the var otherwise it will be dirty as hell
	memset(data, '\0', 128);
	memset(new_name, '\0', MAX_MC_FILENAME_LEN + 1);
	memset(out_filename, '\0', MAX_MC_FILENAME_LEN + 1);
	memset(lastmem, '\0', MAX_MC_FILENAME_LEN + 1);
	strcat(data, "\n");
	bool backslash = false;
	for(int i = 0; i < size; i++){
		if(vec[i] == ':'){
			for(int j = i + 1; j < size; j++){

				// just to eliminate backslash from str
				if(vec[j] == 92 && backslash == false){
					backslash = true;
					continue;
				}
				if(vec[j] == ';'){
					ind++;
					break;
				}
				n_data[ind] = vec[j];
				ind++;
			}
		}
	}

	strcat(data, n_data);
	printf("the array is: %s\n", data);
	
	// check if the string is \PSIO and if it is we get the hell out of here
	if(strcmp(n_data, "\\PSIO") == 0 ||strcmp(n_data, "/XSTATION") == 0 || strcmp(n_data, "UNIROM") == 0 || strcmp(n_data, "\\UNIROM_B.EXE") == 0){
		printf("\nSkipped psio/xstation/UNIROM\n");
		f_close(&fptr);
		return 1;
	}else if(strlen(n_data) < 5){
		printf("array too short, a comunication error might be occured\n");
		f_close(&fptr);
		return 1;
	}

	if (fr == FR_OK) {
		char line[256];
		bool flag = false;
		ind--;
		//checks if the ID is already present
		while (f_gets(line, sizeof(line), &fptr)){
			if(strncmp(n_data, line, ind) == 0){
				flag = true;
				printf("GAMEID already present\n");
				break;
			}
		}
		//if it is we search the memorycard assigned to it
		if(flag){
			if(line[ind] == ':'){
				int len = 1;
				char buff[MAX_MC_FILENAME_LEN + 1];
				memset(buff, '\0', MAX_MC_FILENAME_LEN + 1);
				ind++;
				for(int y = 0; y < MAX_MC_FILENAME_LEN + 1; y++){
					if(line[ind] == '\n'){
						break;
					}
					new_name[y] = line[ind];
					ind++; 
				}
				strcpy(out_filename, new_name);

				//updating lastmemcard id
				f_rewind(&fptr);
				strcat(lastmem, "LASTMEMCARD:");
				sprintf(buff, "%03d", atoi(new_name));
				strcat(lastmem, buff);
				strcat(lastmem, ".MCR\n");
				f_puts(lastmem, &fptr);
				printf("memcard is: %s\n", out_filename);
			}else{
				printf("\n COULD NOT FIND ':' CHAR FIX YOUR INDEX FILE\n");
				f_close(&fptr);
				return FR_DISK_ERR;
			}
			
		}
		//if it is not, then we add the GAMEID with the new MCR file
		else if(!flag){
			printf("GAMEID not present, writing to index.txt\n");
			uint32_t status = memcard_manager_create(new_name);
			//we concatenate stuff to have a valid id with the memcard
			
			strcat(data, ":");
			strcat(data, new_name);
			printf("\n what i write: %s", data);
			f_puts(data, &fptr);
			strcpy(out_filename, new_name);
			printf("\n THE NEW MCR IS: %s\n", out_filename);
		}
	}else{
		//this should not happen
		printf("WE HAVE A HUGE PROBLEM!\n");
		f_close(&fptr);
		return FR_DISK_ERR;
	}
	f_close(&fptr);
	return MM_OK;
}
