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
const unsigned long SLIDESHOW_INTERVAL = 10000; // 10 секунд между изображениями

// ==================== Filter System Files ====================
bool isSystemFile(const String& filename) {
    if (filename.startsWith("._")) return true;
    if (filename.equalsIgnoreCase(".DS_Store")) return true;
    if (filename.equalsIgnoreCase("Thumbs.db")) return true;
    if (filename.equalsIgnoreCase("desktop.ini")) return true;
    return false;
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
    
    int totalFiles = 0;
    int skippedFiles = 0;
    
    while (true) {
        File entry = root.openNextFile();
        if (!entry) break;
        
        totalFiles++;
        String filename = entry.name();
        
        if (entry.isDirectory()) {
            entry.close();
            continue;
        }
        
        if (isSystemFile(filename)) {
            skippedFiles++;
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
    
    // Очищаем экран
    gfx.fillScreen(BLACK);
    
    // Отображаем изображение
    if (path.endsWith(".jpg") || path.endsWith(".jpeg")) {
        // Отключаем масштабирование - используем коэффициент 1 (оригинальный размер)
        TJpgDec.setJpgScale(1);
        TJpgDec.setCallback(tft_output);
        
        // Получаем размеры изображения для центрирования
        uint16_t imgWidth, imgHeight;
        JRESULT res = TJpgDec.getSdJpgSize(&imgWidth, &imgHeight, path.c_str());
        
        if (res == JDR_OK) {
            Serial.printf("Image size: %dx%d pixels\n", imgWidth, imgHeight);
            
            // ВСЕГДА вычисляем смещение для центрирования
            // Если изображение меньше экрана - будут черные поля
            // Если изображение больше экрана - будет показана центральная часть
            int offsetX = (480 - imgWidth) / 2;
            int offsetY = (800 - imgHeight) / 2;
            
            Serial.printf("Centering offset: X=%d, Y=%d\n", offsetX, offsetY);
            
            // Отображаем изображение с вычисленным смещением
            TJpgDec.drawSdJpg(offsetX, offsetY, path.c_str());
            
            // Для отладки: выводим информацию о том, что происходит
            if (offsetX > 0 && offsetY > 0) {
                Serial.println("Image smaller than screen: showing with black borders");
            } else if (offsetX < 0 || offsetY < 0) {
                Serial.println("Image larger than screen: showing center portion");
            } else {
                Serial.println("Image perfectly fits the screen");
            }
        } else {
            // Если не удалось получить размеры, отображаем в центре экрана
            Serial.println("Cannot get image size, displaying at center");
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
    Serial.println("ESP32 Photo Frame - Always Centered");
    Serial.println("Images are ALWAYS centered, no scaling");
    Serial.println(String(60, '='));
    
    // Инициализация дисплея
    setup_display();
    
    // Устанавливаем портретный режим (поворот на 90 градусов)
    gfx.setRotation(1);  // 90 градусов для портретного режима
    ts.setRotation(1);   // Также поворачиваем тачскрин
    
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
            gfx.fillScreen(BLACK);
            gfx.setCursor(140, 350);
            gfx.setTextSize(2);
            gfx.setTextColor(WHITE);
            gfx.print("NO IMAGES");
            
            gfx.setCursor(100, 400);
            gfx.setTextSize(1);
            gfx.print("Add JPG files to SD card");
            
            Serial.println("\nERROR: No JPEG images found on SD card!");
        }
    } else {
        // Ошибка SD карты
        gfx.fillScreen(BLACK);
        gfx.setCursor(140, 350);
        gfx.setTextSize(2);
        gfx.setTextColor(RED);
        gfx.print("SD CARD ERROR");
        
        gfx.setCursor(120, 400);
        gfx.setTextSize(1);
        gfx.setTextColor(WHITE);
        gfx.print("Insert card with images");
        
        Serial.println("\nERROR: SD card initialization failed!");
    }
}

// ==================== Loop ====================
void loop() {
    // Обработка LVGL
    loop_display();
    
    // Автоматическое слайд-шоу
    if (!imageFiles.empty()) {
        if (millis() - lastImageChange >= SLIDESHOW_INTERVAL) {
            currentImageIndex = (currentImageIndex + 1) % imageFiles.size();
            displayImage(currentImageIndex);
        }
    }
    
    delay(10);
}