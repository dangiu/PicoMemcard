#include "led.h"
#include "config.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#ifdef RP2040ZERO
#include "ws2812.pio.h"
#endif

#ifdef PICO
#define PICO_LED_PIN 25
#endif

#ifdef PICO_RGB
#define PICO_RED_PIN 25
#define PICO_GRN_PIN 24
#define PICO_BLU_PIN 23
#endif

static uint smWs2813;
static uint offsetWs2813;

#ifdef RP2040ZERO
void ws2812_put_pixel(uint32_t pixel_grb) {
	sleep_ms(1);	// delay to ensure LED latch will hold data
	pio_sm_put(pio1, smWs2813, pixel_grb << 8u);
}
void ws2812_put_rgb(uint8_t red, uint8_t green, uint8_t blue) {
	uint32_t mask = (green << 16) | (red << 8) | (blue << 0);
	ws2812_put_pixel(mask);
}
#endif

void led_init() {
	#ifdef RP2040ZERO
	offsetWs2813 = pio_add_program(pio1, &ws2812_program);
	smWs2813 = pio_claim_unused_sm(pio1, true);
	ws2812_program_init(pio1, smWs2813, offsetWs2813, 16, 800000, true);
	#endif
        #ifdef PICO_RGB
        gpio_init(PICO_RED_PIN);
        gpio_init(PICO_GRN_PIN);
        gpio_init(PICO_BLU_PIN);
        gpio_set_dir(PICO_RED_PIN, GPIO_OUT); 
        gpio_set_dir(PICO_GRN_PIN, GPIO_OUT); 
        gpio_set_dir(PICO_BLU_PIN, GPIO_OUT); 
        #endif
}

void led_output_sync_status(bool out_of_sync) {
	#ifdef PICO
	gpio_put(PICO_LED_PIN, !out_of_sync);
	#endif
	#ifdef RP2040ZERO
	if(out_of_sync)
		ws2812_put_rgb(255, 200, 0);
        else
		ws2812_put_rgb(0, 255, 0);
	#endif
        #ifdef PICO_RGB
        if(out_of_sync) {
                //yellow
                gpio_put(PICO_RED_PIN, 1);
                gpio_put(PICO_GRN_PIN, 1);
                gpio_put(PICO_BLU_PIN, 0);
        }
        else {
                //green
                gpio_put(PICO_RED_PIN, 0);
                gpio_put(PICO_GRN_PIN, 1);
                gpio_put(PICO_BLU_PIN, 0);
        }
        #endif
}

void led_blink_error(int amount) {
	/* ensure led is off */
	#ifdef PICO
	gpio_put(PICO_LED_PIN, false);
	#endif
	#ifdef RP2040ZERO
	ws2812_put_rgb(0, 0, 0);
	#endif
        #ifdef PICO_RGB
        gpio_put(PICO_RED_PIN, 0);
        gpio_put(PICO_GRN_PIN, 0);
        gpio_put(PICO_BLU_PIN, 0);
        #endif
	sleep_ms(500);
	/* start blinking */
	for(int i = 0; i < amount; ++i) {
		#ifdef PICO
		gpio_put(PICO_LED_PIN, true);
		#endif
		#ifdef RP2040ZERO
		ws2812_put_rgb(255, 0, 0);
		#endif
                #ifdef PICO_RGB
                gpio_put(PICO_RED_PIN, 1);
                gpio_put(PICO_GRN_PIN, 0);
                gpio_put(PICO_BLU_PIN, 0);
                #endif
		sleep_ms(500);
		#ifdef PICO
		gpio_put(PICO_LED_PIN, false);
		#endif
		#ifdef RP2040ZERO
		ws2812_put_rgb(0, 0, 0);
		#endif
                #ifdef PICO_RGB
                gpio_put(PICO_RED_PIN, 0);
                gpio_put(PICO_GRN_PIN, 0);
                gpio_put(PICO_BLU_PIN, 0);
                #endif
		sleep_ms(500);
	}
}

void led_output_mc_change() {
	#ifdef PICO
	gpio_put(PICO_LED_PIN, false);
	sleep_ms(100);
	gpio_put(PICO_LED_PIN, true);
	sleep_ms(100);
	gpio_put(PICO_LED_PIN, false);
	sleep_ms(100);
	#endif
	#ifdef RP2040ZERO
	ws2812_put_rgb(0, 0, 255);
	sleep_ms(100);
	ws2812_put_rgb(0, 0, 0);
	#endif
        #ifdef PICO_RGB
        gpio_put(PICO_RED_PIN, 0);  // blue
        gpio_put(PICO_GRN_PIN, 0);
        gpio_put(PICO_BLU_PIN, 1);
	sleep_ms(100);
        gpio_put(PICO_RED_PIN, 0);
        gpio_put(PICO_GRN_PIN, 0);
        gpio_put(PICO_BLU_PIN, 0);
        #endif
}

void led_output_end_mc_list() {
	#ifdef PICO
	for(int i = 0; i < 3; ++i) {
		gpio_put(PICO_LED_PIN, false);
		sleep_ms(100);
		gpio_put(PICO_LED_PIN, true);
		sleep_ms(100);
		gpio_put(PICO_LED_PIN, false);
		sleep_ms(100);
	}
	#endif
	#ifdef RP2040ZERO
	ws2812_put_rgb(255, 96, 0);	// orange
	sleep_ms(500);
	ws2812_put_rgb(0, 0, 0);
	#endif
        #ifdef PICO_RGB
        gpio_put(PICO_RED_PIN, 1);      // purple
        gpio_put(PICO_GRN_PIN, 0);
        gpio_put(PICO_BLU_PIN, 1);
        sleep_ms(500);
        gpio_put(PICO_RED_PIN, 0);
        gpio_put(PICO_GRN_PIN, 0);
        gpio_put(PICO_BLU_PIN, 0);
        #endif
}

void led_output_new_mc() {
	#ifdef PICO
	for(int i = 0; i < 10; ++i) {
		gpio_put(PICO_LED_PIN, false);
		sleep_ms(50);
		gpio_put(PICO_LED_PIN, true);
		sleep_ms(50);
		gpio_put(PICO_LED_PIN, false);
		sleep_ms(50);
	}
	#endif
	#ifdef RP2040ZERO
	ws2812_put_rgb(52, 171, 235);	// light blue
	sleep_ms(1000);
	ws2812_put_rgb(0, 0, 0);
	#endif
        #ifdef PICO_RGB
        gpio_put(PICO_RED_PIN, 0);      // teal
        gpio_put(PICO_GRN_PIN, 1);
        gpio_put(PICO_BLU_PIN, 1);
        sleep_ms(1000);
        gpio_put(PICO_RED_PIN, 0);
        gpio_put(PICO_GRN_PIN, 0);
        gpio_put(PICO_BLU_PIN, 0);
        #endif
}
