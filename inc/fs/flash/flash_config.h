#pragma once

#include "config.h"
#include "ff.h"

#define UPLOADS_FOLDER	    "UPLOAD"
#define DOWNLOADS_FOLDER	"DOWNLOAD"
#define DOWNLOAD_FILE	    "RETRIEVE.meta"
#define MC_FILE_NAME	    "MC.txt"	// name of the file with the instructions
#define README_FILE_NAME	"README.txt"	// name of the file with the instructions
#define README_INSTRUCTIONS_TEXT \
	"=============\n"	 \
	"PicoMemCard++\n" \
	"=============\n" \
	"  1. Copying files to the 'Memory Card'\n" \
	"  -------------------------------------\n" \
	"\n" \
	"\n" \
	"  This system allows users to upload files with the \".MCR\" extension into the \"UPLOAD\" folder.\n" \
	"\n" \
	"  File naming requirements:\n" \
	"\n" \
    "    The file name must consist of a number followed by the \".MCR\" extension.\n" \
    "    Example: 1.mcr, 2.mcr, ..., 10.mcr\n" \
	"\n" \
	"  Behavior:\n" \
    "    When a file is uploaded, it will automatically overwrite the corresponding memory slot associated with that number.\n" \
	"\n" \
	"\n" \
	"  2. Copying files from the 'Memory Card'\n" \
	"  ---------------------------------------\n" \
	"  Downloading memory slots:\n" \
	"\n" \
    "    If the user wants to download a memory card image, they must copy a file into the \"DOWNLOAD\" folder.\n" \
	"\n" \
    "    The file name must be the number of the slot to download (without extension).\n" \
    "  Example: \n" \
	"    To download memory card 2, copy a file named \"2\" into the DOWNLOADS folder.\n" \
	"\n" \
	"  Next time you plug the device into the computer, the MCR file will be accesible.\n"

#define MC_TEXT \
    "The FLASH contains the following .MCR images:\n\n"


#ifdef __cplusplus
extern "C" {
#endif
    void flash_init(uint8_t num);
    void flash_format();
    uint32_t flash_mount(uint8_t num);
    void flash_umount(uint8_t lun);
    bool flash_startstop(bool start, bool load_eject);
    bool flash_ready(void *dev);

    void* flash_get_by_num(uint8_t lun);
    uint32_t flash_get_sectors(uint8_t lun);
    uint16_t flash_get_block_size(uint8_t lun);

    uint32_t flash_read(uint8_t* data, uint32_t *size, uint8_t* file_name, uint32_t max_size);
    uint32_t flash_read_block(void *dev, uint8_t* buff, uint32_t sector, uint32_t count);
    uint32_t flash_write(uint8_t* data, uint32_t size, uint8_t* file_name, uint32_t *written);
    uint32_t flash_write_at(uint8_t* data, uint32_t size, uint32_t offset, uint8_t* file_name, uint32_t *written);
    uint32_t flash_write_block(void *dev, uint8_t* buff, uint32_t sector, uint32_t count);

    void flash_try_flush(uint8_t* file_name);
    void flash_dir_read(void (*callback)(unsigned char *filename, uint32_t size));
#ifdef __cplusplus
}
#endif
