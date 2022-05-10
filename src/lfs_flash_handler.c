#include "lfs_flash_handler.h"
#include "lfs_disk.h"
#include "hardware/sync.h"
#include "hardware/flash.h"

extern uint32_t __flash_binary_start;
extern uint32_t __lfs_start;

// Read a region in a block. Negative error codes are propagated
// to the user.
int lfs_flash_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size) {
	for(uint32_t i = 0; i < size; i++) {
		//uint8_t temp = *((uint8_t *)&__lfs_start);
		uint8_t temp = *((uint8_t*)(uint32_t)&__lfs_start + (block * LFS_BLOCK_SIZE + off + i));
		((uint8_t*) buffer)[i] = temp;
	}
	return LFS_ERR_OK;
}

// Program a region in a block. The block must have previously
// been erased. Negative error codes are propagated to the user.
// May return LFS_ERR_CORRUPT if the block should be considered bad.
int lfs_flash_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size) {
	uint32_t int_status = save_and_disable_interrupts();
	flash_range_program((uint32_t)&__lfs_start - (uint32_t)&__flash_binary_start + (block * LFS_BLOCK_SIZE) + off, (const uint8_t*) buffer, size);
	restore_interrupts(int_status);
	return LFS_ERR_OK;
}

// Erase a block. A block must be erased before being programmed.
// The state of an erased block is undefined. Negative error codes
// are propagated to the user.
// May return LFS_ERR_CORRUPT if the block should be considered bad.
int lfs_flash_erase(const struct lfs_config *c, lfs_block_t block) {
	uint32_t int_status = save_and_disable_interrupts();
	flash_range_erase((uint32_t)&__lfs_start - (uint32_t)&__flash_binary_start + (block * LFS_BLOCK_SIZE), LFS_BLOCK_SIZE);
	restore_interrupts(int_status);
	return LFS_ERR_OK;
}

// Sync the state of the underlying block device. Negative error codes
// are propagated to the user.
int lfs_flash_sync(const struct lfs_config *c) {
	/* Pico flash primitives ensure that the cache is always flushed after each write operation */
	return LFS_ERR_OK;
}