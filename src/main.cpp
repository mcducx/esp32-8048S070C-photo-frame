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
std::vector<int> shuffledIndices;  // Для случайного порядка
int currentImageIndex = 0;
int currentShuffleIndex = 0;  // Теперь это глобальная переменная
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
    
    Serial.printf("Found %d images\n", imageFiles.size());
}

// ==================== Initialize Random Slideshow Order ====================
void initRandomSlideshow() {
    if (imageFiles.empty()) return;
    
    // Заполняем shuffledIndices числами от 0 до imageFiles.size()-1
    shuffledIndices.clear();
    for (int i = 0; i < imageFiles.size(); i++) {
        shuffledIndices.push_back(i);
    }
    
    // Перемешиваем индексы для случайного порядка
    for (int i = shuffledIndices.size() - 1; i > 0; i--) {
        int j = random(0, i + 1);  // случайное число от 0 до i включительно
        std::swap(shuffledIndices[i], shuffledIndices[j]);
    }
    
    // Сбрасываем индекс для перемешанного списка
    currentShuffleIndex = 0;
    
    Serial.println("Random slideshow order initialized");
}

// ==================== Get Next Random Image Index ====================
int getNextRandomImage() {
    if (imageFiles.empty()) return 0;
    
    // Получаем текущий индекс из перемешанного списка
    int imageIndex = shuffledIndices[currentShuffleIndex];
    
    // Увеличиваем индекс для следующего вызова
    currentShuffleIndex++;
    
    // Если дошли до конца списка, перемешиваем заново
    if (currentShuffleIndex >= shuffledIndices.size()) {
        // Перемешиваем список заново для нового цикла
        for (int i = shuffledIndices.size() - 1; i > 0; i--) {
            int j = random(0, i + 1);
            std::swap(shuffledIndices[i], shuffledIndices[j]);
        }
        
        // Сбрасываем индекс и начинаем заново
        currentShuffleIndex = 0;
        
        Serial.println("Reshuffled image order for new cycle");
    }
    
    return imageIndex;
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
    Serial.println("ESP32 Photo Frame - RANDOM SLIDESHOW");
    Serial.println(String(60, '='));
    
    // Инициализация генератора случайных чисел
    // Используем аналоговый шум с неподключенного пина для лучшей случайности
    randomSeed(analogRead(0));
    
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
            // Инициализация случайного порядка слайдшоу
            initRandomSlideshow();
            
            // Показываем первое случайное изображение и УВЕЛИЧИВАЕМ индекс
            int firstImageIndex = shuffledIndices[currentShuffleIndex];
            displayImage(firstImageIndex);
            currentShuffleIndex++;  // Увеличиваем индекс, чтобы следующее изображение было другим
            
            Serial.println("\nRandom slideshow started!");
            Serial.printf("Interval: %d seconds\n", SLIDESHOW_INTERVAL / 1000);
            Serial.printf("Total images: %d\n", imageFiles.size());
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
    // loop_display();
    
    // Автоматическое случайное слайд-шоу
    if (!imageFiles.empty()) {
        if (millis() - lastImageChange >= SLIDESHOW_INTERVAL) {
            // Получаем следующий случайный индекс
            int nextImageIndex = getNextRandomImage();
            displayImage(nextImageIndex);
        }
    }
    
    delay(10);
}