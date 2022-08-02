#include "ram_disk.h"
#include "ff.h"
#include "lfs.h"
#include "lfs_disk.h"
#include "memory_card.h"
#include "config.h"

#define WORK_BUFF_SIZE 1024

uint8_t* ram_disk = NULL;	// leave empty, allocate memory on initialization

/* FatFS Functions */
uint32_t RAM_disk_status() {
	if(ram_disk == NULL)
		return STA_NOINIT;
	else
		return 0;
}

uint32_t RAM_disk_initialize() {
	if(ram_disk == NULL)
		ram_disk = malloc(sizeof(uint8_t) * DISK_BLOCK_NUM * DISK_BLOCK_SIZE);
	return RAM_disk_status();
}

uint32_t RAM_disk_deinitialize() {
	free(ram_disk);
	ram_disk = NULL;
	return  RAM_disk_status();
}

uint32_t RAM_disk_read(uint8_t* buff, uint32_t sector, uint32_t count) {
	if(ram_disk == NULL)
		return RES_NOTRDY;
	if(sector < 0 || sector >= SECTOR_NUM) {
		return RES_PARERR;
	}
	/* copy data to buffer */
	uint32_t buff_index = 0;
	for(uint32_t i = 0; i < count; ++i) {
		for(uint32_t j = 0; j < SECTOR_SIZE; ++j) {
			buff[buff_index] = ram_disk[((sector + i) * SECTOR_SIZE) + j];
			++buff_index;
		}
	}
	return RES_OK;
}

uint32_t RAM_disk_write(const uint8_t* buff, uint32_t sector, uint32_t count) {
	if(ram_disk == NULL)
		return RES_NOTRDY;
	if(sector < 0 || sector >= SECTOR_NUM) {
		return RES_PARERR;
	}
	/* copy data to buffer */
	uint32_t buff_index = 0;
	for(uint32_t i = 0; i < count; ++i) {
		for(uint32_t j = 0; j < SECTOR_SIZE; ++j) {
			ram_disk[((sector + i) * SECTOR_SIZE) + j] = buff[buff_index];
			++buff_index;
		}
	}
	return RES_OK;
}

uint32_t RAM_disk_ioctl(uint8_t cmd, void* buff) {
	if(ram_disk == NULL)
		return RES_NOTRDY;
	switch(cmd) {
		case CTRL_SYNC:
			return RES_OK;	// no cache, no need to sync
		case GET_SECTOR_COUNT:
			*(LBA_t*) buff = SECTOR_NUM;
			return RES_OK;
		case GET_SECTOR_SIZE:
			*(WORD*) buff = SECTOR_SIZE;
			return RES_OK;
		case GET_BLOCK_SIZE:
			*(DWORD*) buff = 1;	// not a flash storage device, can erase each sector individualy
			return RES_OK;
		case CTRL_TRIM:
			return RES_OK;	// not a flash storage device, we don't need to do anything
		default:
			return RES_PARERR;	// no other commands are supported
	}
}

