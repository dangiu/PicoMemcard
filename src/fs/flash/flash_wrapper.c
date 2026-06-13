#include <string.h>
#include "debug.h"
#include "config.h"
#include "memory_card.h"
#include "led.h"
#include "pico/multicore.h"
#include "pico/time.h"
#include "fs/flash/flash_config.h"
#include "fs/flash/ram_disk.h"
#include "fs/flash/lfs_disk.h"

#define WORK_BUFF_SIZE 1024

static unsigned char initialized = 0;
static bool import_lfs_completed = false;
static uint8_t working_buffer[WORK_BUFF_SIZE] __attribute__((section(".ram")));

static uint16_t out_of_sync = 0;
static uint64_t last_written_time = 0;

const uint64_t FLUSH_TIMEOUT_US = 3*1000000; // 3s


static void lfs_init() {
	lfs_t lfs;
	
	/* Initialize LittleFs */
	int lfs_status = lfs_mount(&lfs, &LFS_CFG);	// mount the filesystem
	if (lfs_status) {
		lfs_format(&lfs, &LFS_CFG);	// reformat file system if error occurred (normally on first boot)
		lfs_mount(&lfs, &LFS_CFG);
	} 
	if(lfs_status == LFS_ERR_OK) {
		lfs_unmount(&lfs);
		initialized = 1;
	}
}

void flash_format() {
	lfs_t lfs;
	lfs_format(&lfs, &LFS_CFG);
}

static int RAM_disk_create_default_FatFs();
static uint32_t RAM_disk_create_FatFs_from_LFS();
static uint32_t RAM_disk_sync_LFS_from_FatFs();

uint32_t flash_mount(uint8_t num) {
	return RAM_disk_create_FatFs_from_LFS();
}


bool flash_startstop(bool start, bool load_eject) {
	if ( load_eject ) {
		if(start) {
			RAM_disk_initialize();
			RAM_disk_create_FatFs_from_LFS();
		} else {
			import_lfs_completed = false;
			RAM_disk_sync_LFS_from_FatFs();
			RAM_disk_deinitialize();
		}
	}

	return true;
}

bool flash_ready(void *dev) {
	return RAM_disk_status() == 0 && import_lfs_completed;
}

void* flash_get_by_num(uint8_t lun) { // Nothing to return. Just for compatibilities aspects
    return (void *)initialized;
}

uint32_t flash_get_sectors(uint8_t lun) {
    return (uint32_t) DISK_BLOCK_NUM;
}

uint16_t flash_get_block_size(uint8_t lun) {
    return (uint16_t) BLOCK_SIZE;
}

void flash_init(uint8_t num) {
	lfs_init();
}


uint32_t flash_read(uint8_t* data, uint32_t *size, uint8_t* file_name, uint32_t max_size) {
	uint32_t status = 0;
	if(data) {
		lfs_t lfs;
		lfs_file_t memcard;
		if(LFS_ERR_OK == lfs_mount(&lfs, &LFS_CFG)) {
			if(LFS_ERR_OK == lfs_file_open(&lfs, &memcard, file_name, LFS_O_RDONLY)) {
				lfs_ssize_t lfs_size = lfs_file_read(&lfs, &memcard, data, max_size);
				*size = lfs_size;
				lfs_file_close(&lfs, &memcard);
				status = FR_OK;
			} else  {
				status = !FR_OK;
			}
			lfs_unmount(&lfs);
		} else {
			status = !FR_OK;
		}
	} else {
		status = !FR_OK;
	}
	return status;
}

uint32_t flash_write(uint8_t* data, uint32_t size, uint8_t* file_name, uint32_t *written) {
	return flash_write_at(data, size, 0, file_name, written);
}

bool timeout_elapsed() {
    uint64_t now = time_us_64();
    return last_written_time == 0 ? false : (now - last_written_time) >= FLUSH_TIMEOUT_US;
}

static uint32_t __flash_write_at(uint8_t* data, uint32_t size, uint32_t offset, uint8_t* file_name, uint32_t *written) {
	uint32_t status = !FR_OK;

	lfs_t lfs;
	lfs_file_t memcard;

	if(LFS_ERR_OK == lfs_mount(&lfs, &LFS_CFG)) {
		if(LFS_ERR_OK == lfs_file_open(&lfs, &memcard, file_name, LFS_O_RDWR | LFS_O_CREAT)) {
			lfs_file_seek(&lfs, &memcard,  offset, LFS_SEEK_SET);
			*written = lfs_file_write(&lfs, &memcard, data, size);
			lfs_file_close(&lfs, &memcard);
			status = FR_OK;
		}
		lfs_unmount(&lfs);
	}
	
	return status;
}

