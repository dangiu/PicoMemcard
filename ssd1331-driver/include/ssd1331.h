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

#ifndef SSD1331_H
#define SSD1331_H

#include <stdint.h>

#define CMD_CLEAR_WINDOW                    0x25
#define CMD_DEACTIVE_SCROLLING              0x2E

#define CMD_SET_COLUMN_ADDRESS              0x15
#define CMD_SET_ROW_ADDRESS                 0x75
#define CMD_SET_CONTRAST_A                  0x81
#define CMD_SET_CONTRAST_B                  0x82
#define CMD_SET_CONTRAST_C                  0x83
#define CMD_MASTER_CURRENT_CONTROL          0x87
#define CMD_SET_PRECHARGE_SPEED_A           0x8A
#define CMD_SET_PRECHARGE_SPEED_B           0x8B
#define CMD_SET_PRECHARGE_SPEED_C           0x8C
#define CMD_SET_REMAP                       0xA0
#define CMD_SET_DISPLAY_START_LINE          0xA1
#define CMD_SET_DISPLAY_OFFSET              0xA2
#define CMD_NORMAL_DISPLAY                  0xA4
#define CMD_SET_MULTIPLEX_RATIO             0xA8
#define CMD_SET_MASTER_CONFIGURE            0xAD
#define CMD_DISPLAY_OFF                     0xAE
#define CMD_NORMAL_BRIGHTNESS_DISPLAY_ON    0xAF
#define CMD_POWER_SAVE_MODE                 0xB0
#define CMD_PHASE_PERIOD_ADJUSTMENT         0xB1
#define CMD_DISPLAY_CLOCK_DIV               0xB3
#define CMD_SET_PRECHARGE_VOLTAGE           0xBB
#define CMD_SET_V_VOLTAGE                   0xBE

typedef struct {
    void (*spi_write)(uint8_t data);
    void (*spi_write16)(uint16_t data);
    void (*enable_dc)();
    void (*disable_dc)();
    void (*enable_reset)();
    void (*disable_reset)();
    void (*enable_cs)();
    void (*disable_cs)();
    uint8_t screen_width;
    uint8_t screen_height;
} ssd1331_config_t;

#define RGB(R,G,B)  ((((R)>>3)<<11) | (((G)>>2)<<5) | ((B)>>3))
#define RGB332_TO_RGB565(x) ((((x) & 0b11100000) << 8) | (((x) & 0b00011100) << 6) | (((x) & 0b00000011) << 3))

enum Colour {
    COLOUR_BLACK     = RGB(  0,  0,  0), // black
    COLOUR_GREY      = RGB(192,192,192), // grey
    COLOUR_WHITE     = RGB(255,255,255), // white
    COLOUR_RED       = RGB(255,  0,  0), // red
    COLOUR_PINK      = RGB(255,192,203), // pink
    COLOUR_YELLOW    = RGB(255,255,  0), // yellow
    COLOUR_GOLDEN    = RGB(255,215,  0), // golden
    COLOUR_BROWN     = RGB(128, 42, 42), // brown
    COLOUR_BLUE      = RGB(  0,  0,255), // blue
    COLOUR_CYAN      = RGB(  0,255,255), // cyan
    COLOUR_GREEN     = RGB(  0,255,  0), // green
    COLOUR_PURPLE    = RGB(160, 32,240), // purple
};

enum DisplayMode{
    //reset the above effect and turn the data to ON at the corresponding gray level.
    NormalDisplay   = 0xA4,
    //forces the entire display to be at "GS63"
    DisplayOn       = 0xA5,
    //forces the entire display to be at gray level "GS0"
    DisplayOff      = 0xA6,
    //swap the gray level of display data
    InverseDisplay  = 0xA7
};

enum DisplayPower{
    DimMode         = 0xAC,
    SleepMode       = 0xAE,
    NormalMode      = 0xAF
};

enum ScollingDirection{
    Horizontal      = 0x00,
    Vertical        = 0x01,
    Diagonal        = 0x02
};

void ssd1331_init(ssd1331_config_t* config);
void ssd1331_draw_pixel(uint8_t x, uint8_t y, uint16_t colour);
void ssd1331_clear_screen();

#endif
