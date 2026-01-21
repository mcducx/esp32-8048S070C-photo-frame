#include "display.h"
#include "config.h"
#include <Arduino.h>
#include <TJpg_Decoder.h>

// ==================== Global Display Objects ====================
Arduino_ESP32RGBPanel rgbpanel(
    41 /* DE */, 40 /* VSYNC */, 39 /* HSYNC */, 42 /* PCLK */,
    14 /* R0 */, 21 /* R1 */, 47 /* R2 */, 48 /* R3 */, 45 /* R4 */,
    9 /* G0 */, 46 /* G1 */, 3 /* G2 */, 8 /* G3 */, 16 /* G4 */, 1 /* G5 */,
    15 /* B0 */, 7 /* B1 */, 6 /* B2 */, 5 /* B3 */, 4 /* B4 */,
    0 /* hsync_polarity */, 20 /* hsync_front_porch */, 30 /* hsync_pulse_width */, 16 /* hsync_back_porch */,
    0 /* vsync_polarity */, 22 /* vsync_front_porch */, 13 /* vsync_pulse_width */, 10 /* vsync_back_porch */,
    true /* pclk_active_neg */, 16000000 /* prefer_speed */, false /* useBigEndian */);
    
Arduino_RGB_Display gfx(800, 480, &rgbpanel, 0, true);

// ==================== TJpg_Decoder Output ====================
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
    // Stop decoding if out of screen bounds
    if (y >= (int16_t)gfx.height()) return 0;
    
    // Adjust width if image exceeds screen
    if ((x + w) > (int16_t)gfx.width()) {
        w = (uint16_t)((int16_t)gfx.width() - x);
    }
    
    // Validate parameter correctness
    if (x < 0 || y < 0 || w == 0 || h == 0) {
        return 0;
    }
    
    gfx.draw16bitRGBBitmap(x, y, bitmap, w, h);
    return 1;
}

// ==================== Display Setup ====================
void setup_display() {
    // Check if Serial is initialized
    if (!Serial) {
        Serial.begin(115200);
        delay(100);
    }
    
    Serial.println("Initializing display...");
    
    // Initialize display
    gfx.begin();
    // Set default portrait mode
    gfx.setRotation(1);  // 90 degrees for portrait mode (480x800)
    
    // Initialize backlight PWM
    ledcSetup(0, 5000, 8);  // 5kHz PWM, 8-bit resolution
    ledcAttachPin(TFT_BL, 0);
    ledcWrite(0, BRIGHTNESS_DEFAULT);  // Default brightness
    
    Serial.println("Display setup complete.");
    Serial.printf("Display: %dx%d\n", gfx.width(), gfx.height());
}

// ==================== Set Brightness ====================
void set_brightness(uint8_t level) {
    if (level < MIN_BRIGHTNESS) level = MIN_BRIGHTNESS;
    if (level > MAX_BRIGHTNESS) level = MAX_BRIGHTNESS;
    ledcWrite(0, level);
    Serial.printf("Brightness set to: %d\n", level);
}