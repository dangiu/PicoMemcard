#include "led.h"
#include "hardware/gpio.h"

#define LED_PIN 25

void led_output_sync_status(bool out_of_sync) {
	gpio_put(LED_PIN, !out_of_sync);
}

void blink(int amount, int on_ms, int off_ms) {
	bool initial_state = gpio_get(LED_PIN);
	if(initial_state) {
		/* turn led off if already on */
		gpio_put(LED_PIN, false);
		sleep_ms(off_ms);
		--amount;	// last blink is performed while restoring led state
	}
	for(int i = 0; i < amount; ++i) {
		gpio_put(LED_PIN, true);
		sleep_ms(on_ms);
		gpio_put(LED_PIN, false);
		sleep_ms(off_ms);
	}
	if(initial_state) {
		/* restore led state */
		gpio_put(LED_PIN, true);
	}
}

void blink_fast(int amount) {
	blink(amount, 250, 250);
}

void blink_normal(int amount) {
	blink(amount, 500, 500);
}

void blink_slow(int amount) {
	blink(amount, 1000, 1000);
}