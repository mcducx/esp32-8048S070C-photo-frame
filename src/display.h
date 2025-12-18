#ifndef DISPLAY_H
#define DISPLAY_H

#include <lvgl.h>
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

// ==================== LVGL Callbacks ====================
uint32_t millis_cb(void);
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);
void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data);
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap);  // Объявление здесь!

// ==================== Display Setup ====================
void setup_display();
void loop_display();

#endif // DISPLAY_H