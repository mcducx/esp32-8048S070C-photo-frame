#include "display.h"
#include "config.h"
#include <SD.h>
#include <SPI.h>
#include <TJpg_Decoder.h>
#include <vector>
#include <algorithm>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <ArduinoJson.h>

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
//const unsigned long SETTING_TIMEOUT = 5000; // 5 секунд для выхода из настроек
//const unsigned long MENU_TIMEOUT = 10000;  // 10 секунд для выхода из главного меню

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

// Wi-Fi & OTA
bool wifiConnected = false;
WebServer server(WEB_PORT);
String wifiStatusMessage = "";
unsigned long lastWifiCheck = 0;
const unsigned long WIFI_CHECK_INTERVAL = 30000;

// Loading screen
bool showingLoading = false;
String loadingMessage = "";
float loadingProgress = 0.0;
unsigned long lastProgressUpdate = 0;
const unsigned long PROGRESS_UPDATE_INTERVAL = 100;

// OTA update progress
unsigned long otaTotalSize = 0;
unsigned long otaCurrentSize = 0;
bool otaInProgress = false;

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

// Wi-Fi & OTA Functions
void initWiFi();
void checkWiFiConnection();
void initWebServer();
void handleRoot();
void handleOTA();
void handleAPI();

// SD Card Functions
bool initSDCard();
int countTotalFiles();
void findImageFiles();
bool isSystemFile(const String& filename);

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
    
    // Wi-Fi status
    gfx.setCursor(50, y);
    gfx.setTextColor(WHITE);
    gfx.print("Wi-Fi: ");
    gfx.setTextColor(wifiConnected ? GREEN : RED);
    gfx.print(wifiConnected ? "Connected" : "Disabled");
    
    if (wifiConnected) {
        gfx.setTextColor(WHITE);
        gfx.setCursor(50, y + 20);
        gfx.setTextSize(1);
        gfx.print("IP: ");
        gfx.setTextColor(CYAN);
        gfx.print(WiFi.localIP().toString());
        gfx.setTextSize(2);
    }
    
    y += lineHeight;
    
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
        
        if (wifiConnected) {
            gfx.setCursor(50, 400);
            gfx.setTextSize(1);
            gfx.setTextColor(CYAN);
            gfx.print("OTA: ");
            gfx.print(WiFi.localIP().toString());
        }
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

// ==================== Wi-Fi & OTA Functions ====================
void initWiFi() {
    Serial.println("Initializing Wi-Fi...");
    updateLoadingProgress(0.3, "Connecting to Wi-Fi...");
    
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(OTA_HOSTNAME);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        float progress = 0.3 + (attempts * 0.02);
        updateLoadingProgress(progress, "Connecting to Wi-Fi...");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        Serial.println("\nWi-Fi connected!");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        
        updateLoadingProgress(0.7, "Wi-Fi connected!");
        
        // Initialize mDNS
        if (!MDNS.begin(OTA_HOSTNAME)) {
            Serial.println("Error setting up MDNS responder!");
        } else {
            Serial.println("mDNS responder started");
            MDNS.addService("http", "tcp", 80);
        }
    } else {
        wifiConnected = false;
        Serial.println("\nWi-Fi connection failed!");
        updateLoadingProgress(0.7, "Wi-Fi failed!");
    }
}

void checkWiFiConnection() {
    if (millis() - lastWifiCheck > WIFI_CHECK_INTERVAL) {
        lastWifiCheck = millis();
        
        if (WiFi.status() != WL_CONNECTED) {
            wifiConnected = false;
            Serial.println("Wi-Fi disconnected, attempting to reconnect...");
            WiFi.reconnect();
            
            delay(1000);
            if (WiFi.status() == WL_CONNECTED) {
                wifiConnected = true;
                Serial.println("Wi-Fi reconnected!");
            }
        }
    }
}

