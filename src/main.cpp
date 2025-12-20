#include "display.h"
#include "config.h"
#include <SD.h>
#include <SPI.h>
#include <TJpg_Decoder.h>
#include <vector>
#include <algorithm>

// ==================== Configuration ====================
#define INTERVAL_DEFAULT_INDEX 2
#define INTERVAL_FILENAME "/interval.txt"

// ==================== Button Configuration ====================
#define BOOT_BUTTON_PIN 0  // GPIO0 - кнопка BOOT на ESP32
bool lastButtonState = HIGH;
unsigned long lastButtonPress = 0;
const unsigned long BUTTON_COOLDOWN = 300;  // 300 мс между нажатиями

// ==================== Message Display ====================
unsigned long messageStartTime = 0;
bool showingMessage = false;
const unsigned long MESSAGE_DURATION = 1000;  // Сообщение показывается 1 секунду

// ==================== Global Variables ====================
SPIClass sdSPI = SPIClass(HSPI);
std::vector<String> imageFiles;
std::vector<int> shuffledIndices;
int currentImageIndex = 0;
int currentShuffleIndex = 0;
unsigned long lastImageChange = 0;

// Slideshow intervals
const unsigned long intervals[] = {3000, 5000, 10000, 15000, 30000, 60000, 120000};
int currentIntervalIndex = INTERVAL_DEFAULT_INDEX;
unsigned long slideshowInterval = intervals[currentIntervalIndex];

bool fatalError = false;

// Loading screen types
enum LoadingType {
    LOADING_SPINNER,
    LOADING_PROGRESS,
    LOADING_BAR,
    LOADING_DOTS
};
LoadingType currentLoadingType = LOADING_BAR;

// ==================== Forward Declarations ====================
void displayImage(int index);
void showIntervalMessage();
void changeInterval();
void saveIntervalToSD();
void loadIntervalFromSD();
void showLoadingScreen(const String& message = "Loading...");
void updateLoadingProgress(float progress, const String& message = "");
void hideLoadingScreen();
void processButtonInput();
void hideMessage();

// ==================== SD Card Interval Functions ====================
void saveIntervalToSD() {
    if (!SD.exists("/")) {
        Serial.println("SD card not available for saving interval");
        return;
    }
    
    File intervalFile = SD.open(INTERVAL_FILENAME, FILE_WRITE);
    if (intervalFile) {
        intervalFile.print(currentIntervalIndex);
        intervalFile.close();
        Serial.printf("Interval saved to SD: %d (index), %lu ms\n", 
                      currentIntervalIndex, slideshowInterval);
    } else {
        Serial.println("Failed to save interval to SD card!");
    }
}

void loadIntervalFromSD() {
    if (!SD.exists("/")) {
        Serial.println("SD card not available for loading interval");
        currentIntervalIndex = INTERVAL_DEFAULT_INDEX;
        slideshowInterval = intervals[currentIntervalIndex];
        return;
    }
    
    if (SD.exists(INTERVAL_FILENAME)) {
        File intervalFile = SD.open(INTERVAL_FILENAME, FILE_READ);
        if (intervalFile) {
            String intervalStr = intervalFile.readString();
            intervalFile.close();
            
            int savedIndex = intervalStr.toInt();
            
            // Validate the saved index
            if (savedIndex >= 0 && savedIndex < (sizeof(intervals) / sizeof(intervals[0]))) {
                currentIntervalIndex = savedIndex;
                slideshowInterval = intervals[currentIntervalIndex];
                Serial.printf("Interval loaded from SD: %d (index), %lu ms\n", 
                              currentIntervalIndex, slideshowInterval);
            } else {
                // Invalid data - use default
                currentIntervalIndex = INTERVAL_DEFAULT_INDEX;
                slideshowInterval = intervals[currentIntervalIndex];
                Serial.printf("Invalid interval data, using default: %d (index), %lu ms\n", 
                              currentIntervalIndex, slideshowInterval);
                saveIntervalToSD(); // Save default
            }
        } else {
            currentIntervalIndex = INTERVAL_DEFAULT_INDEX;
            slideshowInterval = intervals[currentIntervalIndex];
            Serial.printf("Failed to read interval file, using default: %d (index), %lu ms\n", 
                          currentIntervalIndex, slideshowInterval);
            saveIntervalToSD(); // Create with default
        }
    } else {
        // File doesn't exist - use default and create file
        currentIntervalIndex = INTERVAL_DEFAULT_INDEX;
        slideshowInterval = intervals[currentIntervalIndex];
        Serial.printf("Interval file not found, using default: %d (index), %lu ms\n", 
                      currentIntervalIndex, slideshowInterval);
        saveIntervalToSD(); // Create with default
    }
}

