/*
* Portions of this file are from the Seeed Graphical Library.
*
* Copyright (c) 2022 Darren Anderson
* Copyright (c) 2014 seeed technology inc.
*
* The MIT License (MIT)
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
*/

#include <stdio.h>
#include "lcd.h"
#include "ssd1331.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "pmcsplash.h"
#include "font.h"

#define LCD_SCK 14
#define LCD_MOSI 15
#define LCD_MISO 12
#define LCD_CS 13
#define LCD_DC 11
#define LCD_RES 10

void lcd_enable_cs() {
    asm volatile("nop \n nop \n nop");
    gpio_put(LCD_CS, 0);
    asm volatile("nop \n nop \n nop");
}

void lcd_disable_cs() {
    asm volatile("nop \n nop \n nop");
    gpio_put(LCD_CS, 1);
    asm volatile("nop \n nop \n nop");
}

void lcd_enable_dc() {
    gpio_put(LCD_DC, 1);
}

void lcd_disable_dc() {
    gpio_put(LCD_DC, 0);
}

void lcd_spi_write(uint8_t data) {
    lcd_enable_cs();
    spi_write_blocking(spi1, &data, sizeof(uint8_t));
    lcd_disable_cs();
}

void lcd_spi_write16(uint16_t data) {
    lcd_enable_cs();
    spi_write16_blocking(spi1, &data, sizeof(uint16_t));
    lcd_disable_cs();
}

void lcd_enable_reset() {
    gpio_put(LCD_RES, 0);
}

void lcd_disable_reset() {
    gpio_put(LCD_RES, 1);
}

ssd1331_config_t lcd_config = {
    .spi_write = lcd_spi_write,
    .spi_write16 = lcd_spi_write16,
    .enable_dc = lcd_enable_dc,
    .disable_dc = lcd_disable_dc,
    .enable_reset = lcd_enable_reset,
    .disable_reset = lcd_disable_reset,
    .enable_cs = lcd_enable_cs,
    .disable_cs = lcd_disable_cs,
    .screen_width = 96,
    .screen_height = 64
};

void show_splash() {
    for(int y = 0; y < lcd_config.screen_height; y++) {
        for(int x = 0; x < lcd_config.screen_width; x++) {
            ssd1331_draw_pixel(x, y, RGB332_TO_RGB565(pmc_splash[x + (lcd_config.screen_width * y)]));
        }
    }
}

void lcd_init() {
    spi_init(spi1, 10 * 1000000); // 10MHz SPI clock
    spi_set_format(spi1, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    gpio_set_function(LCD_SCK, GPIO_FUNC_SPI);
    gpio_set_function(LCD_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(LCD_MISO, GPIO_FUNC_SPI);

    gpio_init(LCD_DC);
    gpio_set_dir(LCD_DC, GPIO_OUT);
    lcd_disable_dc();

    gpio_init(LCD_RES);
    gpio_set_dir(LCD_RES, GPIO_OUT);
    lcd_enable_reset();

    gpio_init(LCD_CS);
    gpio_set_dir(LCD_CS, GPIO_OUT);
    lcd_disable_cs();

    ssd1331_init(&lcd_config);

    // Clear the screen (will be filled with garbage on startup)
    ssd1331_clear_screen();

    show_splash();
    sleep_ms(2000);
    ssd1331_clear_screen();
}

void fill_rectangle(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t colour)
{
    for(uint16_t i = 0; i < height; i++){
        for(uint16_t j = 0; j < width; j++){
            ssd1331_draw_pixel(x+j,y+i,colour);
        }
    }
}

void draw_char(uint8_t ascii, uint16_t x, uint16_t y, uint16_t size, uint16_t colour)
{
    if((ascii<32)||(ascii>=127)){
        return;
    }

    for (int8_t i = 0; i < FONT_X; i++ ) {
        int8_t temp = lcd_font[ascii-0x20][i];
        int8_t inrun = 0;
        int8_t runlen = 0;
        int8_t endrun = 0;

        for(int8_t f = 0; f < FONT_Y; f++){
            if((temp>>f)&0x01){
                if (inrun) runlen += 1;
                else {
                    inrun = 1;
                    runlen = 1;
                }
            } else if (inrun) {
                endrun = 1;
                inrun = 0;
            }

            if (f == FONT_Y - 1 && inrun) {
                endrun = 1;
                // need the +1 b/c we this code is normally
                // only triggered  when f == FONT_Y, due to the
                // edge-triggered nature of this algorithm
                f += 1;
            }

            if (endrun) {
                fill_rectangle(x+i*size, y+(f-runlen)*size, size, runlen*size, colour);
                inrun = 0;
                runlen = 0;
                endrun = 0;
            }
        }
    }
}

void draw_string(char *string, uint16_t x, uint16_t y, uint16_t size, uint16_t colour)
{
    while(*string){
        draw_char(*string, x, y, size, colour);
        *string++;
        x += FONT_SPACE*size;
        if(x >= lcd_config.screen_width-1){
            y += FONT_Y*size;
            x = 0;
        }
    }
}

void lcd_update_bank_number(uint8_t bank_number) {
    ssd1331_clear_screen();
    char bank_string[10];
    sprintf(bank_string, "BANK: %d", bank_number);
    draw_string(bank_string, 5, 25, 2, COLOUR_PURPLE);
}