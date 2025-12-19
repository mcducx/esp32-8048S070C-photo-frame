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
std::vector<int> shuffledIndices;
int currentImageIndex = 0;
int currentShuffleIndex = 0;
unsigned long lastImageChange = 0;

// Интервалы для слайд-шоу
const unsigned long intervals[] = {3000, 5000, 10000, 15000, 30000, 60000, 120000};
int currentIntervalIndex = 2;
unsigned long slideshowInterval = intervals[currentIntervalIndex];

bool fatalError = false;
unsigned long lastTouchTime = 0;
const unsigned long TOUCH_DEBOUNCE = 50; // 50 мс для защиты от дребезга

// ==================== Forward Declarations ====================
void displayImage(int index);
void showIntervalMessage();
void changeInterval();

// ==================== Filter System Files ====================
bool isSystemFile(const String& filename) {
    if (filename.startsWith("._")) return true;
    if (filename.equalsIgnoreCase(".DS_Store")) return true;
    if (filename.equalsIgnoreCase("Thumbs.db")) return true;
    if (filename.equalsIgnoreCase("desktop.ini")) return true;
    return false;
}

// ==================== Display Error Message ====================
void displayErrorScreen(const String& title, const String& message) {
    gfx.fillScreen(BLACK);
    
    gfx.setCursor(140, 350);
    gfx.setTextSize(2);
    gfx.setTextColor(RED);
    gfx.print(title);
    
    gfx.setCursor(100, 400);
    gfx.setTextSize(1);
    gfx.setTextColor(WHITE);
    gfx.print(message);
    
    gfx.setCursor(100, 450);
    gfx.setTextSize(1);
    gfx.print("Restart device to retry");
}

// ==================== Show Interval Message ====================
void showIntervalMessage() {
    // Временное сообщение поверх изображения
    gfx.fillRect(0, 0, 480, 50, BLACK);
    gfx.setCursor(10, 10);
    gfx.setTextSize(2);
    gfx.setTextColor(CYAN);
    gfx.print("Interval: ");
    gfx.print(slideshowInterval / 1000);
    gfx.print(" sec");
    
    delay(2000);
    
    // Перерисовываем изображение
    if (!imageFiles.empty()) {
        displayImage(currentImageIndex);
    }
}

// ==================== Change Interval ====================
void changeInterval() {
    currentIntervalIndex = (currentIntervalIndex + 1) % (sizeof(intervals) / sizeof(intervals[0]));
    slideshowInterval = intervals[currentIntervalIndex];
    showIntervalMessage();
    lastImageChange = millis();
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
    
    shuffledIndices.clear();
    for (int i = 0; i < imageFiles.size(); i++) {
        shuffledIndices.push_back(i);
    }
    
    for (int i = shuffledIndices.size() - 1; i > 0; i--) {
        int j = random(0, i + 1);
        std::swap(shuffledIndices[i], shuffledIndices[j]);
    }
    
    currentShuffleIndex = 0;
    
    Serial.println("Random slideshow order initialized");
}

// ==================== Get Next Random Image Index ====================
int getNextRandomImage() {
    if (imageFiles.empty()) return 0;
    
    int imageIndex = shuffledIndices[currentShuffleIndex];
    
    currentShuffleIndex++;
    
    if (currentShuffleIndex >= shuffledIndices.size()) {
        for (int i = shuffledIndices.size() - 1; i > 0; i--) {
            int j = random(0, i + 1);
            std::swap(shuffledIndices[i], shuffledIndices[j]);
        }
        
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

// ==================== Check Touch Input ====================
void processTouchInput() {
    uint16_t x, y;
    
    // Проверяем касание
    if (check_touch(&x, &y)) {
        // Защита от дребезга
        if (millis() - lastTouchTime > TOUCH_DEBOUNCE) {
            // Обычное нажатие - меняем интервал
            changeInterval();
            lastTouchTime = millis();
        }
    }
}

// ==================== Setup ====================
void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n" + String(60, '='));
    Serial.println("ESP32 Photo Frame - RANDOM SLIDESHOW");
    Serial.println("Tap screen to change interval");
    Serial.println(String(60, '='));
    
    randomSeed(analogRead(0));
    
    // Инициализация дисплея
    setup_display();
    
    // Установка портретного режима
    gfx.setRotation(1);
    ts.setRotation(1);
    
    // Очищаем экран
    gfx.fillScreen(BLACK);
    
    // Инициализация SD карты
    if (initSDCard()) {
        // Поиск изображений
        findImageFiles();
        
        if (!imageFiles.empty()) {
            // Инициализация случайного порядка слайдшоу
            initRandomSlideshow();
            
            // Показываем первое случайное изображение
            int firstImageIndex = shuffledIndices[currentShuffleIndex];
            displayImage(firstImageIndex);
            currentShuffleIndex++;
            
            Serial.println("\nRandom slideshow started!");
            Serial.printf("Initial interval: %d seconds\n", slideshowInterval / 1000);
            Serial.printf("Available intervals: 3, 5, 10, 15, 30, 60 seconds\n");
            Serial.printf("Total images: %d\n", imageFiles.size());
        } else {
            // Нет изображений
            fatalError = true;
            displayErrorScreen("NO IMAGES", "Add JPG files to SD card");
            Serial.println("\nERROR: No JPEG images found on SD card!");
            
            while (fatalError) {
                delay(100);
            }
        }
    } else {
        // Ошибка SD карты
        fatalError = true;
        displayErrorScreen("SD CARD ERROR", "Insert card with images");
        Serial.println("\nERROR: SD card initialization failed!");
        
        while (fatalError) {
            delay(100);
        }
    }
}

// ==================== Loop ====================
void loop() {
    if (fatalError) {
        delay(100);
        return;
    }
    
    // Обработка сенсорного ввода
    processTouchInput();
    
    // Автоматическое слайд-шоу
    if (!imageFiles.empty()) {
        if (millis() - lastImageChange >= slideshowInterval) {
            int nextImageIndex = getNextRandomImage();
            displayImage(nextImageIndex);
        }
    }
    
    delay(10);
}