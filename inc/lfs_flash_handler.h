#ifndef __LFS_FLASH_HANDLER_H__
#define __LFS_FLASH_HANDLER_H__

#include "lfs.h"

int lfs_flash_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size);
int lfs_flash_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size);
int lfs_flash_erase(const struct lfs_config *c, lfs_block_t block);
int lfs_flash_sync(const struct lfs_config *c);

#endif