void flash_try_flush(uint8_t* file_name) {
	uint32_t status;
	int i = 0;

	if(timeout_elapsed() && out_of_sync) {
		uint32_t written;
		led_output_sync_status(true);
		for(i=0;i<MC_SLOT_COUNT;i++) {
			if((out_of_sync & (1<<i)) != 0) {
				__flash_write_at(ram_disk + i*MC_SLOT_SIZE, MC_SLOT_SIZE, i*MC_SLOT_SIZE, file_name, &written);
				if(status!=FR_OK || written!=MC_SLOT_SIZE) {
					while(true)
						led_blink_error(status);
				}
				out_of_sync &= ~(1 << i);
			}
		}
		led_output_sync_status(false);	
	}
}

uint32_t flash_write_at(uint8_t* data, uint32_t size, uint32_t offset, uint8_t* file_name, uint32_t *written) {
	uint32_t status = !FR_OK;

	if(data) {
		// Creation of a new MC. Block the system and perform the copy
		if(size!=MC_SEC_SIZE) {
			status = __flash_write_at(data,size, offset, file_name, written);
		} else {	// Small copy. Performed by the PSX. Cache the slots
			*written = size; // How to actually test?
			status = FR_OK;

			int diff = ((uint32_t)data - (uint32_t)ram_disk);
			if(diff>=0 && diff<SIZE_RAM_BUFFER ) {   // We are writing data from ram_disk
				int current_sector = offset / MC_SEC_SIZE;
				int current_slot = current_sector / MC_SLOT_SEC_COUNT;
				current_sector %= MC_SLOT_SEC_COUNT;

				last_written_time = time_us_64();
				out_of_sync |= 1<<current_slot;
				flash_try_flush(file_name);
			}
		}
	}

	return status;
}

void flash_umount(uint8_t lun) {
	RAM_disk_sync_LFS_from_FatFs();
}

void flash_dir_read(void (*callback)(unsigned char *filename, uint32_t size)) {
	lfs_dir_t dir;
	struct lfs_info info;
	lfs_t lfs;
	lfs_file_t memcard;

	if(LFS_ERR_OK == lfs_mount(&lfs, &LFS_CFG)) {
		lfs_dir_open(&lfs, &dir, "/");

		while (lfs_dir_read(&lfs, &dir, &info) > 0) {
			callback(info.name, info.size);			
		}

		lfs_dir_close(&lfs, &dir);
		lfs_unmount(&lfs);
	}
}


static int RAM_disk_create_FatFs_from_file() {
	lfs_t lfs;
	lfs_file_t file_lfs, memcard_lfs;
	FIL memcard_fat;
	int status = 1;

	if(LFS_ERR_OK == lfs_mount(&lfs, &LFS_CFG)) {
		if(LFS_ERR_OK == lfs_file_open(&lfs, &file_lfs, DOWNLOAD_FILE, LFS_O_RDWR)) {
			int value;
			if(lfs_file_read(&lfs, &file_lfs, &value, sizeof(int)) < 0) {
				status = 2;	/// error during LFS read
			} else {
				status = 0;
    			char memcard_name[20];

    			snprintf(memcard_name, sizeof(memcard_name), "%d.MCR", value);
				if(LFS_ERR_OK == lfs_file_open(&lfs, &memcard_lfs, memcard_name, LFS_O_RDWR)) {
					if(FR_OK == f_open(&memcard_fat, memcard_name, FA_CREATE_NEW | FA_WRITE)) {
						/* Import virtual disk memory card from LFS into FAT */
						while(true) {
							int bytes_read;
							int bytes_written;

							bytes_read = lfs_file_read(&lfs, &memcard_lfs, working_buffer, WORK_BUFF_SIZE);
							if(bytes_read < 0) {
								status = 5;	/// error during LFS read
								break;	
							}
							if(FR_OK != f_write(&memcard_fat, working_buffer, bytes_read, &bytes_written)) {
								status = 4;	// error during virtual disk write
								break;
							}
							if(bytes_read < WORK_BUFF_SIZE) {
								status = 0;
								break;	// reached EOF, copy completed
							}
						}
						f_close(&memcard_fat);
						f_chmod(memcard_name, AM_RDO, AM_RDO);
					}
					lfs_file_close(&lfs, &memcard_lfs);
				} else {
					status = 3; // The memory card indicated by DOWNLOAD_FILE does not exists
				}
			}
			lfs_file_close(&lfs, &file_lfs);
			lfs_remove(&lfs, DOWNLOAD_FILE);	// remove old retrieve file
		}
		lfs_unmount(&lfs);
	}
	DBG_VERBOSE("RAM_disk_create_FatFs_from_file. Status = %d", status);
	return status;
}

