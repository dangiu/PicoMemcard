#ifndef __LED_H__
#define __LED_H__

#include "pico/stdlib.h"

void led_output_sync_status(bool out_of_sync);
void blink(int amount, int on_ms, int off_ms);
void blink_fast(int amount);
void blink_normal(int amount);
void blink_slow(int amount);

#endif