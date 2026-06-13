#ifndef __CONFIG_H__
#define __CONFIG_H__

#include "debug.h"
#include <stdint.h>

/* Alloc a giant block of 180KB */
#define SIZE_RAM_BUFFER 180 * 1024
extern uint8_t ram_disk[SIZE_RAM_BUFFER] __attribute__((section(".ram"), aligned(4096)));


/* Global configuration options for PicoMemcard */
#define TUD_MOUNT_TIMEOUT	3000			// max time (in ms) before giving up on MSC mode (USB) and starting memcard simulation
#define MSC_WRITE_SYNC_TIMEOUT 1 * 1000		// time (in ms) expired since last MSC write before exporting RAM disk into LFS
#define IDLE_AUTOSYNC_TIMEOUT 5 * 1000		// time (in ms) the memory card must be inactive before automatic sync from RAM to LFS
#define MAX_MC_FILENAME_LEN	32				// max length of memory card file name (including extension)
#define MC_RECONNECT_TIME	1000				// time (in ms) the memory card stays disconnected when simulating reconnection

#ifdef USE_SDCARD
#define MAX_MC_IMAGES	255					// maximum number of different mc images
#else
#ifdef USE_16MBFLASH
#define MAX_MC_IMAGES	100
#else
#define MAX_MC_IMAGES	10
#endif
#endif

/* Invert red and green. Uncomment this if the LED colours for your RP2040 Zero are incorrect. */
// #define INVERT_RED_GREEN

/* SD Card Configuration */
#define BLOCK_SIZE	512				// SD card communicate using only 512 block size for consistency
#define BAUD_RATE	5000 * 1000

#endif
