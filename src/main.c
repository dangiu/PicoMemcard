#include "pico/stdio.h"
#include "pico/stdlib.h"
/* SD Card */
#include "sd_config.h"
/* Time and Timestamps */
#include "pico/time.h"
/* TinyUSB */
#include "bsp/board.h"
#include "tusb.h"
/* Memcard Simulation */
#include "memcard_simulator.h"
/* LED Control */
#include "led.h"
/* Global Configuration */
#include "config.h"
/* LCD Files */
#include "DEV_Config.h"
#include "LCD_0in96.h"


bool tud_mount_status = false;

void cdc_task(void);

/*------------- MAIN -------------*/
int main(void) {
	stdio_init_all();
	led_init();
	
	/* Pico connected to PC, initialize USB transfer mode */
	board_init();
	tusb_init();

	while(true) {
		tud_task(); // tinyusb device task
		cdc_task();

		LCD_0IN96_Clear(Green);
		
		if(to_ms_since_boot(get_absolute_time()) > TUD_MOUNT_TIMEOUT && !tud_mount_status)
			break;
	}
	
	/* Pico powered by PSX, initialize memory card simulation */
	simulate_memory_card();	

	LCD_0IN96_Clear(Blue);
	
	return 0;
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void) {
	tud_mount_status = true;
	/* Initialize SD card */
	sd_card_t *p_sd = sd_get_by_num(0);
	if (!p_sd) return;
	sd_init_card(p_sd);
}

// Invoked when device is unmounted
void tud_umount_cb(void) {}

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
void cdc_task(void) {
	// connected() check for DTR bit
	// Most but not all terminal client set this when making connection
	// if ( tud_cdc_connected() )
	{
		// connected and there are data available
		if ( tud_cdc_available() )
		{
			// read datas
			char buf[64];
			uint32_t count = tud_cdc_read(buf, sizeof(buf));
			(void) count;

			// Echo back
			// Note: Skip echo by commenting out write() and write_flush()
			// for throughput test e.g
			//    $ dd if=/dev/zero of=/dev/ttyACM0 count=10000
			tud_cdc_write(buf, count);
			tud_cdc_write_flush();
		}
	}
}

// Invoked when cdc when line state changed e.g connected/disconnected
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts) {
	(void) itf;
	(void) rts;
}

// Invoked when CDC interface received data from host
void tud_cdc_rx_cb(uint8_t itf) {
	(void) itf;
}