static int has_mcr_extension(const char *name)
{
    const char *dot = strrchr(name, '.');
    if (!dot) return 0;

    // case-insensitive compare
    return strcasecmp(dot, ".mcr") == 0;
}

static void print_mc_namefiles(uint8_t* filename, uint32_t fsize) {
	FIL fatf;
	if(has_mcr_extension(filename)) {
		if(FR_OK == f_open(&fatf, MC_FILE_NAME, FA_OPEN_EXISTING | FA_WRITE)) {
			UINT bytes_written;
			snprintf(working_buffer,WORK_BUFF_SIZE,"   %s (%d B)\n", filename, fsize);
			f_lseek(&fatf, f_size(&fatf));
			f_write(&fatf, working_buffer, strlen(working_buffer), &bytes_written);
			f_close(&fatf);
		}
	}
}

static int RAM_disk_create_default_FatFs() {
	int status = 0;
	lfs_t lfs;	
	FIL memcard_fat;		// FAT filesystem memory card file handler
	lfs_file_t memcard_lfs;	// LittleFS filesystem memory card file handler
	if(FR_OK == f_open(&memcard_fat, README_FILE_NAME, FA_CREATE_NEW | FA_WRITE)) {
		UINT bytes_written;
		char instructions[] = README_INSTRUCTIONS_TEXT;
		f_write(&memcard_fat, instructions, strlen(instructions), &bytes_written);
		f_chmod(README_FILE_NAME, AM_RDO, AM_RDO);
		f_close(&memcard_fat);
	} else {
		status = 3;
	}
	if(FR_OK == f_open(&memcard_fat, MC_FILE_NAME, FA_CREATE_NEW | FA_WRITE)) {
		UINT bytes_written;
		char instructions[] = MC_TEXT;
		f_write(&memcard_fat, instructions, strlen(instructions), &bytes_written);
		f_close(&memcard_fat);
		flash_dir_read(print_mc_namefiles);
		f_chmod(MC_FILE_NAME, AM_RDO, AM_RDO);
	} else {
		status = 4;
	}
		
	FRESULT fr = f_mkdir(UPLOADS_FOLDER);
	if (FR_OK != fr && FR_EXIST != fr) {
		status = 5;
	}
	fr = f_mkdir(DOWNLOADS_FOLDER);
	if (FR_OK != fr && FR_EXIST != fr) {
		status = 6;
	}
	return status;
}

static uint32_t RAM_disk_create_FatFs_from_LFS() {
	FATFS fs;
	uint32_t status = 0;

	/* Create and format FAT virtual disk */
	MKFS_PARM opt = {
		FM_ANY,
		1,  // number of FAT copies
		1,  // data alignment (in sectors)
		0,  // number of root dir entires (default 512)
		0   // cluster size (let FatFS decide)
	};

	DBG_VERBOSE("RAM_disk_create_FatFs_from_LFS\n");
	if(FR_OK == f_mkfs("", &opt, working_buffer, WORK_BUFF_SIZE)) {
		if(FR_OK == f_mount(&fs, "", 0)) {
			f_setlabel(VOLUME_LABEL);
			if(RAM_disk_create_FatFs_from_file()!=0) { 
				status = RAM_disk_create_default_FatFs();
			}
			import_lfs_completed = true;
			f_unmount("");
		} else {
			status = 2;	// failed to mount virtual disk
		}
	} else {
		status = 1;	// failed to format virtual disk
	}
	DBG_VERBOSE("RAM_disk_create_FatFs_from_LFS status=%d", status);
	return status;
}

static int iterate_over_fatfs(char *folder, int (*callback)(FILINFO *fno)) {
	DIR dir;
	FILINFO fno;
	FRESULT res;
	uint32_t status = 0;

	res = f_opendir(&dir, folder);
	if (res != FR_OK) {
		status = 1;
		return status;
	}

	while (1) {
		res = f_readdir(&dir, &fno);
		if (res != FR_OK || fno.fname[0] == 0) {
			status = 2;
			break;
		}

		// Skip directories
		if (fno.fattrib & AM_DIR)
			continue;

		status = callback(&fno);
		if(status) {
			break;
		}
	}

	f_closedir(&dir);
	return status;
}

