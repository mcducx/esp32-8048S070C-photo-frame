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

// Updated slideshow intervals: 5с, 30с, 1м, 5м, 15м, 30м, 60м
const unsigned long intervals[] = {
  5000,      // 5 seconds
  30000,     // 30 seconds
  60000,     // 1 minute
  300000,    // 5 minutes
  900000,    // 15 minutes
  1800000,   // 30 minutes
  3600000    // 60 minutes
};
int currentIntervalIndex = INTERVAL_DEFAULT_INDEX;
unsigned long slideshowInterval = intervals[currentIntervalIndex];

// Brightness
uint8_t currentBrightness = BRIGHTNESS_DEFAULT;

// System state
enum SystemState {
    STATE_SLIDESHOW,
    STATE_MENU,
    STATE_SETTING_INTERVAL,
    STATE_SETTING_BRIGHTNESS,
    STATE_INFO
};
SystemState currentState = STATE_SLIDESHOW;

// Menu
const char* menuItems[] = {"Set Interval", "Set Brightness", "System Info", "Exit"};
int menuItemCount = 4;
int selectedMenuItem = 0;
unsigned long menuLastInteraction = 0;

// Fatal error
bool fatalError = false;
String errorMessage = "";

// Button state
bool lastButtonState = HIGH;
bool buttonPressed = false;
unsigned long buttonPressTime = 0;
unsigned long buttonReleaseTime = 0;
bool longPressTriggered = false;

// Message display
unsigned long messageStartTime = 0;
bool showingMessage = false;
String currentMessage = "";
const unsigned long MESSAGE_DURATION = 2000;

// Loading screen
bool showingLoading = false;
String loadingMessage = "";
float loadingProgress = 0.0;
unsigned long lastProgressUpdate = 0;
const unsigned long PROGRESS_UPDATE_INTERVAL = 100;

// ==================== Forward Declarations ====================
void displayImage(int index);
void showMessage(const String& message, uint16_t color = CYAN);
void hideMessage();
void processButtonInput();
void handleShortPress();
void handleLongPress();
void saveIntervalToSD();
void loadIntervalFromSD();
void saveBrightnessToSD();
void loadBrightnessFromSD();
void initRandomSlideshow();
int getNextRandomImage();
void showMainMenu();
void showIntervalSetting();
void showBrightnessSetting();
void showSystemInfo();
void exitToSlideshow();
void adjustInterval(int direction);
void adjustBrightness(int direction);
void changeInterval();

// Loading functions
void showLoadingScreen(const String& message);
void updateLoadingProgress(float progress, const String& message = "");
void hideLoadingScreen();

// SD Card Functions
bool initSDCard();
int countTotalFiles();
void findImageFiles();
bool isSystemFile(const String& filename);
uint64_t getSDFreeSpace();
String formatBytes(uint64_t bytes);

// Debug function
void debugFileList();

// ==================== Utility Functions ====================
String formatBytes(uint64_t bytes) {
    if (bytes < 1024) {
        return String(bytes) + " B";
    } else if (bytes < 1024 * 1024) {
        return String(bytes / 1024.0, 1) + " KB";
    } else if (bytes < 1024 * 1024 * 1024) {
        return String(bytes / (1024.0 * 1024.0), 1) + " MB";
    } else {
        return String(bytes / (1024.0 * 1024.0 * 1024.0), 1) + " GB";
    }
}

uint64_t getSDFreeSpace() {
    if (!SD.exists("/")) {
        return 0;
    }
    uint64_t total = SD.totalBytes();
    uint64_t used = SD.usedBytes();
    return total - used;
}

// ==================== Loading Screen Functions ====================
void showLoadingScreen(const String& message) {
    showingLoading = true;
    loadingMessage = message;
    loadingProgress = 0.0;
    lastProgressUpdate = 0;
    
    gfx.fillScreen(BLACK);
    
    // Logo/Title
    gfx.setCursor(140, 280);
    gfx.setTextSize(3);
    gfx.setTextColor(CYAN);
    gfx.print("Photo Frame");
    
    // Message
    gfx.setCursor(80, 340);
    gfx.setTextSize(2);
    gfx.setTextColor(WHITE);
    gfx.print(message);
    
    // Progress bar background
    gfx.drawRect(80, 390, 320, 25, WHITE);
}