// ==================== Loading Screen Functions ====================
void showLoadingScreen(const String& message) {
    gfx.fillScreen(BLACK);
    
    // Draw title
    gfx.setCursor(140, 300);
    gfx.setTextSize(3);
    gfx.setTextColor(CYAN);
    gfx.print("Photo Frame");
    
    // Draw message
    gfx.setCursor(150, 350);
    gfx.setTextSize(2);
    gfx.setTextColor(WHITE);
    gfx.print(message);
    
    // Draw initial loading indicator based on type
    switch(currentLoadingType) {
        case LOADING_SPINNER:
            // Initial spinner position will be drawn in update function
            break;
        case LOADING_PROGRESS:
            gfx.setCursor(180, 400);
            gfx.setTextSize(2);
            gfx.print("0%");
            break;
        case LOADING_BAR:
            // Draw empty progress bar
            gfx.drawRect(100, 395, 280, 20, WHITE);
            break;
        case LOADING_DOTS:
            gfx.setCursor(220, 400);
            gfx.setTextSize(2);
            gfx.print(".");
            break;
    }
}

void updateLoadingProgress(float progress, const String& message) {
    static unsigned long lastUpdate = 0;
    static int spinnerAngle = 0;
    static int dotCount = 0;
    
    // Throttle updates for smoother animation
    if (millis() - lastUpdate < 50 && progress < 1.0) return;
    lastUpdate = millis();
    
    // Clamp progress between 0 and 1
    if (progress < 0.0) progress = 0.0;
    if (progress > 1.0) progress = 1.0;
    
    int centerX = 240;
    int centerY = 420;
    
    switch(currentLoadingType) {
        case LOADING_SPINNER: {
            // Clear previous spinner
            gfx.fillCircle(centerX, centerY, 30, BLACK);
            
            // Draw new spinner
            spinnerAngle = (spinnerAngle + 30) % 360;
            for (int i = 0; i < 12; i++) {
                int angle = spinnerAngle + i * 30;
                float rad = angle * 3.14159 / 180.0;
                int x1 = centerX + cos(rad) * 20;
                int y1 = centerY + sin(rad) * 20;
                int x2 = centerX + cos(rad) * 28;
                int y2 = centerY + sin(rad) * 28;
                
                int brightness = 255 - i * 20;
                if (brightness < 50) brightness = 50;
                uint16_t color = gfx.color565(brightness, brightness, brightness);
                
                gfx.drawLine(x1, y1, x2, y2, color);
            }
            break;
        }
            
        case LOADING_PROGRESS: {
            // Update percentage
            gfx.fillRect(160, 395, 160, 30, BLACK);
            gfx.setCursor(180, 400);
            gfx.setTextSize(2);
            gfx.setTextColor(WHITE);
            gfx.printf("%.0f%%", progress * 100);
            break;
        }
            
        case LOADING_BAR: {
            int barWidth = (int)(progress * 260);
            // Clear progress bar area
            gfx.fillRect(102, 397, 260, 16, BLACK);
            
            // Draw progress bar
            gfx.fillRect(102, 397, barWidth, 16, GREEN);
            
            // Draw bar border
            gfx.drawRect(100, 395, 280, 20, WHITE);
            
            // Show percentage inside bar if space
            if (barWidth > 40) {
                gfx.setCursor(102 + barWidth/2 - 15, 398);
                gfx.setTextSize(1);
                gfx.setTextColor(BLACK);
                gfx.printf("%.0f%%", progress * 100);
            }
            break;
        }
            
        case LOADING_DOTS: {
            // Animate dots
            dotCount = (dotCount + 1) % 4;
            gfx.fillRect(200, 395, 80, 30, BLACK);
            gfx.setCursor(220, 400);
            gfx.setTextSize(2);
            for (int i = 0; i < dotCount; i++) {
                gfx.print(".");
            }
            break;
        }
    }
    
    // Update message if provided
    if (message.length() > 0) {
        gfx.fillRect(100, 430, 280, 25, BLACK);
        gfx.setCursor(100, 430);
        gfx.setTextSize(1);
        gfx.setTextColor(WHITE);
        gfx.print(message);
    }
}

