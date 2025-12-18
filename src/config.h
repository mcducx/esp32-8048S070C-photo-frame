#ifndef CONFIG_H
#define CONFIG_H

// ==================== SD Card SPI Configuration ====================
#define SD_SCK   12
#define SD_MISO  13
#define SD_MOSI  11
#define SD_CS    10

// ==================== Touch Configuration ====================
#define TOUCH_GT911_SDA 19
#define TOUCH_GT911_SCL 20
#define TOUCH_GT911_INT -1
#define TOUCH_GT911_RST 38

// ==================== Display Configuration ====================
#define TFT_BL   2
#define DISP_WIDTH  800
#define DISP_HEIGHT 480

#endif // CONFIG_H