static int copy_mcr_file_to_lfs(const char *name)
{
	FIL memcard_fat;		// FAT filesystem memory card file handler
	int status = 1;
	int lfs_status = 1;
	lfs_t lfs;
	lfs_file_t memcard_lfs;	// LittleFS filesystem memory card file handler


	char path[256] = UPLOADS_FOLDER;
	char fullpath[512];

    snprintf(fullpath, sizeof(fullpath), "%s/%s", path, name);
    
	if(FR_OK == f_open(&memcard_fat, fullpath, FA_READ)) {
		lfs_status = lfs_mount(&lfs, &LFS_CFG);
		if(LFS_ERR_OK == lfs_status) {
			/* Prepare LFS memory card */
			lfs_remove(&lfs, name);	// remove old memory card file
			if(LFS_ERR_OK == lfs_file_open(&lfs, &memcard_lfs, name, LFS_O_RDWR | LFS_O_CREAT)) {
				
				/* Import virtual disk memory card to LFS */
				while(true) {
					UINT bytes_read;
					if(FR_OK != f_read(&memcard_fat, working_buffer, WORK_BUFF_SIZE, &bytes_read)) {
						status = 3;	// error during virtual disk read
						break;
					}
					if(lfs_file_write(&lfs, &memcard_lfs, working_buffer, bytes_read) < 0) {
						status = 4;	/// error during LFS write
						break;	
					}
					if(bytes_read < WORK_BUFF_SIZE) {
						status = 0;
						break;	// reached EOF, copy completed
					}
				}
				lfs_file_sync(&lfs, &memcard_lfs);
				lfs_file_close(&lfs, &memcard_lfs);
			} else {
				status = 2;	// failed to open/create new memory card file
			}
			lfs_unmount(&lfs);
		} else {
			status = 1;	// unable to mount LFS
		}
		f_close(&memcard_fat);
	}
    DBG_VERBOSE("copy_mcr_file_to_lfs %s return = %d lfs_status %d", name, status, lfs_status);
	
    return status;
}

static int filename_to_mc_slot(char *filename) {
	int n;
	char *end;
	long lvalue = strtol(filename, &end, 10);
	int value = -1;
	
	if(lvalue <= 0) {
		value = -1;
	} else if (*end == '\0' || strcasecmp(end, ".mcr") == 0) { 
		value = lvalue>0 && lvalue<=MAX_MC_IMAGES ? (int)lvalue:-1;
	}
	return value;
}
static int check_and_copy_mcr(FILINFO *fno) {
	int status;
	if (has_mcr_extension(fno->fname) &&
	 	filename_to_mc_slot(fno->fname)>0 && 
		fno->fsize >= MC_SIZE) {
			status = copy_mcr_file_to_lfs(fno->fname);
	} else {
			status = 1;
	}
	return status;
}

static uint32_t RAM_disk_sync_upload_folder() {
	return  iterate_over_fatfs(UPLOADS_FOLDER, check_and_copy_mcr);
}

static int create_retrieve_file(FILINFO *fno) {
	int status = 0;
	int value = filename_to_mc_slot(fno->fname);
	
	if (value>0) { // If the filename is a Number greater than 0
		lfs_t lfs;
		lfs_file_t file_lfs;
		if(LFS_ERR_OK == lfs_mount(&lfs, &LFS_CFG)) {
			lfs_remove(&lfs, DOWNLOAD_FILE);	// remove old retrieve file
			if(LFS_ERR_OK == lfs_file_open(&lfs, &file_lfs, DOWNLOAD_FILE, LFS_O_RDWR | LFS_O_CREAT)) {
				if(lfs_file_write(&lfs, &file_lfs, &value, sizeof(int)) < 0) {
					status = 4;	/// error during LFS write
				}
				lfs_file_close(&lfs, &file_lfs);
				status = 0;
			}
			lfs_unmount(&lfs);
		} else {
			status = 3;	// failed to open new file
		}
	}

	return status;
}

static uint32_t RAM_disk_sync_download_folder() {
	return iterate_over_fatfs(DOWNLOADS_FOLDER, create_retrieve_file);
}


static uint32_t RAM_disk_sync_LFS_from_FatFs() {
	uint32_t status = 0;
	FATFS fs;
	
	DBG_VERBOSE("RAM_disk_sync_LFS_from_FatFs");

	if(FR_OK == f_mount(&fs, "", 0)) {
		status = RAM_disk_sync_upload_folder();
		status |= RAM_disk_sync_download_folder();

		f_unmount("");
	}
	DBG_VERBOSE("RAM_disk_sync_LFS_from_FatFs status=%d", status);

	return status;
}