void hideLoadingScreen() {
    // Just clear the screen - next image will be drawn
    gfx.fillScreen(BLACK);
}

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
    // Draw message overlay
    gfx.fillRect(0, 0, 480, 50, BLACK);
    gfx.setCursor(10, 10);
    gfx.setTextSize(2);
    gfx.setTextColor(CYAN);
    gfx.print("Interval: ");
    gfx.print(slideshowInterval / 1000);
    gfx.print(" sec");
    
    // Set message display state
    showingMessage = true;
    messageStartTime = millis();
}

// ==================== Hide Message ====================
void hideMessage() {
    if (showingMessage) {
        // Redraw current image to remove message
        if (!imageFiles.empty()) {
            displayImage(currentImageIndex);
        }
        showingMessage = false;
    }
}

// ==================== Change Interval ====================
void changeInterval() {
    currentIntervalIndex = (currentIntervalIndex + 1) % (sizeof(intervals) / sizeof(intervals[0]));
    slideshowInterval = intervals[currentIntervalIndex];
    
    // Save new interval to SD card
    saveIntervalToSD();
    
    // Show message
    showIntervalMessage();
    
    // Reset slideshow timer
    lastImageChange = millis();
    
    Serial.printf("Interval changed to: %lu ms\n", slideshowInterval);
}

// ==================== Process Button Input ====================
void processButtonInput() {
    int currentButtonState = digitalRead(BOOT_BUTTON_PIN);
    unsigned long now = millis();
    
    // Detect button press (LOW = pressed)
    if (currentButtonState == LOW && lastButtonState == HIGH) {
        // Simple debounce - check again after 10ms
        delay(10);
        if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
            // Prevent multiple rapid presses
            if (now - lastButtonPress > BUTTON_COOLDOWN) {
                lastButtonPress = now;
                
                // Immediately change interval
                changeInterval();
                
                Serial.println("Button pressed - interval changed");
            }
        }
    }
    
    // Update button state
    lastButtonState = currentButtonState;
}

// ==================== SD Card Initialization ====================
bool initSDCard() {
    Serial.println("Initializing SD card...");
    updateLoadingProgress(0.0, "Initializing SD card...");
    
    sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    delay(100);
    
    if (!SD.begin(SD_CS, sdSPI, 4000000)) {
        Serial.println("SD card initialization failed!");
        return false;
    }
    
    updateLoadingProgress(0.1, "SD card detected");
    
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
    
    // Load interval from SD card after SD is initialized
    updateLoadingProgress(0.15, "Loading settings...");
    loadIntervalFromSD();
    
    return true;
}

// ==================== Find All Image Files ====================
int countTotalFiles() {
    int totalFiles = 0;
    
    File root = SD.open("/");
    if (!root) {
        Serial.println("Cannot open root directory for counting");
        return 0;
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
        
        totalFiles++;
        entry.close();
    }
    
    root.close();
    return totalFiles;
}

