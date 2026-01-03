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

// ==================== PWM for Backlight ====================
const int pwmChannel = 0;
const int pwmFrequency = 5000;
const int pwmResolution = 8;  // 8-bit = 0-255
uint8_t currentBrightness = BRIGHTNESS_DEFAULT;

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

// ==================== Brightness Functions ====================
void setBrightness(uint8_t brightness) {
    if (brightness > 255) brightness = 255;
    currentBrightness = brightness;
    ledcWrite(pwmChannel, brightness);
    Serial.printf("Brightness set to: %d\n", brightness);
}

uint8_t getBrightness() {
    return currentBrightness;
}

void saveBrightnessToSD() {
    if (!SD.exists("/")) {
        Serial.println("SD card not available for saving brightness");
        return;
    }
    
    File brightnessFile = SD.open(BRIGHTNESS_FILENAME, FILE_WRITE);
    if (brightnessFile) {
        brightnessFile.print(currentBrightness);
        brightnessFile.close();
        Serial.printf("Brightness saved to SD: %d\n", currentBrightness);
    } else {
        Serial.println("Failed to save brightness to SD card!");
    }
}

void loadBrightnessFromSD() {
    if (!SD.exists("/")) {
        Serial.println("SD card not available for loading brightness");
        currentBrightness = BRIGHTNESS_DEFAULT;
        return;
    }
    
    if (SD.exists(BRIGHTNESS_FILENAME)) {
        File brightnessFile = SD.open(BRIGHTNESS_FILENAME, FILE_READ);
        if (brightnessFile) {
            String brightnessStr = brightnessFile.readString();
            brightnessFile.close();
            
            int savedBrightness = brightnessStr.toInt();
            
            if (savedBrightness >= 0 && savedBrightness <= 255) {
                currentBrightness = savedBrightness;
                Serial.printf("Brightness loaded from SD: %d\n", currentBrightness);
            } else {
                currentBrightness = BRIGHTNESS_DEFAULT;
                Serial.printf("Invalid brightness data, using default: %d\n", currentBrightness);
                saveBrightnessToSD();
            }
        } else {
            currentBrightness = BRIGHTNESS_DEFAULT;
            Serial.printf("Failed to read brightness file, using default: %d\n", currentBrightness);
            saveBrightnessToSD();
        }
    } else {
        currentBrightness = BRIGHTNESS_DEFAULT;
        Serial.printf("Brightness file not found, using default: %d\n", currentBrightness);
        saveBrightnessToSD();
    }
    
    setBrightness(currentBrightness);
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
    
    // Backlight PWM setup
    pinMode(TFT_BL, OUTPUT);
    ledcSetup(pwmChannel, pwmFrequency, pwmResolution);
    ledcAttachPin(TFT_BL, pwmChannel);
    
    // Set default brightness
    setBrightness(BRIGHTNESS_DEFAULT);
    
    Serial.println("Display setup complete.");
    Serial.printf("Display: %dx%d\n", gfx.width(), gfx.height());
    Serial.printf("Initial brightness: %d\n", currentBrightness);
}