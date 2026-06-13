#ifndef __LED_H__
#define __LED_H__

#include "pico/stdlib.h"

#define NCOLORS       5

typedef struct {
    uint8_t r, g, b;
} rgb_t;

#define COLOR_GREEN    ((rgb_t){0, 255,   0})
#define COLOR_BLUE     ((rgb_t){0,   0, 255})
#define COLOR_CYAN     ((rgb_t){0, 255, 255})
#define COLOR_MAGENTA  ((rgb_t){255, 0, 255})
#define COLOR_YELLOW   ((rgb_t){255, 255, 0})


void led_init();
void led_output_sync_status(bool out_of_sync);
void led_blink_error(int amount);
void led_output_mc_change();
void led_output_end_mc_list();
void led_output_new_mc();

int32_t is_pico_w();
void init_led(uint32_t pin);
void set_led(uint32_t pin, uint32_t level);
void set_led_color(rgb_t *color);

#endif