void findImageFiles() {
    Serial.println("Scanning for images...");
    
    // Сначала подсчитаем общее количество файлов
    updateLoadingProgress(0.2, "Counting files...");
    int totalFiles = countTotalFiles();
    
    Serial.printf("Total non-system files to scan: %d\n", totalFiles);
    
    if (totalFiles == 0) {
        updateLoadingProgress(1.0, "No files found");
        return;
    }
    
    // Теперь сканируем файлы с реальным прогрессом
    updateLoadingProgress(0.2, "Scanning for images...");
    
    File root = SD.open("/");
    if (!root) {
        Serial.println("Cannot open root directory");
        return;
    }
    
    int processedFiles = 0;
    int imageCount = 0;
    
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
        
        processedFiles++;
        
        // Обновляем прогресс от 20% до 100% в зависимости от обработанных файлов
        float progress = 0.2 + ((float)processedFiles / totalFiles) * 0.8;
        
        // Обновляем прогресс каждые 5 файлов или если это последний файл
        if (processedFiles % 5 == 0 || processedFiles == totalFiles) {
            updateLoadingProgress(progress, "Found: " + String(imageCount) + " images");
        }
        
        String ext = filename.substring(filename.lastIndexOf('.'));
        ext.toLowerCase();
        
        if (ext == ".jpg" || ext == ".jpeg") {
            String path = "/" + String(entry.name());
            imageFiles.push_back(path);
            imageCount++;
            Serial.printf("Found image: %s (%d bytes)\n", path.c_str(), entry.size());
        }
        
        entry.close();
    }
    
    root.close();
    
    Serial.printf("Found %d images\n", imageFiles.size());
    updateLoadingProgress(1.0, String(imageFiles.size()) + " images found");
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

// ==================== Setup ====================
void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n" + String(60, '='));
    Serial.println("ESP32 Photo Frame - RANDOM SLIDESHOW");
    Serial.println("Press BOOT button to change interval");
    Serial.println("Interval saved to SD card (interval.txt)");
    Serial.println(String(60, '='));
    
    randomSeed(analogRead(0));
    
    // Initialize BOOT button
    pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
    
    // Display initialization
    setup_display();
    
    // Set portrait mode (only display, touch not used)
    gfx.setRotation(1);
    
    // Show loading screen immediately
    showLoadingScreen("Starting...");
    delay(300); // Brief pause to show loading screen
    
    // SD card initialization
    if (initSDCard()) {
        // Find images with real progress from 0% to 100%
        findImageFiles();
        
        if (!imageFiles.empty()) {
            // Initialize random slideshow order
            initRandomSlideshow();
            
            delay(300); // Show completion briefly
            
            // Hide loading screen and show first image
            hideLoadingScreen();
            
            // Show first random image
            int firstImageIndex = shuffledIndices[currentShuffleIndex];
            displayImage(firstImageIndex);
            currentShuffleIndex++;
            
            Serial.println("\nRandom slideshow started!");
            Serial.printf("Current interval: %lu ms (%d seconds)\n", 
                         slideshowInterval, slideshowInterval / 1000);
            Serial.printf("Saved interval index: %d\n", currentIntervalIndex);
            Serial.printf("Available intervals: 3, 5, 10, 15, 30, 60, 120 seconds\n");
            Serial.printf("Total images: %d\n", imageFiles.size());
            Serial.println("Press BOOT button to change slideshow interval");
        } else {
            // No images found
            fatalError = true;
            hideLoadingScreen();
            displayErrorScreen("NO IMAGES", "Add JPG files to SD card");
            Serial.println("\nERROR: No JPEG images found on SD card!");
            
            while (fatalError) {
                delay(100);
            }
        }
    } else {
        // SD card error
        fatalError = true;
        hideLoadingScreen();
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
    
    // Process button input
    processButtonInput();
    
    // Hide message after duration
    if (showingMessage && (millis() - messageStartTime >= MESSAGE_DURATION)) {
        hideMessage();
    }
    
    // Automatic slideshow (only when not showing message)
    if (!imageFiles.empty() && !showingMessage) {
        if (millis() - lastImageChange >= slideshowInterval) {
            int nextImageIndex = getNextRandomImage();
            displayImage(nextImageIndex);
        }
    }
    
    // Small delay to prevent watchdog issues
    delay(10);
}