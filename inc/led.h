#ifndef __LED_H__
#define __LED_H__

#include "pico/stdlib.h"

void led_init();
void led_output_sync_status(bool out_of_sync);
void led_blink_error(int amount);
void led_output_mc_change();
void led_output_end_mc_list();
void led_output_new_mc();

int32_t is_pico_w();
void init_led(uint32_t pin);
void set_led(uint32_t pin, uint32_t level);

#endif