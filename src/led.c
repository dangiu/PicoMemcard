#include "led.h"
#include "config.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/adc.h"
#ifdef PICO
#include "pico/cyw43_arch.h"
#endif
#ifdef RP2040ZERO
#include "ws2812.pio.h"
#endif

#ifdef PICO
#define PICO_LED_PIN 25
#endif

static uint smWs2813;
static uint offsetWs2813;
static int32_t picoW = -1;

#ifdef RP2040ZERO
void ws2812_put_pixel(uint32_t pixel_grb) {
	sleep_ms(1);	// delay to ensure LED latch will hold data
	pio_sm_put(pio1, smWs2813, pixel_grb << 8u);
}
void ws2812_put_rgb(uint8_t red, uint8_t green, uint8_t blue) {
	#ifdef INVERT_RED_GREEN
	uint32_t mask = (red << 16) | (green << 8) | (blue << 0);
	#elif
	uint32_t mask = (green << 16) | (red << 8) | (blue << 0);
	#endif
	ws2812_put_pixel(mask);
}
#endif

void led_init() {
  #ifdef PICO
  if (is_pico_w()) {
    if (cyw43_arch_init()) {
      picoW = 0;
    }
  }
  init_led(PICO_LED_PIN);
  #endif
	#ifdef RP2040ZERO
	offsetWs2813 = pio_add_program(pio1, &ws2812_program);
	smWs2813 = pio_claim_unused_sm(pio1, true);
	ws2812_program_init(pio1, smWs2813, offsetWs2813, 16, 800000, true);
	#endif
}

void led_output_sync_status(bool out_of_sync) {
	#ifdef PICO
	set_led(PICO_LED_PIN, !out_of_sync);
	#endif
	#ifdef RP2040ZERO
	if(out_of_sync) {
		ws2812_put_rgb(255, 0, 0);
		sleep_ms(25);
	} else {
		ws2812_put_rgb(0, 255, 0);
	}
	#endif
}

void led_blink_error(int amount) {
	/* ensure led is off */
	#ifdef PICO
	set_led(PICO_LED_PIN, false);
	#endif
	#ifdef RP2040ZERO
	ws2812_put_rgb(0, 0, 0);
	#endif
	sleep_ms(500);
	/* start blinking */
	for(int i = 0; i < amount; ++i) {
		#ifdef PICO
		set_led(PICO_LED_PIN, true);
		#endif
		#ifdef RP2040ZERO
		ws2812_put_rgb(255, 0, 0);
		#endif
		sleep_ms(500);
		#ifdef PICO
		set_led(PICO_LED_PIN, false);
		#endif
		#ifdef RP2040ZERO
		ws2812_put_rgb(0, 0, 0);
		#endif
		sleep_ms(500);
	}
}

void led_output_mc_change() {
	#ifdef PICO
	set_led(PICO_LED_PIN, false);
	sleep_ms(100);
	set_led(PICO_LED_PIN, true);
	sleep_ms(100);
	set_led(PICO_LED_PIN, false);
	sleep_ms(100);
	#endif
	#ifdef RP2040ZERO
	ws2812_put_rgb(0, 0, 255);
	sleep_ms(100);
	ws2812_put_rgb(0, 0, 0);
	#endif
}

void led_output_end_mc_list() {
	#ifdef PICO
	for(int i = 0; i < 3; ++i) {
		set_led(PICO_LED_PIN, false);
		sleep_ms(100);
		set_led(PICO_LED_PIN, true);
		sleep_ms(100);
		set_led(PICO_LED_PIN, false);
		sleep_ms(100);
	}
	#endif
	#ifdef RP2040ZERO
	ws2812_put_rgb(255, 96, 0);	// orange
	sleep_ms(500);
	ws2812_put_rgb(0, 0, 0);
	#endif
}

void led_output_new_mc() {
	#ifdef PICO
	for(int i = 0; i < 10; ++i) {
		set_led(PICO_LED_PIN, false);
		sleep_ms(50);
		set_led(PICO_LED_PIN, true);
		sleep_ms(50);
		set_led(PICO_LED_PIN, false);
		sleep_ms(50);
	}
	#endif
	#ifdef RP2040ZERO
	ws2812_put_rgb(52, 171, 235);	// light blue
	sleep_ms(1000);
	led_disable();
	#endif
}

// Attempt to check if device is a Raspberry Pi Pico W
// Based on https://forums.raspberrypi.com/viewtopic.php?t=336884
int32_t is_pico_w() {
  if (picoW == -1) {
    // When the VSYS_ADC is read on a Pico it will always be > 1V
    // On a Pico-W it will be < 1V when GPIO25 is low
    #define VSYS_ADC_GPIO  29
    #define VSYS_ADC_CHAN   3
    #define ONE_VOLT      409 // 1V divide by 3, as 12-bit with 3V3 Vref
    #define VSYS_CTL_GPIO  25 // Enables VSYS_ADC on Pico-W
    adc_init();
    adc_gpio_init(VSYS_ADC_GPIO);
    adc_select_input(VSYS_ADC_CHAN);
    // Check to see if we can detect a Pico-W without using any GPIO
    if (adc_read() < ONE_VOLT) {
      picoW = 1;                             // Pico W
    }
    else {
      gpio_init(VSYS_CTL_GPIO);              // GPIO
      gpio_set_dir(VSYS_CTL_GPIO, GPIO_OUT); // output
      gpio_put(VSYS_CTL_GPIO, 0);            // low
      if (adc_read() < ONE_VOLT) {
          gpio_put(VSYS_CTL_GPIO, 1);        // high
          picoW = 1;                         // Pico W
      } else {
          picoW = 0;                         // Pico
      }
    }
  }
  return picoW;
}

void init_led(uint32_t pin) {
  if (!is_pico_w() || (pin != 25)) {
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_OUT);
  }
}

void set_led(uint32_t pin, uint32_t level) {
  if ((pin == 25) && is_pico_w()) {
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, level);
  } else {
    gpio_put(pin, level);
  }
}