// ==================== API Functions ====================
void handleAPIFiles() {
    // GET request - return list of files (only names)
    if (server.method() == HTTP_GET) {
        Serial.println("GET /api/files - Processing request");
        
        // Создаем JSON документ с достаточным размером
        DynamicJsonDocument doc(24576); // Увеличил размер для большего количества файлов
        JsonArray files = doc.to<JsonArray>();
        
        // Просто добавляем имена файлов без открытия каждого файла
        for (const String& filepath : imageFiles) {
            // Извлекаем только имя файла из пути
            String filename = filepath.substring(1); // Убираем ведущий "/"
            
            // Находим последний слэш (если есть подкаталоги)
            int lastSlash = filename.lastIndexOf('/');
            if (lastSlash != -1) {
                filename = filename.substring(lastSlash + 1);
            }
            
            files.add(filename);
        }
        
        doc["count"] = imageFiles.size();
        doc["status"] = "success";
        
        String response;
        size_t jsonSize = serializeJson(doc, response);
        Serial.printf("JSON size: %d bytes, Files: %d\n", jsonSize, imageFiles.size());
        
        server.send(200, "application/json", response);
    }
    // POST request - upload file
    else if (server.method() == HTTP_POST) {
        HTTPUpload& upload = server.upload();
        static File uploadFile;
        
        if (upload.status == UPLOAD_FILE_START) {
            String filename = "/" + upload.filename;
            Serial.printf("Upload start: %s\n", filename.c_str());
            
            // Close previous file if open
            if (uploadFile) {
                uploadFile.close();
            }
            
            SD.remove(filename);
            uploadFile = SD.open(filename, FILE_WRITE);
            if (!uploadFile) {
                Serial.println("Failed to open file for writing");
            }
        } 
        else if (upload.status == UPLOAD_FILE_WRITE) {
            if (uploadFile) {
                uploadFile.write(upload.buf, upload.currentSize);
            }
        } 
        else if (upload.status == UPLOAD_FILE_END) {
            if (uploadFile) {
                uploadFile.close();
                Serial.printf("Upload complete: %s, Size: %u\n", 
                             upload.filename.c_str(), upload.totalSize);
                
                // Refresh image list
                findImageFiles();
                initRandomSlideshow();
                
                server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"File uploaded successfully\"}");
            } else {
                server.send(500, "application/json", "{\"status\":\"error\",\"message\":\"Failed to save file\"}");
            }
        }
    }
    // DELETE request - delete file
    else if (server.method() == HTTP_DELETE) {
        if (server.hasArg("filename")) {
            String filename = "/" + server.arg("filename");
            
            if (SD.exists(filename)) {
                if (SD.remove(filename)) {
                    // Refresh image list
                    findImageFiles();
                    initRandomSlideshow();
                    
                    server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"File deleted successfully\"}");
                } else {
                    server.send(500, "application/json", "{\"status\":\"error\",\"message\":\"Failed to delete file\"}");
                }
            } else {
                server.send(404, "application/json", "{\"status\":\"error\",\"message\":\"File not found\"}");
            }
        } else {
            server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing filename parameter\"}");
        }
    }
}