uint32_t RAM_disk_import_lfs_memcard() {
	uint32_t status = 0;
	FATFS fs;
	lfs_t lfs;	
	uint8_t working_buffer[WORK_BUFF_SIZE];
	FIL memcard_fat;		// FAT filesystem memory card file handler
	lfs_file_t memcard_lfs;	// LittleFS filesystem memory card file handler

	/* Create and format FAT virtual disk */
	MKFS_PARM opt = {
		FM_ANY,
		1,  // number of FAT copies
		1,  // data alignment (in sectors)
		0,  // number of root dir entires (default 512)
		0   // cluster size (let FatFS decide)
	};

	if(FR_OK == f_mkfs("", &opt, working_buffer, WORK_BUFF_SIZE)) {
		if(FR_OK == f_mount(&fs, "", 0)) {
			f_setlabel(VOLUME_LABEL);
			
			/* Check stored memory card size */
			if(LFS_ERR_OK == lfs_mount(&lfs, &LFS_CFG)) {
				if(LFS_ERR_OK == lfs_file_open(&lfs, &memcard_lfs, MEMCARD_FILE_NAME, LFS_O_RDWR)) {
					lfs_soff_t memcard_lfs_size = lfs_file_size(&lfs, &memcard_lfs); 
					if(memcard_lfs_size > 0 && memcard_lfs_size <= MC_SIZE) {
						
						/* Mirror stored memory card to virtual disk (only if size is correct) */
						if(FR_OK == f_open(&memcard_fat, MEMCARD_FILE_NAME, FA_CREATE_NEW | FA_WRITE)) {
							while(true) {
								int bytes = lfs_file_read(&lfs, &memcard_lfs, working_buffer, WORK_BUFF_SIZE);
								if(bytes < 0) {
									status = 1;	// error during LFS read
									break;
								} else {
									UINT bytes_written;
									if(FR_OK != f_write(&memcard_fat, working_buffer, bytes, &bytes_written)) {
										status = 1;	// error during virtual disk write
										break;
									}
									if(bytes < WORK_BUFF_SIZE) {
										break;	// reached EOF
									}
								}
							}
							f_close(&memcard_fat);
						} else {
							status = 1;
						}
					}
					lfs_file_close(&lfs, &memcard_lfs);
				} else {
					status = 1;	// failed to open memory card
				}
				lfs_unmount(&lfs);
			} else {
				status = 1;	// failed to mount LFS
			}
			f_unmount("");
		} else {
			status = 1;	// failed to mount virtual disk
		}
	} else {
		status = 1;	// failed to format virtual disk
	}
	return status;
}

uint32_t RAM_disk_export_lfs_memcard() {
	uint32_t status = 0;
	FATFS fs;
	lfs_t lfs;	
	uint8_t working_buffer[WORK_BUFF_SIZE];
	FIL memcard_fat;		// FAT filesystem memory card file handler
	lfs_file_t memcard_lfs;	// LittleFS filesystem memory card file handler
	
	if(FR_OK == f_mount(&fs, "", 0)) {
		if(FR_OK == f_open(&memcard_fat, MEMCARD_FILE_NAME, FA_READ)) {

			/* Check virtual disk memory card size */
			FSIZE_t fat_memcard_size = f_size(&memcard_fat);
			if(fat_memcard_size <= MC_SIZE) {
				if(LFS_ERR_OK == lfs_mount(&lfs, &LFS_CFG)) {
					/* Prepare LFS memory card */
					lfs_remove(&lfs, MEMCARD_FILE_NAME);	// remove old memory card file
					if(LFS_ERR_OK == lfs_file_open(&lfs, &memcard_lfs, MEMCARD_FILE_NAME, LFS_O_RDWR | LFS_O_CREAT)) {
						
						/* Import virtual disk memory card to LFS */
						while(true) {
							UINT bytes_read;
							if(FR_OK != f_read(&memcard_fat, working_buffer, WORK_BUFF_SIZE, &bytes_read)) {
								status = 1;	// error during virtual disk read
								break;
							}
							if(lfs_file_write(&lfs, &memcard_lfs, working_buffer, bytes_read) < 0) {
								status = 1;	/// error during LFS write
								break;	
							}
							if(bytes_read < WORK_BUFF_SIZE)
								break;	// reached EOF, copy completed
						}
						lfs_file_close(&lfs, &memcard_lfs);
					} else {
						status = 1;	// failed to open/create new memory card file
					}
					lfs_unmount(&lfs);
				} else {
					status = 1;	// unable to mount LFS
				}
			} else {
				status = 1;	// image has incorrect size
			}
			f_close(&memcard_fat);
		} else {
			status = 1;	// unable to find memory card on virtual disk
		}
		f_unmount("");
	} else {
		status = 1;	// unable to mount virtual disk
	}
	return status;
}