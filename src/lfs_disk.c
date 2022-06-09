#include "lfs_disk.h"

const struct lfs_config LFS_CFG = {
	// block device operations
	.read  = lfs_flash_read,
	.prog  = lfs_flash_prog,
	.erase = lfs_flash_erase,
	.sync  = lfs_flash_sync,

	// block device configuration
	.read_size = 256,
	.prog_size = 256,
	.block_size = LFS_BLOCK_SIZE,
	.block_count = LFS_BLOCK_COUNT,	// 4096 * 64 = 256KB
	.cache_size = 256,
	.lookahead_size = 64,
	.block_cycles = 500,
};

__attribute__((section(".lfs"))) const uint8_t LFS_SPACE[LFS_BLOCK_COUNT * LFS_BLOCK_SIZE];	// space allocated to LFS