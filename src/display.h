#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino_GFX_Library.h>
#include "TAMC_GT911.h"
#include <SD.h>
#include <SPI.h>

// ==================== SD Card Configuration ====================
#define SD_CS    10
#define SD_SCK   12 
#define SD_MISO  13
#define SD_MOSI  11

// ==================== Display & Touch Configuration ====================
#define TFT_BL 2
#define TOUCH_GT911_SCL 20
#define TOUCH_GT911_SDA 19
#define TOUCH_GT911_INT -1
#define TOUCH_GT911_RST 38

extern TAMC_GT911 ts;
extern Arduino_RGB_Display gfx;

// ==================== Display Functions ====================
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap);
void setup_display();
bool check_touch(uint16_t *x, uint16_t *y);

#endif // DISPLAY_H