void updateLoadingProgress(float progress, const String& message) {
    if (!showingLoading) return;
    
    unsigned long now = millis();
    if (now - lastProgressUpdate < PROGRESS_UPDATE_INTERVAL && progress < 1.0) {
        return;
    }
    lastProgressUpdate = now;
    
    loadingProgress = progress;
    if (progress < 0.0) progress = 0.0;
    if (progress > 1.0) progress = 1.0;
    
    int barWidth = (int)(progress * 300);
    
    // Clear progress bar area
    gfx.fillRect(82, 392, 300, 21, BLACK);
    
    // Draw progress bar
    gfx.fillRect(82, 392, barWidth, 21, GREEN);
    gfx.drawRect(80, 390, 320, 25, WHITE);
    
    // Show percentage
    gfx.setCursor(200, 395);
    gfx.setTextSize(1);
    gfx.setTextColor(WHITE);
    gfx.printf("%.0f%%", progress * 100);
    
    // Update message if provided
    if (message.length() > 0) {
        loadingMessage = message;
        gfx.fillRect(80, 420, 320, 30, BLACK);
        gfx.setCursor(80, 425);
        gfx.setTextSize(1);
        gfx.setTextColor(YELLOW);
        gfx.print(message);
    }
}

void hideLoadingScreen() {
    showingLoading = false;
    gfx.fillScreen(BLACK);
}

// ==================== SD Card Functions ====================
bool initSDCard() {
    Serial.println("Initializing SD card...");
    updateLoadingProgress(0.0, "Initializing SD card...");
    
    sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    delay(100);
    
    if (!SD.begin(SD_CS, sdSPI, 40000000)) {
        if (!SD.begin(SD_CS, sdSPI, 20000000)) {
            Serial.println("SD card initialization failed!");
            updateLoadingProgress(0.0, "SD card failed!");
            delay(1000);
            return false;
        }
    }
    
    updateLoadingProgress(0.1, "SD card detected");
    
    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        Serial.println("No SD card attached");
        updateLoadingProgress(0.1, "No SD card!");
        delay(1000);
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
    updateLoadingProgress(0.15, String(cardSize) + "MB detected");
    
    updateLoadingProgress(0.2, "Loading settings...");
    loadIntervalFromSD();
    loadBrightnessFromSD();
    
    return true;
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
            
            if (savedIndex >= 0 && savedIndex < (sizeof(intervals) / sizeof(intervals[0]))) {
                currentIntervalIndex = savedIndex;
                slideshowInterval = intervals[currentIntervalIndex];
                Serial.printf("Interval loaded from SD: %d (index), %lu ms\n", 
                              currentIntervalIndex, slideshowInterval);
            } else {
                currentIntervalIndex = INTERVAL_DEFAULT_INDEX;
                slideshowInterval = intervals[currentIntervalIndex];
                Serial.printf("Invalid interval data, using default: %d (index), %lu ms\n", 
                              currentIntervalIndex, slideshowInterval);
                saveIntervalToSD();
            }
        } else {
            currentIntervalIndex = INTERVAL_DEFAULT_INDEX;
            slideshowInterval = intervals[currentIntervalIndex];
            Serial.printf("Failed to read interval file, using default: %d (index), %lu ms\n", 
                          currentIntervalIndex, slideshowInterval);
            saveIntervalToSD();
        }
    } else {
        currentIntervalIndex = INTERVAL_DEFAULT_INDEX;
        slideshowInterval = intervals[currentIntervalIndex];
        Serial.printf("Interval file not found, using default: %d (index), %lu ms\n", 
                      currentIntervalIndex, slideshowInterval);
        saveIntervalToSD();
    }
}

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

