#include "led.h"
#include "hardware/gpio.h"

#define PICO_LED_PIN 25

void led_init() {

}

void led_output_sync_status(bool out_of_sync) {
	gpio_put(PICO_LED_PIN, !out_of_sync);
}

void led_blink(int amount, int on_ms, int off_ms) {
	bool initial_state = gpio_get(PICO_LED_PIN);
	if(initial_state) {
		/* turn led off if already on */
		gpio_put(PICO_LED_PIN, false);
		sleep_ms(off_ms);
		--amount;	// last blink is performed while restoring led state
	}
	for(int i = 0; i < amount; ++i) {
		gpio_put(PICO_LED_PIN, true);
		sleep_ms(on_ms);
		gpio_put(PICO_LED_PIN, false);
		sleep_ms(off_ms);
	}
	if(initial_state) {
		/* restore led state */
		gpio_put(PICO_LED_PIN, true);
	}
}

void led_blink_fast(int amount) {
	led_blink(amount, 250, 250);
}

void led_blink_normal(int amount) {
	led_blink(amount, 500, 500);
}

void led_blink_slow(int amount) {
	led_blink(amount, 1000, 1000);
}