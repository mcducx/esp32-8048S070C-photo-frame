#include "display.h"
#include "config.h"
#include <Arduino.h>
#include <TJpg_Decoder.h>

// ==================== Global Display Objects ====================
TAMC_GT911 ts(TOUCH_GT911_SDA, TOUCH_GT911_SCL, TOUCH_GT911_INT, TOUCH_GT911_RST, 800, 480);

Arduino_ESP32RGBPanel rgbpanel(
    41 /* DE */, 40 /* VSYNC */, 39 /* HSYNC */, 42 /* PCLK */,
    14 /* R0 */, 21 /* R1 */, 47 /* R2 */, 48 /* R3 */, 45 /* R4 */,
    9 /* G0 */, 46 /* G1 */, 3 /* G2 */, 8 /* G3 */, 16 /* G4 */, 1 /* G5 */,
    15 /* B0 */, 7 /* B1 */, 6 /* B2 */, 5 /* B3 */, 4 /* B4 */,
    0 /* hsync_polarity */, 20 /* hsync_front_porch */, 30 /* hsync_pulse_width */, 16 /* hsync_back_porch */,
    0 /* vsync_polarity */, 22 /* vsync_front_porch */, 13 /* vsync_pulse_width */, 10 /* vsync_back_porch */,
    true /* pclk_active_neg */, 16000000 /* prefer_speed */, false /* useBigEndian */);
    
Arduino_RGB_Display gfx(800, 480, &rgbpanel, 0, true);

static uint32_t screenWidth;
static uint32_t screenHeight;
static uint32_t bufSize;
static lv_display_t *disp;
static lv_color_t *disp_draw_buf;

// ==================== TJpg_Decoder Output ====================
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
    // Stop decoding if out of screen bounds
    if (y >= (int16_t)gfx.height()) return 0;
    
    // Adjust width if image exceeds screen
    if ((x + w) > (int16_t)gfx.width()) {
        w = (uint16_t)((int16_t)gfx.width() - x);
    }
    
    // Проверка на корректность параметров
    if (x < 0 || y < 0 || w == 0 || h == 0) {
        return 0;
    }
    
    gfx.draw16bitRGBBitmap(x, y, bitmap, w, h);
    return 1;
}

// ==================== Timer Callback ====================
uint32_t millis_cb(void) {
    return millis();
}

// ==================== Display Flush Function ====================
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    uint32_t w = lv_area_get_width(area);
    uint32_t h = lv_area_get_height(area);
    
    // Convert LVGL colors to 16-bit RGB565
    gfx.draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);
    
    lv_disp_flush_ready(disp);
}

// ==================== Touchpad Read ====================
void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data) {
    ts.read();
    
    if (ts.isTouched && ts.touches > 0) {
        data->state = LV_INDEV_STATE_PRESSED;
        
        // Map touch coordinates to display coordinates
        int16_t x = map(ts.points[0].x, 0, 800, 0, screenWidth);
        int16_t y = map(ts.points[0].y, 0, 480, 0, screenHeight);
        
        // Проверка границ - простой и надежный способ
        if (x < 0) x = 0;
        if (y < 0) y = 0;
        if (x >= (int16_t)screenWidth) x = (int16_t)screenWidth - 1;
        if (y >= (int16_t)screenHeight) y = (int16_t)screenHeight - 1;
        
        data->point.x = x;
        data->point.y = y;
        
        Serial.printf("Touch: %d, %d\n", x, y);
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

// ==================== Display Setup ====================
void setup_display() {
    // Проверяем, инициализирован ли Serial
    if (!Serial) {
        Serial.begin(115200);
        delay(100);
    }
    
    Serial.println("Initializing display...");
    
    // Initialize display
    gfx.begin();
    // Устанавливаем портретный режим по умолчанию
    gfx.setRotation(1);  // 90 градусов для портретного режима (480x800)
    
    // Backlight
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
    
    // Initialize touch
    ts.begin();
    delay(100);
    ts.setRotation(1);
    Serial.println("Touch controller initialized");
    
    // Initialize LVGL
    lv_init();
    lv_tick_set_cb(millis_cb);
    
    screenWidth = gfx.width();
    screenHeight = gfx.height();
    
    Serial.printf("Display: %dx%d\n", screenWidth, screenHeight);
    
    // Allocate buffer for LVGL
    bufSize = screenWidth * 40;  // 40 lines buffer
    
#ifdef BOARD_HAS_PSRAM
    disp_draw_buf = (lv_color_t *)ps_malloc(bufSize * sizeof(lv_color_t));
    Serial.println("Using PSRAM for display buffer");
#else
    disp_draw_buf = (lv_color_t *)malloc(bufSize * sizeof(lv_color_t));
#endif
    
    if (!disp_draw_buf) {
        Serial.println("Failed to allocate display buffer!");
        while(1);
    }
    
    // Create display
    disp = lv_display_create(screenWidth, screenHeight);
    lv_display_set_flush_cb(disp, my_disp_flush);
    lv_display_set_buffers(disp, disp_draw_buf, NULL, bufSize * 2, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    
    // Create input device (touch)
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touchpad_read);
    
    Serial.println("Display setup complete.");
}

// ==================== Main Loop ====================
void loop_display() {
    lv_timer_handler();
    delay(5);
}