// ==================== Web Server Handlers ====================
void handleRoot() {
    String html = "<!DOCTYPE html><html><head><title>ESP32 Photo Frame OTA</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<meta charset='UTF-8'>";
    html += "<style>";
    html += "body{font-family:Arial,sans-serif;margin:0;padding:20px;background:#f0f0f0;min-height:100vh}";
    html += ".container{max-width:600px;margin:0 auto;background:white;padding:20px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1)}";
    html += "h1{color:#333;text-align:center;margin-bottom:20px}";
    html += ".status{margin:20px 0;padding:15px;background:#f8f9fa;border-radius:5px}";
    html += ".status-item{margin:10px 0}";
    html += ".label{font-weight:bold;color:#555}";
    html += ".value{color:#333}";
    html += ".ota-section{margin:30px 0;padding:20px;background:#e8f4fd;border-radius:5px;border:2px solid #2196F3}";
    html += ".ota-section h2{margin-top:0;color:#2196F3}";
    html += "input[type='file']{width:100%;padding:10px;margin:10px 0;border:1px solid #ddd;border-radius:5px}";
    html += "input[type='submit']{background:#4CAF50;color:white;border:none;padding:12px 24px;border-radius:5px;cursor:pointer;font-size:16px;width:100%}";
    html += ".progress{width:100%;background:#ddd;border-radius:5px;margin:10px 0;display:none}";
    html += ".progress-bar{width:0%;height:20px;background:#4CAF50;border-radius:5px}";
    html += "</style>";
    html += "</head><body>";
    
    html += "<div class='container'>";
    html += "<h1>ESP32 Photo Frame - OTA Update</h1>";
    
    // Status
    html += "<div class='status'>";
    html += "<div class='status-item'><span class='label'>Device:</span> <span class='value'>" + String(OTA_HOSTNAME) + "</span></div>";
    html += "<div class='status-item'><span class='label'>Wi-Fi:</span> <span class='value'>" + (wifiConnected ? "Connected (" + WiFi.localIP().toString() + ")" : "Disconnected") + "</span></div>";
    html += "<div class='status-item'><span class='label'>Images:</span> <span class='value'>" + String(imageFiles.size()) + "</span></div>";
    html += "<div class='status-item'><span class='label'>Free Memory:</span> <span class='value'>" + String(ESP.getFreeHeap() / 1024) + " KB</span></div>";
    html += "</div>";
    
    // OTA Section
    html += "<div class='ota-section'>";
    html += "<h2>Firmware Update</h2>";
    html += "<form id='uploadForm' method='POST' action='/update' enctype='multipart/form-data'>";
    html += "<input type='file' name='update' accept='.bin' required id='fileInput'>";
    html += "<input type='submit' value='Upload Firmware' id='submitBtn'>";
    html += "</form>";
    
    html += "<div class='progress' id='progressContainer'>";
    html += "<div class='progress-bar' id='progressBar'></div>";
    html += "<div id='progressText' style='text-align:center;margin-top:5px'></div>";
    html += "</div>";
    html += "</div>";
    
    html += "<div style='text-align:center;margin-top:20px;color:#666;font-size:14px'>";
    html += "<p>API Endpoints:</p>";
    html += "<p>GET /api/files - List all photos</p>";
    html += "<p>POST /api/files - Upload photo</p>";
    html += "<p>DELETE /api/files?filename=... - Delete photo</p>";
    html += "</div>";
    
    html += "</div>"; // End container
    
    // JavaScript for OTA progress
    html += "<script>";
    html += "document.getElementById('uploadForm').onsubmit = function(e) {";
    html += "e.preventDefault();";
    html += "var fileInput = document.getElementById('fileInput');";
    html += "if(fileInput.files.length === 0) {";
    html += "alert('Please select a firmware file first!');";
    html += "return false;";
    html += "}";
    
    html += "var formData = new FormData();";
    html += "formData.append('update', fileInput.files[0]);";
    
    html += "var xhr = new XMLHttpRequest();";
    html += "document.getElementById('progressContainer').style.display = 'block';";
    html += "document.getElementById('submitBtn').disabled = true;";
    html += "document.getElementById('submitBtn').value = 'Uploading...';";
    
    html += "xhr.upload.onprogress = function(e) {";
    html += "if (e.lengthComputable) {";
    html += "var percent = Math.round((e.loaded / e.total) * 100);";
    html += "document.getElementById('progressBar').style.width = percent + '%';";
    html += "document.getElementById('progressText').textContent = 'Uploading: ' + percent + '%';";
    html += "}";
    html += "};";
    
    html += "xhr.onload = function() {";
    html += "if (xhr.status === 200) {";
    html += "document.getElementById('progressText').textContent = '✅ Update complete! Rebooting...';";
    html += "document.getElementById('progressText').style.color = '#4CAF50';";
    html += "setTimeout(function() { location.reload(); }, 3000);";
    html += "} else {";
    html += "document.getElementById('progressText').textContent = '❌ Update failed!';";
    html += "document.getElementById('progressText').style.color = '#f44336';";
    html += "document.getElementById('submitBtn').disabled = false;";
    html += "document.getElementById('submitBtn').value = 'Upload Firmware';";
    html += "}";
    html += "};";
    
    html += "xhr.open('POST', '/update');";
    html += "xhr.send(formData);";
    html += "return false;";
    html += "};";
    html += "</script>";
    
    html += "</body></html>";
    
    server.send(200, "text/html", html);
}