void loadBrightnessFromSD() {
    if (!SD.exists("/")) {
        Serial.println("SD card not available for loading brightness");
        currentBrightness = BRIGHTNESS_DEFAULT;
        set_brightness(currentBrightness);
        return;
    }
    
    if (SD.exists(BRIGHTNESS_FILENAME)) {
        File brightnessFile = SD.open(BRIGHTNESS_FILENAME, FILE_READ);
        if (brightnessFile) {
            String brightnessStr = brightnessFile.readString();
            brightnessFile.close();
            
            int savedBrightness = brightnessStr.toInt();
            
            if (savedBrightness >= MIN_BRIGHTNESS && savedBrightness <= MAX_BRIGHTNESS) {
                currentBrightness = savedBrightness;
                set_brightness(currentBrightness);
                Serial.printf("Brightness loaded from SD: %d\n", currentBrightness);
            } else {
                currentBrightness = BRIGHTNESS_DEFAULT;
                set_brightness(currentBrightness);
                Serial.printf("Invalid brightness data, using default: %d\n", currentBrightness);
                saveBrightnessToSD();
            }
        } else {
            currentBrightness = BRIGHTNESS_DEFAULT;
            set_brightness(currentBrightness);
            Serial.printf("Failed to read brightness file, using default: %d\n", currentBrightness);
            saveBrightnessToSD();
        }
    } else {
        currentBrightness = BRIGHTNESS_DEFAULT;
        set_brightness(currentBrightness);
        Serial.printf("Brightness file not found, using default: %d\n", currentBrightness);
        saveBrightnessToSD();
    }
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

// ==================== Image Management ====================
bool isSystemFile(const String& filename) {
    if (filename.startsWith("._")) return true;
    if (filename.equalsIgnoreCase(".DS_Store")) return true;
    if (filename.equalsIgnoreCase("Thumbs.db")) return true;
    if (filename.equalsIgnoreCase("desktop.ini")) return true;
    return false;
}

int countTotalFiles() {
    int totalFiles = 0;
    
    File root = SD.open("/");
    if (!root) {
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
    updateLoadingProgress(0.2, "Scanning for images...");
    
    File root = SD.open("/");
    if (!root) {
        Serial.println("Cannot open root directory");
        return;
    }
    
    imageFiles.clear();
    int fileCount = 0;
    int imageCount = 0;
    int totalFiles = countTotalFiles();
    
    if (totalFiles == 0) {
        Serial.println("No files found on SD card");
        updateLoadingProgress(0.5, "No files found");
        root.close();
        return;
    }
    
    Serial.printf("Total files to scan: %d\n", totalFiles);
    
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
        
        fileCount++;
        float progress = 0.2 + ((float)fileCount / totalFiles) * 0.7;
        
        String ext = filename.substring(filename.lastIndexOf('.'));
        ext.toLowerCase();
        
        if (ext == ".jpg" || ext == ".jpeg") {
            String path = "/" + String(entry.name());
            imageFiles.push_back(path);
            imageCount++;
            
            if (fileCount % 10 == 0 || fileCount == totalFiles) {
                updateLoadingProgress(progress, String(imageCount) + " images found");
            }
        }
        
        entry.close();
    }
    
    root.close();
    
    Serial.printf("Found %d images\n", imageFiles.size());
    updateLoadingProgress(0.9, String(imageFiles.size()) + " images found");
}

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

void displayImage(int index) {
    if (imageFiles.empty()) {
        return;
    }
    
    if (index < 0) index = 0;
    if (index >= imageFiles.size()) index = imageFiles.size() - 1;
    
    currentImageIndex = index;
    String path = imageFiles[currentImageIndex];
    
    Serial.printf("Displaying image %d/%d: %s\n", currentImageIndex + 1, imageFiles.size(), path.c_str());
    
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

// ==================== Message Functions ====================
void showMessage(const String& message, uint16_t color) {
    if (showingLoading || currentState != STATE_SLIDESHOW) return;
    
    gfx.fillRect(0, 0, 480, 50, BLACK);
    gfx.setCursor(10, 10);
    gfx.setTextSize(2);
    gfx.setTextColor(color);
    gfx.print(message);
    
    currentMessage = message;
    showingMessage = true;
    messageStartTime = millis();
}

void hideMessage() {
    if (showingMessage) {
        if (!imageFiles.empty() && currentState == STATE_SLIDESHOW) {
            displayImage(currentImageIndex);
        }
        showingMessage = false;
        currentMessage = "";
    }
}

// ==================== Debug Functions ====================
void debugFileList() {
    Serial.println("=== DEBUG File List ===");
    Serial.printf("Total image files in vector: %d\n", imageFiles.size());
    
    for(int i = 0; i < min(40, (int)imageFiles.size()); i++) {
        Serial.printf("%d: %s\n", i + 1, imageFiles[i].c_str());
    }
    
    if(imageFiles.size() > 40) {
        Serial.println("... and more");
    }
    Serial.println("=====================");
}

// ==================== Button Handling ====================
void processButtonInput() {
    int currentButtonState = digitalRead(BOOT_BUTTON_PIN);
    unsigned long now = millis();
    
    // Detect button press
    if (currentButtonState == LOW && lastButtonState == HIGH) {
        buttonPressed = true;
        buttonPressTime = now;
        buttonReleaseTime = 0;
        longPressTriggered = false;
    }
    
    // Detect button release
    if (currentButtonState == HIGH && lastButtonState == LOW && buttonPressed) {
        buttonReleaseTime = now;
        unsigned long pressDuration = buttonReleaseTime - buttonPressTime;
        
        if (pressDuration > SHORT_PRESS_TIME) {
            if (!longPressTriggered && pressDuration < LONG_PRESS_TIME) {
                handleShortPress();
            } else if (longPressTriggered) {
                // Long press was already handled during hold
            }
        }
        
        buttonPressed = false;
        longPressTriggered = false;
    }
    
    // Detect long press while holding
    if (buttonPressed && !longPressTriggered && (now - buttonPressTime > LONG_PRESS_TIME)) {
        longPressTriggered = true;
        handleLongPress();
    }
    
    lastButtonState = currentButtonState;
    
    // Check timeout for settings screens
    if (currentState == STATE_SETTING_INTERVAL || currentState == STATE_SETTING_BRIGHTNESS) {
        if (now - menuLastInteraction > SETTING_TIMEOUT) {
            // Return to main menu after 5 seconds of inactivity
            currentState = STATE_MENU;
            showMainMenu();
            Serial.println("Settings timeout - returning to menu");
        }
    }
    
    // Check timeout for main menu
    if (currentState == STATE_MENU) {
        if (now - menuLastInteraction > MENU_TIMEOUT) {
            // Return to slideshow after 10 seconds of inactivity
            exitToSlideshow();
            Serial.println("Menu timeout - returning to slideshow");
        }
    }
}

void handleShortPress() {
    menuLastInteraction = millis();
    
    switch (currentState) {
        case STATE_SLIDESHOW:
            // Enter menu
            currentState = STATE_MENU;
            showMainMenu();
            Serial.println("Entered menu");
            break;
            
        case STATE_MENU:
            // Select menu item
            switch (selectedMenuItem) {
                case 0:  // Set Interval
                    currentState = STATE_SETTING_INTERVAL;
                    showIntervalSetting();
                    Serial.println("Selected: Set Interval");
                    break;
                case 1:  // Set Brightness
                    currentState = STATE_SETTING_BRIGHTNESS;
                    showBrightnessSetting();
                    Serial.println("Selected: Set Brightness");
                    break;
                case 2:  // System Info
                    currentState = STATE_INFO;
                    showSystemInfo();
                    Serial.println("Selected: System Info");
                    break;
                case 3:  // Exit
                    exitToSlideshow();
                    break;
            }
            break;
            
        case STATE_SETTING_INTERVAL:
            // Change interval (next value)
            adjustInterval(1);
            break;
            
        case STATE_SETTING_BRIGHTNESS:
            // Increase brightness
            adjustBrightness(1);
            break;
            
        case STATE_INFO:
            // Exit info to menu
            currentState = STATE_MENU;
            showMainMenu();
            break;
    }
}

void handleLongPress() {
    menuLastInteraction = millis();
    
    switch (currentState) {
        case STATE_SLIDESHOW:
            // Change interval directly
            changeInterval();
            break;
            
        case STATE_MENU:
            // Navigate to next menu item
            selectedMenuItem = (selectedMenuItem + 1) % menuItemCount;
            showMainMenu();
            Serial.printf("Menu navigation: %s\n", menuItems[selectedMenuItem]);
            break;
            
        case STATE_SETTING_INTERVAL:
            // Decrease interval
            adjustInterval(-1);
            break;
            
        case STATE_SETTING_BRIGHTNESS:
            // Decrease brightness
            adjustBrightness(-1);
            break;
            
        case STATE_INFO:
            // Exit info to slideshow
            exitToSlideshow();
            break;
    }
}

void changeInterval() {
    currentIntervalIndex = (currentIntervalIndex + 1) % (sizeof(intervals) / sizeof(intervals[0]));
    slideshowInterval = intervals[currentIntervalIndex];
    
    saveIntervalToSD();
    
    // Format interval for display
    String intervalStr;
    if (slideshowInterval < 60000) {
        intervalStr = String(slideshowInterval / 1000) + " sec";
    } else {
        intervalStr = String(slideshowInterval / 60000) + " min";
    }
    
    showMessage("Interval: " + intervalStr, GREEN);
    lastImageChange = millis();
    
    Serial.printf("Interval changed to: %lu ms\n", slideshowInterval);
}

// ==================== Menu Functions ====================
void showMainMenu() {
    gfx.fillScreen(BLACK);
    
    // Title
    gfx.setCursor(150, 50);
    gfx.setTextSize(3);
    gfx.setTextColor(CYAN);
    gfx.print("Settings");
    
    // Menu items
    gfx.setTextSize(2);
    for (int i = 0; i < menuItemCount; i++) {
        int y = 150 + i * 50;
        
        if (i == selectedMenuItem) {
            gfx.fillRect(100, y - 5, 280, 30, BLUE);
            gfx.setTextColor(WHITE);
            gfx.setCursor(120, y);
            gfx.print("> ");
            gfx.print(menuItems[i]);
        } else {
            gfx.setTextColor(GREEN);
            gfx.setCursor(140, y);
            gfx.print(menuItems[i]);
        }
    }
    
    // Instructions
    gfx.setCursor(50, 400);
    gfx.setTextSize(1);
    gfx.setTextColor(YELLOW);
    gfx.print("Short: Select/Change  Long: Navigate/Adjust");
    
    gfx.setCursor(100, 430);
    gfx.print("Auto-exit in 10 seconds");
}

void showIntervalSetting() {
    gfx.fillScreen(BLACK);
    
    // Title
    gfx.setCursor(100, 50);
    gfx.setTextSize(3);
    gfx.setTextColor(CYAN);
    gfx.print("Set Interval");
    
    // Current value
    gfx.setCursor(150, 200);
    gfx.setTextSize(4);
    gfx.setTextColor(GREEN);
    
    // Format interval for display
    if (slideshowInterval < 60000) {
        gfx.print(slideshowInterval / 1000);
        gfx.print(" sec");
    } else {
        gfx.print(slideshowInterval / 60000);
        gfx.print(" min");
    }
    
    // Options
    gfx.setCursor(50, 300);
    gfx.setTextSize(2);
    gfx.setTextColor(YELLOW);
    gfx.print("5s, 30s, 1m, 5m, 15m, 30m, 60m");
    
    // Instructions
    gfx.setCursor(50, 400);
    gfx.setTextSize(1);
    gfx.setTextColor(WHITE);
    gfx.print("Short: Next interval  Long: Previous");
    
    gfx.setCursor(50, 420);
    gfx.print("Auto-return to menu in 5 seconds");
}

void showBrightnessSetting() {
    gfx.fillScreen(BLACK);
    
    // Title
    gfx.setCursor(100, 50);
    gfx.setTextSize(3);
    gfx.setTextColor(CYAN);
    gfx.print("Set Brightness");
    
    // Current value
    gfx.setCursor(150, 200);
    gfx.setTextSize(4);
    gfx.setTextColor(GREEN);
    gfx.print(currentBrightness);
    gfx.print("/255");
    
    // Visual indicator
    int barWidth = map(currentBrightness, MIN_BRIGHTNESS, MAX_BRIGHTNESS, 0, 300);
    gfx.fillRect(90, 280, 300, 30, DARKGREY);
    gfx.fillRect(90, 280, barWidth, 30, GREEN);
    gfx.drawRect(90, 280, 300, 30, WHITE);
    
    // Instructions
    gfx.setCursor(50, 400);
    gfx.setTextSize(1);
    gfx.setTextColor(WHITE);
    gfx.print("Short: Increase  Long: Decrease");
    
    gfx.setCursor(50, 420);
    gfx.print("Auto-return to menu in 5 seconds");
}

void showSystemInfo() {
    gfx.fillScreen(BLACK);
    
    // Title
    gfx.setCursor(150, 50);
    gfx.setTextSize(3);
    gfx.setTextColor(CYAN);
    gfx.print("System Info");
    
    // Info
    gfx.setTextSize(2);
    
    int y = 150;
    int lineHeight = 40;
    
    // Interval
    gfx.setTextColor(WHITE);
    gfx.setCursor(50, y);
    gfx.print("Interval: ");
    gfx.setTextColor(GREEN);
    if (slideshowInterval < 60000) {
        gfx.print(slideshowInterval / 1000);
        gfx.print(" sec");
    } else {
        gfx.print(slideshowInterval / 60000);
        gfx.print(" min");
    }
    
    y += lineHeight;
    
    // Brightness
    gfx.setTextColor(WHITE);
    gfx.setCursor(50, y);
    gfx.print("Brightness: ");
    gfx.setTextColor(GREEN);
    gfx.print(currentBrightness);
    
    y += lineHeight;
    
    // Image count
    gfx.setTextColor(WHITE);
    gfx.setCursor(50, y);
    gfx.print("Images: ");
    gfx.setTextColor(GREEN);
    gfx.print(imageFiles.size());
    
    y += lineHeight;
    
    // SD Card free space
    gfx.setTextColor(WHITE);
    gfx.setCursor(50, y);
    gfx.print("SD Free: ");
    gfx.setTextColor(GREEN);
    
    uint64_t freeSpace = getSDFreeSpace();
    if (freeSpace > 0) {
        gfx.print(formatBytes(freeSpace));
    } else {
        gfx.print("N/A");
    }
    
    y += lineHeight;
    
    // Free memory
    gfx.setTextColor(WHITE);
    gfx.setCursor(50, y);
    gfx.print("Free RAM: ");
    gfx.setTextColor(GREEN);
    gfx.print(ESP.getFreeHeap() / 1024);
    gfx.print(" KB");
    
    // Instructions
    gfx.setCursor(100, 450);
    gfx.setTextSize(1);
    gfx.setTextColor(YELLOW);
    gfx.print("Press button to go back");
}

void exitToSlideshow() {
    currentState = STATE_SLIDESHOW;
    
    if (!imageFiles.empty()) {
        displayImage(currentImageIndex);
        Serial.println("Exited to slideshow");
    } else {
        gfx.fillScreen(BLACK);
        gfx.setCursor(100, 350);
        gfx.setTextSize(2);
        gfx.setTextColor(RED);
        gfx.print("No images found");
        gfx.setCursor(80, 400);
        gfx.setTextSize(1);
        gfx.setTextColor(YELLOW);
        gfx.print("Please add JPEG images to SD card");
    }
}

void adjustInterval(int direction) {
    if (direction > 0) {
        // Increase interval
        currentIntervalIndex = (currentIntervalIndex + 1) % (sizeof(intervals) / sizeof(intervals[0]));
    } else {
        // Decrease interval
        currentIntervalIndex = (currentIntervalIndex - 1 + (sizeof(intervals) / sizeof(intervals[0]))) % (sizeof(intervals) / sizeof(intervals[0]));
    }
    
    slideshowInterval = intervals[currentIntervalIndex];
    saveIntervalToSD();
    
    // Update display
    showIntervalSetting();
    
    Serial.printf("Interval changed to: %lu ms\n", slideshowInterval);
}

void adjustBrightness(int direction) {
    int newBrightness = currentBrightness + (direction > 0 ? BRIGHTNESS_STEP : -BRIGHTNESS_STEP);
    
    if (newBrightness < MIN_BRIGHTNESS) newBrightness = MIN_BRIGHTNESS;
    if (newBrightness > MAX_BRIGHTNESS) newBrightness = MAX_BRIGHTNESS;
    
    if (newBrightness != currentBrightness) {
        currentBrightness = newBrightness;
        set_brightness(currentBrightness);
        saveBrightnessToSD();
        
        // Update display
        showBrightnessSetting();
        
        Serial.printf("Brightness changed to: %d\n", currentBrightness);
    }
}

// ==================== Setup ====================
void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n" + String(60, '='));
    Serial.println("ESP32 Photo Frame - Standalone Version");
    Serial.println("No Wi-Fi / No Web Interface");
    Serial.println("Intervals: 5s, 30s, 1m, 5m, 15m, 30m, 60m");
    Serial.println("Short press: Open menu / Select");
    Serial.println("Long press: Change interval / Navigate");
    Serial.println("Menu auto-close: 10s, Settings auto-close: 5s");
    Serial.println(String(60, '='));
    
    randomSeed(micros());
    
    // Initialize button
    pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
    
    // Initialize display
    setup_display();
    gfx.setRotation(1);
    
    // Show initial loading screen
    showLoadingScreen("Starting...");
    delay(500);
    
    // Initialize JPG decoder
    TJpgDec.setCallback(tft_output);
    
    // Try to initialize SD card
    bool sdInitialized = initSDCard();
    
    if (sdInitialized) {
        // Find images
        findImageFiles();
        debugFileList();
        
        if (!imageFiles.empty()) {
            initRandomSlideshow();
            
            // Finalize loading
            updateLoadingProgress(1.0, "Ready!");
            delay(500);
            hideLoadingScreen();
            
            // Start slideshow
            int firstImageIndex = shuffledIndices[currentShuffleIndex];
            displayImage(firstImageIndex);
            currentShuffleIndex++;
            
            Serial.println("\nSlideshow started!");
            Serial.printf("Total images: %d\n", imageFiles.size());
            
            // SD card info
            uint64_t totalSpace = SD.cardSize();
            uint64_t usedSpace = SD.usedBytes();
            uint64_t freeSpace = totalSpace - usedSpace;
            Serial.printf("SD Card: Total=%s, Used=%s, Free=%s\n", 
                formatBytes(totalSpace).c_str(),
                formatBytes(usedSpace).c_str(),
                formatBytes(freeSpace).c_str());
            
            // Format interval for display
            String intervalStr;
            if (slideshowInterval < 60000) {
                intervalStr = String(slideshowInterval / 1000) + " seconds";
            } else {
                intervalStr = String(slideshowInterval / 60000) + " minutes";
            }
            Serial.printf("Interval: %s\n", intervalStr.c_str());
            Serial.printf("Brightness: %d/255\n", currentBrightness);
        } else {
            errorMessage = "No JPEG images found on SD card";
            fatalError = true;
            Serial.println("\nERROR: " + errorMessage);
        }
    } else {
        errorMessage = "SD card initialization failed";
        fatalError = true;
        Serial.println("\nERROR: " + errorMessage);
    }
    
    // Show error if any
    if (fatalError) {
        hideLoadingScreen();
        gfx.fillScreen(BLACK);
        gfx.setCursor(50, 300);
        gfx.setTextSize(2);
        gfx.setTextColor(RED);
        gfx.print("ERROR:");
        
        gfx.setCursor(50, 350);
        gfx.setTextSize(1);
        gfx.setTextColor(WHITE);
        gfx.print(errorMessage);
        
        gfx.setCursor(100, 400);
        gfx.setTextSize(1);
        gfx.setTextColor(YELLOW);
        gfx.print("Press button for menu");
    }
    
    // Hide loading screen if still showing
    if (showingLoading) {
        hideLoadingScreen();
    }
}

// ==================== Loop ====================
void loop() {
    processButtonInput();
    
    // Handle messages
    if (showingMessage && (millis() - messageStartTime >= MESSAGE_DURATION)) {
        hideMessage();
    }
    
    // Handle slideshow
    if (!fatalError && currentState == STATE_SLIDESHOW && !imageFiles.empty() && !showingMessage) {
        if (millis() - lastImageChange >= slideshowInterval) {
            int nextImageIndex = getNextRandomImage();
            displayImage(nextImageIndex);
        }
    }
    
    delay(10);
}