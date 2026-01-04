#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino_GFX_Library.h>
#include <SD.h>
#include <SPI.h>

// ==================== Display & Touch Configuration ====================
#define TFT_BL 2

extern Arduino_RGB_Display gfx;

// ==================== Display Functions ====================
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap);
void setup_display();
void set_brightness(uint8_t level);

#endif // DISPLAY_H