void handleOTA() {
    HTTPUpload& upload = server.upload();
    
    if (upload.status == UPLOAD_FILE_START) {
        Serial.printf("OTA Update Started: %s\n", upload.filename.c_str());
        Serial.setDebugOutput(true);
        otaInProgress = true;
        otaTotalSize = upload.totalSize;
        otaCurrentSize = 0;
        
        // Start the update
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Update.printError(Serial);
        }
    } 
    else if (upload.status == UPLOAD_FILE_WRITE) {
        // Write the received data
        size_t written = Update.write(upload.buf, upload.currentSize);
        if (written != upload.currentSize) {
            Update.printError(Serial);
        }
        otaCurrentSize += upload.currentSize;
        
        // Show progress
        if (otaTotalSize > 0) {
            int progress = (otaCurrentSize * 100) / otaTotalSize;
            Serial.printf("OTA Progress: %d%%\n", progress);
        }
    } 
    else if (upload.status == UPLOAD_FILE_END) {
        // Finalize the update
        if (Update.end(true)) {
            Serial.printf("OTA Update Success: %u bytes\n", upload.totalSize);
            Serial.println("Rebooting...");
            
            // Send success response
            server.sendHeader("Location", "/");
            server.send(303);
            
            // Delay and reboot
            delay(1000);
            ESP.restart();
        } else {
            Update.printError(Serial);
            server.send(500, "text/plain", "Update FAILED");
        }
        otaInProgress = false;
        Serial.setDebugOutput(false);
    } 
    else if (upload.status == UPLOAD_FILE_ABORTED) {
        Serial.println("OTA Update was aborted");
        Update.end();
        otaInProgress = false;
        server.send(500, "text/plain", "Update aborted");
    }
}

void initWebServer() {
    server.on("/", HTTP_GET, handleRoot);
    server.on("/update", HTTP_POST, 
        []() { server.send(200, "text/plain", "OK"); },
        handleOTA
    );
    
    // API endpoints for file management
    server.on("/api/files", HTTP_GET, handleAPIFiles);
    server.on("/api/files", HTTP_POST, 
        []() { server.send(200, "application/json", "{\"status\":\"success\"}"); },
        handleAPIFiles
    );
    server.on("/api/files", HTTP_DELETE, handleAPIFiles);
    
    server.begin();
    Serial.println("HTTP server started");
    Serial.print("Web interface: http://");
    Serial.println(WiFi.localIP());
    Serial.println("API: GET/POST/DELETE /api/files");
}

// ==================== Setup ====================
void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n" + String(60, '='));
    Serial.println("ESP32 Photo Frame - Enhanced Version");
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
    
    // Initialize Wi-Fi
    updateLoadingProgress(0.3, "Connecting to Wi-Fi...");
    initWiFi();
    
    // Try to initialize SD card
    bool sdInitialized = initSDCard();
    
    if (sdInitialized) {
        // Find images
        findImageFiles();
        
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
            
            // Format interval for display
            String intervalStr;
            if (slideshowInterval < 60000) {
                intervalStr = String(slideshowInterval / 1000) + " seconds";
            } else {
                intervalStr = String(slideshowInterval / 60000) + " minutes";
            }
            Serial.printf("Interval: %s\n", intervalStr.c_str());
            Serial.printf("Brightness: %d/255\n", currentBrightness);
            
            if (wifiConnected) {
                Serial.print("Web interface: http://");
                Serial.println(WiFi.localIP());
            }
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
    
    // Initialize web server if Wi-Fi connected (even if SD card failed)
    if (wifiConnected) {
        updateLoadingProgress(0.9, "Starting web server...");
        initWebServer();
        updateLoadingProgress(1.0, "Ready!");
        delay(500);
        
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
            
            gfx.setCursor(50, 400);
            gfx.setTextColor(CYAN);
            gfx.print("Web: ");
            gfx.print(WiFi.localIP().toString());
            
            gfx.setCursor(100, 450);
            gfx.setTextSize(1);
            gfx.setTextColor(YELLOW);
            gfx.print("Press button for menu");
        }
    } else if (fatalError) {
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
        gfx.print("Check SD card and restart");
    }
    
    // Hide loading screen if still showing
    if (showingLoading) {
        hideLoadingScreen();
    }
}

// ==================== Loop ====================
void loop() {
    processButtonInput();
    
    // Handle web server
    if (wifiConnected) {
        checkWiFiConnection();
        server.handleClient();
    }
    
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