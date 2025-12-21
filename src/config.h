#ifndef CONFIG_H
#define CONFIG_H

// ==================== Slideshow Configuration ====================
#define INTERVAL_DEFAULT_INDEX 2
#define INTERVAL_FILENAME "/interval.txt"

// ==================== Button Configuration ====================
#define BOOT_BUTTON_PIN 0  // GPIO0 - кнопка BOOT на ESP32

// ==================== SD Card SPI Configuration ====================
#define SD_SCK   12
#define SD_MISO  13
#define SD_MOSI  11
#define SD_CS    10

#endif // CONFIG_H