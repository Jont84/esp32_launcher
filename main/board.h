/*
 * board.h - pins and panel settings for the LCDwiki ES3C28P, as verified
 * during the Keen port (keen_board.h, 2026-11-06). Don't change these.
 */
#pragma once

/* ---- Display (ILI9341V, SPI) ---- */
#define BD_LCD_HOST        SPI2_HOST
#define BD_PIN_LCD_SCLK    12
#define BD_PIN_LCD_MOSI    11
#define BD_PIN_LCD_CS      10
#define BD_PIN_LCD_DC      46
#define BD_PIN_LCD_BL      45
#define BD_LCD_CLOCK_HZ    (40 * 1000 * 1000)   /* 80 MHz glitched every few seconds on Keen */
#define BD_LCD_INVERT      true                 /* required on this IPS panel */
#define BD_LCD_RGB_ORDER   LCD_RGB_ELEMENT_ORDER_BGR
#define BD_LCD_SWAP_XY     true                 /* landscape, I2C jack at the bottom */
#define BD_LCD_MIRROR_X    true
#define BD_LCD_MIRROR_Y    true
#define BD_LCD_W           320
#define BD_LCD_H           240

/* ---- I2C bus (controller, touch, audio codec) ---- */
#define BD_PIN_I2C_SDA     16
#define BD_PIN_I2C_SCL     15

/* ---- Joystick controller (Nulllab/Emakefun mini joystick) ---- */
#define BD_CTRL_ADDR       0x5A
#define BD_CTRL_REG_X      0x10
#define BD_CTRL_REG_Y      0x11
#define BD_CTRL_REG_OK     0x20
#define BD_CTRL_REG_C      0x21
#define BD_CTRL_REG_A      0x22
#define BD_CTRL_REG_B      0x23
#define BD_CTRL_REG_D      0x24
#define BD_CTRL_DEADZONE   40       /* stick counts either side of 128 */
#define BD_CTRL_INVERT_X   false
#define BD_CTRL_INVERT_Y   true     /* controller mounted rotated 180 degrees */
