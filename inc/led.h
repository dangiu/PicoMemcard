#ifndef __LED_H__
#define __LED_H__

#include "pico/stdlib.h"

void led_init();
void led_output_sync_status(bool out_of_sync);
void led_blink_error(int amount);

#endif