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

// Slideshow intervals
const unsigned long intervals[] = {3000, 5000, 10000, 15000, 30000, 60000, 120000};
int currentIntervalIndex = INTERVAL_DEFAULT_INDEX;
unsigned long slideshowInterval = intervals[currentIntervalIndex];

bool fatalError = false;

// Loading screen type
enum LoadingType {
    LOADING_BAR
};
LoadingType currentLoadingType = LOADING_BAR;

// Button state
bool lastButtonState = HIGH;
unsigned long lastButtonPress = 0;
const unsigned long BUTTON_COOLDOWN = 300;

// Message display
unsigned long messageStartTime = 0;
bool showingMessage = false;
const unsigned long MESSAGE_DURATION = 1000;

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
int countTotalFiles();
void findImageFiles();
void initRandomSlideshow();
int getNextRandomImage();

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

// ==================== Loading Screen Functions ====================
void showLoadingScreen(const String& message) {
    gfx.fillScreen(BLACK);
    
    gfx.setCursor(140, 300);
    gfx.setTextSize(3);
    gfx.setTextColor(CYAN);
    gfx.print("Photo Frame");
    
    gfx.setCursor(150, 350);
    gfx.setTextSize(2);
    gfx.setTextColor(WHITE);
    gfx.print(message);
    
    gfx.drawRect(100, 395, 280, 20, WHITE);
}

void updateLoadingProgress(float progress, const String& message) {
    static unsigned long lastUpdate = 0;
    
    if (millis() - lastUpdate < 50 && progress < 1.0) return;
    lastUpdate = millis();
    
    if (progress < 0.0) progress = 0.0;
    if (progress > 1.0) progress = 1.0;
    
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
    gfx.fillRect(0, 0, 480, 50, BLACK);
    gfx.setCursor(10, 10);
    gfx.setTextSize(2);
    gfx.setTextColor(CYAN);
    gfx.print("Interval: ");
    gfx.print(slideshowInterval / 1000);
    gfx.print(" sec");
    
    showingMessage = true;
    messageStartTime = millis();
}

// ==================== Hide Message ====================
void hideMessage() {
    if (showingMessage) {
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
    
    saveIntervalToSD();
    showIntervalMessage();
    lastImageChange = millis();
    
    Serial.printf("Interval changed to: %lu ms\n", slideshowInterval);
}

// ==================== Process Button Input ====================
void processButtonInput() {
    int currentButtonState = digitalRead(BOOT_BUTTON_PIN);
    unsigned long now = millis();
    
    if (currentButtonState == LOW && lastButtonState == HIGH) {
        delay(10);
        if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
            if (now - lastButtonPress > BUTTON_COOLDOWN) {
                lastButtonPress = now;
                changeInterval();
                Serial.println("Button pressed - interval changed");
            }
        }
    }
    
    lastButtonState = currentButtonState;
}

// ==================== SD Card Initialization ====================
bool initSDCard() {
    Serial.println("Initializing SD card...");
    updateLoadingProgress(0.0, "Initializing SD card...");
    
    // Используем пины из display.h
    sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    delay(100);
    
    if (!SD.begin(SD_CS, sdSPI, 2000000)) {  // Reduced speed for stability
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
    
    updateLoadingProgress(0.2, "Counting files...");
    int totalFiles = countTotalFiles();
    
    Serial.printf("Total non-system files to scan: %d\n", totalFiles);
    
    if (totalFiles == 0) {
        updateLoadingProgress(1.0, "No files found");
        return;
    }
    
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
        
        float progress = 0.2 + ((float)processedFiles / totalFiles) * 0.8;
        
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
    
    // Initialize display
    Serial.println("Initializing display...");
    setup_display();
    
    // Set portrait mode
    gfx.setRotation(1);
    
    // Initialize JPG decoder
    // TJpgDec.setSwapBytes(true); // Убрали эту строку, чтобы не менять порядок байтов
    TJpgDec.setCallback(tft_output);
    
    // Show loading screen
    showLoadingScreen("Starting...");
    delay(300);
    
    // SD card initialization
    if (initSDCard()) {
        findImageFiles();
        
        if (!imageFiles.empty()) {
            initRandomSlideshow();
            
            delay(300);
            hideLoadingScreen();
            
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
            fatalError = true;
            hideLoadingScreen();
            displayErrorScreen("NO IMAGES", "Add JPG files to SD card");
            Serial.println("\nERROR: No JPEG images found on SD card!");
        }
    } else {
        fatalError = true;
        hideLoadingScreen();
        displayErrorScreen("SD CARD ERROR", "Insert card with images");
        Serial.println("\nERROR: SD card initialization failed!");
    }
}
// ==================== Loop ====================
void loop() {
    if (fatalError) {
        delay(100);
        return;
    }
    
    processButtonInput();
    
    if (showingMessage && (millis() - messageStartTime >= MESSAGE_DURATION)) {
        hideMessage();
    }
    
    if (!imageFiles.empty() && !showingMessage) {
        if (millis() - lastImageChange >= slideshowInterval) {
            int nextImageIndex = getNextRandomImage();
            displayImage(nextImageIndex);
        }
    }
    
    delay(10);
}