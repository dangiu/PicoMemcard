#include "pico/stdio.h"
#include "pico/stdlib.h"
/* File system */
#include "fs/fs.h"
#include "fs/flash/flash_config.h"
/* Time and Timestamps */
#include "pico/time.h"
/* TinyUSB */
#include "bsp/board.h"
#include "tusb.h"
/* Memcard Simulation */
#include "memcard_simulator.h"
#include "memcard_manager.h"
#include "memory_card.h"
/* LED Control */
#include "led.h"
/* Global Configuration */
#include "config.h"
#include "pico/bootrom.h"

bool tud_mount_status = false;

void cdc_task(void);

/*------------- MAIN -------------*/
int main(void) {
	stdio_init_all();
	led_init();
	
	/* Pico connected to PC, initialize USB transfer mode */
	board_init();
	tusb_init();

	if(fs_manager.init) fs_manager.init(0);

	while(true) {
		tud_task(); // tinyusb device task
		cdc_task();

		if(to_ms_since_boot(get_absolute_time()) > TUD_MOUNT_TIMEOUT && !tud_mount_status)
			break;
	}
	
	/* Pico powered by PSX, initialize memory card simulation */
	simulate_memory_card();	

	return 0;
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void) {
	/* Initialize SD card/flash */
	if(fs_manager.mount) {
		tud_mount_status = true;
		fs_manager.mount(0);
	}
}

// Invoked when device is unmounted
void tud_umount_cb(void) {
	if(fs_manager.umount) {
		fs_manager.umount(0);
	}
	tud_mount_status = false;
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en) {
	(void) remote_wakeup_en;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void) {}

//--------------------------------------------------------------------+
// USB CDC
//--------------------------------------------------------------------+

#define CMD_BUF_SIZE 64

static char cmd_buf[CMD_BUF_SIZE];
static uint8_t cmd_len = 0;
static bool prompt_shown = false;

void show_prompt(void) {
    if (tud_cdc_connected()) {
        tud_cdc_write_str("> ");
        tud_cdc_write_flush();
        prompt_shown = true;
    }
}


void print_files(uint8_t* filename, uint32_t fsize) {
	DBG_INFO("  %s (%dB)", filename, fsize);
}

void handle_command(const char *cmd) {
    if (strcmp(cmd, "bootsel") == 0) {
        DBG_INFO("\nRebooting to BOOTSEL...");
        sleep_ms(100); // allow flush
        reset_usb_boot(0, 0);
    } else if (strcmp(cmd, "ls") == 0) {
	    fs_manager.dir_read(print_files);
    } else if (strcmp(cmd, "mount") == 0) {
        tud_mount_cb();
    } else if (strcmp(cmd, "umount") == 0) {
        tud_umount_cb();
    #ifndef USE_SDCARD
    } else if (strcmp(cmd, "format") == 0) {
        flash_format();
    #endif
    } else if (strcmp(cmd, "mc") == 0) {
        uint8_t mc_file_name[MAX_MC_FILENAME_LEN + 1];
        memory_card_t mc;
        int status;
        status = memory_card_init(&mc);
        status = memcard_manager_get_initial(mc_file_name);
        DBG_INFO("memcard_manager_get_initial: %s (status=%d)", mc_file_name, status);
	    status = memory_card_import(&mc, mc_file_name);
        DBG_INFO("memory_card_import (status=%d)", status);
    } else if (strcmp(cmd, "sim") == 0) {
        simulate_memory_card();
    } else if (strcmp(cmd, "led") == 0) {
        led_blink_error(2);
    } else if (strcmp(cmd, "last") == 0) {
        int last = memcard_manager_get_prev_loaded_memcard_index();
        DBG_INFO("Last memcard: %d", last);
    } else if (strcmp(cmd, "new") == 0) {
	    uint8_t name[MAX_MC_FILENAME_LEN + 1];
        uint32_t status;
        status = memcard_manager_create(name);
        DBG_INFO("memcard_manager_create (status=%d) name =%s", status, name);
    } else if (strcmp(cmd, "count") == 0) {
        DBG_INFO("memcard_manager_count: %d", memcard_manager_count_with_err_size());
    } else if (strcmp(cmd, "help") == 0) {
        DBG_INFO("\nCommands:");
        DBG_INFO("  help     - show this message");
        DBG_INFO("  bootsel  - reboot to BOOTSEL");
        DBG_INFO("  ls       - list files in the filesystem");
        DBG_INFO("  mount    - mount the FAT32 filesystem");
        DBG_INFO("  umount   - umount the FAT32 filesystem");
    } else if (strlen(cmd) == 0) {
        // ignore empty
    } else {
        DBG_INFO("\nUnknown: %s", cmd);
    }
}

void cdc_task(void) {
    while (tud_cdc_available()) {
        char c;
        tud_cdc_read(&c, 1);

        // ENTER
        if (c == '\r' || c == '\n') {
            tud_cdc_write_str("\r");

            cmd_buf[cmd_len] = '\0';
            handle_command(cmd_buf);

            cmd_len = 0;
            prompt_shown = false;
        }
        // BACKSPACE
        else if (c == 0x7F || c == '\b') {
            if (cmd_len > 0) {
                cmd_len--;
                tud_cdc_write_str("\b \b"); // erase char on terminal
            }
        }
        // NORMAL CHAR
        else if (cmd_len < CMD_BUF_SIZE - 1) {
            cmd_buf[cmd_len++] = c;
            tud_cdc_write(&c, 1); // echo
        }
    }

    if (!prompt_shown && tud_cdc_connected()) {
        show_prompt();
    }
}

// Invoked when cdc when line state changed e.g connected/disconnected
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts) {
	(void) itf;
	(void) rts;
}


void tud_cdc_rx_cb(uint8_t itf) {
    (void) itf;
}
