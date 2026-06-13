
#include "fs/fs.h"
#include "fs/sd/sd_config.h"
#include "fs/flash/flash_config.h"

#ifdef USE_SDCARD
struct fs_manager_t fs_manager = {
    .init  = sd_init,
    .mount = sd_mount,
    .umount = NULL,
    .startstop = sd_startstop,
    .ready = sd_ready,
    .get_by_num = sd_ext_get_by_num,
    .get_sectors = sd_get_sectors,
    .get_block_size = sd_get_block_size,
    .read = sd_read,
    .write = sd_write,
    .write_at = sd_write_at,
    .try_flush = NULL,
    .dir_read = sd_dir_read
};
#else
struct fs_manager_t fs_manager = {
    .init  = flash_init,
    .mount = flash_mount,
    .umount = flash_umount,
    .startstop = flash_startstop,
    .ready = flash_ready,
    .get_by_num = flash_get_by_num,
    .get_sectors = flash_get_sectors,
    .get_block_size = flash_get_block_size,
    .read = flash_read,
    .write = flash_write,
    .write_at = flash_write_at,
    .try_flush = flash_try_flush,
    .dir_read = flash_dir_read
};
#endif