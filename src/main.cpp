#include "display.h"
#include "config.h"
#include <SD.h>
#include <SPI.h>
#include <TJpg_Decoder.h>
#include <vector>
#include <algorithm>

// ==================== Global Variables ====================
SPIClass sdSPI = SPIClass(HSPI);
std::vector<String> imageFiles;
int currentImageIndex = 0;
unsigned long lastImageChange = 0;
const unsigned long SLIDESHOW_INTERVAL = 10000;
bool fatalError = false; // Флаг фатальной ошибки

// ==================== Filter System Files ====================
bool isSystemFile(const String& filename) {
    if (filename.startsWith("._")) return true;
    if (filename.equalsIgnoreCase(".DS_Store")) return true;
    if (filename.equalsIgnoreCase("Thumbs.db")) return true;
    if (filename.equalsIgnoreCase("desktop.ini")) return true;
    return false;
}

// ==================== Display Error Message (FIXED) ====================
void displayErrorScreen(const String& title, const String& message) {
    // Полная очистка экрана черным цветом
    gfx.fillScreen(BLACK);
    
    // Отображаем заголовок
    gfx.setCursor(140, 350);
    gfx.setTextSize(2);
    gfx.setTextColor(RED);
    gfx.print(title);
    
    // Отображаем сообщение
    gfx.setCursor(100, 400);
    gfx.setTextSize(1);
    gfx.setTextColor(WHITE);
    gfx.print(message);
    
    // Также отображаем инструкцию
    gfx.setCursor(100, 450);
    gfx.setTextSize(1);
    gfx.print("Restart device to retry");
}

// ==================== SD Card Initialization ====================
bool initSDCard() {
    Serial.println("Initializing SD card...");
    
    sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    delay(100);
    
    if (!SD.begin(SD_CS, sdSPI, 4000000)) {
        Serial.println("SD card initialization failed!");
        return false;
    }
    
    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        Serial.println("No SD card attached");
        return false;
    }
    
    Serial.print("SD Card Type: ");
    switch(cardType) {
        case CARD_MMC:  Serial.println("MMC"); break;
        case CARD_SD:   Serial.println("SDSC"); break;
        case CARD_SDHC: Serial.println("SDHC"); break;
        default:        Serial.println("Unknown");
    }
    
    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("SD Card Size: %lluMB\n", cardSize);
    
    return true;
}

// ==================== Find All Image Files ====================
void findImageFiles() {
    Serial.println("Scanning for images...");
    
    File root = SD.open("/");
    if (!root) {
        Serial.println("Cannot open root directory");
        return;
    }
    
    while (true) {
        File entry = root.openNextFile();
        if (!entry) break;
        
        String filename = entry.name();
        
        if (entry.isDirectory()) {
            entry.close();
            continue;
        }
        
        if (isSystemFile(filename)) {
            entry.close();
            continue;
        }
        
        String ext = filename.substring(filename.lastIndexOf('.'));
        ext.toLowerCase();
        
        if (ext == ".jpg" || ext == ".jpeg") {
            String path = "/" + String(entry.name());
            imageFiles.push_back(path);
            Serial.printf("Found image: %s (%d bytes)\n", path.c_str(), entry.size());
        }
        
        entry.close();
    }
    
    root.close();
    
    std::sort(imageFiles.begin(), imageFiles.end(), [](const String& a, const String& b) {
        return a < b;
    });
    
    Serial.printf("Found %d images\n", imageFiles.size());
}

// ==================== Display Image ====================
void displayImage(int index) {
    if (imageFiles.empty()) {
        return;
    }
    
    if (index < 0) index = 0;
    if (index >= imageFiles.size()) index = imageFiles.size() - 1;
    
    currentImageIndex = index;
    String path = imageFiles[currentImageIndex];
    
    Serial.printf("\n=== Displaying %d/%d: %s ===\n", 
                  currentImageIndex + 1, imageFiles.size(), path.c_str());
    
    if (path.endsWith(".jpg") || path.endsWith(".jpeg")) {
        TJpgDec.setJpgScale(1);
        TJpgDec.setCallback(tft_output);
        
        uint16_t imgWidth, imgHeight;
        JRESULT res = TJpgDec.getSdJpgSize(&imgWidth, &imgHeight, path.c_str());
        
        if (res == JDR_OK) {
            int offsetX = (480 - imgWidth) / 2;
            int offsetY = (800 - imgHeight) / 2;
            TJpgDec.drawSdJpg(offsetX, offsetY, path.c_str());
        } else {
            TJpgDec.drawSdJpg(240, 400, path.c_str());
        }
    }
    
    lastImageChange = millis();
}

// ==================== Setup ====================
void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n" + String(60, '='));
    Serial.println("ESP32 Photo Frame");
    Serial.println(String(60, '='));
    
    // Инициализация дисплея
    setup_display();
    
    // Устанавливаем портретный режим
    gfx.setRotation(1);
    ts.setRotation(1);
    
    // Очищаем экран черным цветом
    gfx.fillScreen(BLACK);
    
    // Инициализация SD карты
    if (initSDCard()) {
        // Поиск изображений
        findImageFiles();
        
        if (!imageFiles.empty()) {
            // Показываем первое изображение
            displayImage(0);
            Serial.println("\nSlideshow started!");
            Serial.printf("Interval: %d seconds\n", SLIDESHOW_INTERVAL / 1000);
        } else {
            // Нет изображений
            fatalError = true;
            displayErrorScreen("NO IMAGES", "Add JPG files to SD card");
            Serial.println("\nERROR: No JPEG images found on SD card!");
            
            // Бесконечный цикл при ошибке
            while (fatalError) {
                delay(100);
            }
        }
    } else {
        // Ошибка SD карты
        fatalError = true;
        displayErrorScreen("SD CARD ERROR", "Insert card with images");
        Serial.println("\nERROR: SD card initialization failed!");
        
        // Бесконечный цикл при ошибке
        while (fatalError) {
            delay(100);
        }
    }
}

// ==================== Loop ====================
void loop() {
    // Если фатальная ошибка - не выполняем loop_display
    if (fatalError) {
        delay(100);
        return;
    }
    
    // Обработка LVGL
    //loop_display();
    
    // Автоматическое слайд-шоу
    if (!imageFiles.empty()) {
        if (millis() - lastImageChange >= SLIDESHOW_INTERVAL) {
            currentImageIndex = (currentImageIndex + 1) % imageFiles.size();
            displayImage(currentImageIndex);
        }
    }
    
    delay(10);
}