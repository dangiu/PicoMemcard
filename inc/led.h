#ifndef __LED_H__
#define __LED_H__

#include "pico/stdlib.h"

void led_init();
void led_output_sync_status(bool out_of_sync);
void led_blink(int amount, int on_ms, int off_ms);
void led_blink_fast(int amount);
void led_blink_normal(int amount);
void led_blink_slow(int amount);

#endif