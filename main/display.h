#pragma once
#include <stdint.h>

/* Start the LCD. The backlight comes on with the first frame. */
void display_init(void);

/* Show a 320x240 frame, one byte per pixel, through lut (byte-swapped RGB565).
 * Sends only the bands that changed; returns how many (0-12). Blocks until done. */
int display_show(const uint8_t *fb, const uint16_t lut[256]);

/* 8-bit RGB to byte-swapped RGB565, as the panel expects (from keen_port.h) */
static inline uint16_t display_rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    uint16_t v = (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
    return (uint16_t)((v >> 8) | (v << 8));
}
