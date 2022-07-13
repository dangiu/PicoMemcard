/*
* SSD1331.cpp
* A library for SSD1331 OLED modules for use with the RP2040
*
* Copyright (c) 2022 Darren Anderson
* Copyright (c) 2014 seeed technology inc.
* Copyright (c) 2012, Adafruit Industries.
*
* This library is based on the RGB_OLED_SSD1331 driver by Seeed Technologies inc.
*
* Software License Agreement (BSD License)
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
* 1. Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
* 2. Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution.
* 3. Neither the name of the copyright holders nor the
* names of its contributors may be used to endorse or promote products
* derived from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
*/

#include "ssd1331.h"
#include "pico/time.h"

ssd1331_config_t *ssd1331_config;

void ssd1331_command(uint8_t command, uint8_t value) {
    ssd1331_config->spi_write(command);
    ssd1331_config->spi_write(value);
}

void ssd1331_init(ssd1331_config_t* config) {
    ssd1331_config = config;

    ssd1331_config->enable_reset();
    ssd1331_config->disable_dc();
    sleep_ms(10);
    ssd1331_config->disable_reset();
    sleep_ms(1);

    ssd1331_config->spi_write(CMD_DISPLAY_OFF);
    ssd1331_command(CMD_SET_CONTRAST_A, 0x91);
    ssd1331_command(CMD_SET_CONTRAST_B, 0x50);
    ssd1331_command(CMD_SET_CONTRAST_C, 0x7D);
    ssd1331_command(CMD_MASTER_CURRENT_CONTROL, 0x06);
    ssd1331_command(CMD_SET_PRECHARGE_SPEED_A, 0x64);
    ssd1331_command(CMD_SET_PRECHARGE_SPEED_B, 0x78);
    ssd1331_command(CMD_SET_PRECHARGE_SPEED_C, 0x64);
    ssd1331_command(CMD_SET_REMAP, 0x72);
    ssd1331_command(CMD_SET_DISPLAY_START_LINE, 0x0);
    ssd1331_command(CMD_SET_DISPLAY_OFFSET, 0x0);
    ssd1331_config->spi_write(CMD_NORMAL_DISPLAY);
    ssd1331_command(CMD_SET_MULTIPLEX_RATIO, 0x3F);
    ssd1331_command(CMD_SET_MASTER_CONFIGURE, 0x8E);
    ssd1331_command(CMD_POWER_SAVE_MODE, 0x0B);
    ssd1331_command(CMD_PHASE_PERIOD_ADJUSTMENT, 0x31);
    ssd1331_command(CMD_DISPLAY_CLOCK_DIV, 0xF0);
    ssd1331_command(CMD_SET_PRECHARGE_VOLTAGE, 0x3A);
    ssd1331_command(CMD_SET_V_VOLTAGE, 0x3E);
    ssd1331_config->spi_write(CMD_DEACTIVE_SCROLLING);
    ssd1331_config->spi_write(CMD_NORMAL_BRIGHTNESS_DISPLAY_ON);
}

void ssd1331_draw_pixel(uint8_t x, uint8_t y, uint16_t colour) {
    if ((x < 0) || (x >= ssd1331_config->screen_width) || (y < 0) || (y >= ssd1331_config->screen_height))
        return;

    ssd1331_config->spi_write(CMD_SET_COLUMN_ADDRESS);
    ssd1331_config->spi_write(x); // column start address
    ssd1331_config->spi_write(ssd1331_config->screen_width-1); // column end address

    ssd1331_config->spi_write(CMD_SET_ROW_ADDRESS);
    ssd1331_config->spi_write(y); // row start address
    ssd1331_config->spi_write(ssd1331_config->screen_height-1); // row end address

    ssd1331_config->enable_dc();
    ssd1331_config->spi_write(colour >> 8);
    ssd1331_config->spi_write(colour);
    ssd1331_config->disable_dc();
}

void ssd1331_clear_screen() {
    ssd1331_config->spi_write(CMD_CLEAR_WINDOW);
    ssd1331_config->spi_write(0);
    ssd1331_config->spi_write(0);
    ssd1331_config->spi_write(ssd1331_config->screen_width-1);
    ssd1331_config->spi_write(ssd1331_config->screen_height-1);
}