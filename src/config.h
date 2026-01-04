#ifndef CONFIG_H
#define CONFIG_H

// ==================== Slideshow Configuration ====================
#define INTERVAL_DEFAULT_INDEX 2  // Default to 1 minute (60000 ms)
#define INTERVAL_FILENAME "/interval.txt"
#define BRIGHTNESS_FILENAME "/brightness.txt"

// ==================== Button Configuration ====================
#define BOOT_BUTTON_PIN 0  // GPIO0 - кнопка BOOT на ESP32

// Button timing (milliseconds)
#define SHORT_PRESS_TIME 50
#define LONG_PRESS_TIME 500    // 500ms для длинного нажатия
#define MENU_TIMEOUT 10000     // 10 секунд бездействия в главном меню
#define SETTING_TIMEOUT 5000   // 5 секунд для выхода из настроек

// ==================== SD Card SPI Configuration ====================
#define SD_SCK   12
#define SD_MISO  13
#define SD_MOSI  11
#define SD_CS    10

// ==================== Wi-Fi Settings ====================
#define WIFI_SSID "Sinelogic_Net"
#define WIFI_PASSWORD "24111981"

// ==================== OTA Settings ====================
#define OTA_HOSTNAME "ESP32-Photo-Frame"
#define OTA_PASSWORD "ota_password"  // Измените на свой пароль

// ==================== Web Server Settings ====================
#define WEB_PORT 80
#define MAX_UPLOAD_SIZE 10485760  // 10MB max file size

// ==================== Display Settings ====================
#define MAX_BRIGHTNESS 255
#define MIN_BRIGHTNESS 20
#define BRIGHTNESS_STEP 10
#define BRIGHTNESS_DEFAULT 128

// ==================== Debug Settings ====================
#define DEBUG_SERIAL true

#endif // CONFIG_H