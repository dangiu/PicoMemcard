#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "sd_config.h"
#include "sd_card.h"
#include "ff.h"

#include "memcard_simulator.h"

int main(void) {
	stdio_init_all();

	printf("Start\n");

	// Mount the SD card
	sd_card_t *pSD = sd_get_by_num(0);
	f_mount(&pSD->fatfs, "", 1);
	
	simulate_memory_card();

	while (1) {}
}
