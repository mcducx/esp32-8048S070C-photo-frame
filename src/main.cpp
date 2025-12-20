#include "display.h"
#include "config.h"
#include <SD.h>
#include <SPI.h>
#include <TJpg_Decoder.h>
#include <EEPROM.h>
#include <vector>
#include <algorithm>

// ==================== EEPROM Configuration ====================
#define EEPROM_SIZE 64
#define INTERVAL_EEPROM_ADDR 0
#define INTERVAL_DEFAULT_INDEX 2

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
unsigned long lastTouchTime = 0;
const unsigned long TOUCH_DEBOUNCE = 50; // 50 ms debounce delay

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
void saveIntervalToEEPROM();
void loadIntervalFromEEPROM();
void showLoadingScreen(const String& message = "Loading...");
void updateLoadingProgress(float progress, const String& message = "");
void hideLoadingScreen();

// ==================== EEPROM Functions ====================
void saveIntervalToEEPROM() {
    EEPROM.write(INTERVAL_EEPROM_ADDR, currentIntervalIndex);
    if (EEPROM.commit()) {
        Serial.printf("Interval saved to EEPROM: %d (index), %lu ms\n", 
                      currentIntervalIndex, slideshowInterval);
    } else {
        Serial.println("Failed to save interval to EEPROM!");
    }
}

void loadIntervalFromEEPROM() {
    EEPROM.begin(EEPROM_SIZE);
    
    // Read saved interval index from EEPROM
    int savedIndex = EEPROM.read(INTERVAL_EEPROM_ADDR);
    
    // Validate the saved index
    if (savedIndex >= 0 && savedIndex < (sizeof(intervals) / sizeof(intervals[0]))) {
        currentIntervalIndex = savedIndex;
        slideshowInterval = intervals[currentIntervalIndex];
        Serial.printf("Interval loaded from EEPROM: %d (index), %lu ms\n", 
                      currentIntervalIndex, slideshowInterval);
    } else {
        // Invalid or first run - use default
        currentIntervalIndex = INTERVAL_DEFAULT_INDEX;
        slideshowInterval = intervals[currentIntervalIndex];
        Serial.printf("Using default interval: %d (index), %lu ms\n", 
                      currentIntervalIndex, slideshowInterval);
        // Save default to EEPROM
        saveIntervalToEEPROM();
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
    if (millis() - lastUpdate < 100 && progress < 1.0) return;
    lastUpdate = millis();
    
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
            // Update progress bar
            gfx.fillRect(102, 397, barWidth, 16, GREEN);
            
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
    // Temporary message overlaid on the image
    gfx.fillRect(0, 0, 480, 50, BLACK);
    gfx.setCursor(10, 10);
    gfx.setTextSize(2);
    gfx.setTextColor(CYAN);
    gfx.print("Interval: ");
    gfx.print(slideshowInterval / 1000);
    gfx.print(" sec");
    
    delay(2000);
    
    // Redraw the image
    if (!imageFiles.empty()) {
        displayImage(currentImageIndex);
    }
}

// ==================== Change Interval ====================
void changeInterval() {
    currentIntervalIndex = (currentIntervalIndex + 1) % (sizeof(intervals) / sizeof(intervals[0]));
    slideshowInterval = intervals[currentIntervalIndex];
    
    // Save new interval to EEPROM
    saveIntervalToEEPROM();
    
    showIntervalMessage();
    lastImageChange = millis();
}

// ==================== SD Card Initialization ====================
bool initSDCard() {
    Serial.println("Initializing SD card...");
    updateLoadingProgress(0.1, "Initializing SD card...");
    
    sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    delay(100);
    
    if (!SD.begin(SD_CS, sdSPI, 4000000)) {
        Serial.println("SD card initialization failed!");
        return false;
    }
    
    updateLoadingProgress(0.3, "SD card detected");
    
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
    updateLoadingProgress(0.4, "Scanning for images...");
    
    File root = SD.open("/");
    if (!root) {
        Serial.println("Cannot open root directory");
        return;
    }
    
    int totalFiles = 0;
    int imageCount = 0;
    
    while (true) {
        File entry = root.openNextFile();
        if (!entry) break;
        
        String filename = entry.name();
        totalFiles++;
        
        // Update progress every 10 files
        if (totalFiles % 10 == 0) {
            updateLoadingProgress(0.4 + (totalFiles * 0.3 / 1000.0), 
                                 "Scanning... Found " + String(imageCount) + " images");
        }
        
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
            imageCount++;
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
    
    updateLoadingProgress(0.7, "Preparing slideshow...");
    
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
    
    // Check for touch
    if (check_touch(&x, &y)) {
        // Debounce protection
        if (millis() - lastTouchTime > TOUCH_DEBOUNCE) {
            // Normal touch - change interval
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
    
    // Load saved interval from EEPROM
    loadIntervalFromEEPROM();
    
    // Display initialization
    setup_display();
    
    // Set portrait mode
    gfx.setRotation(1);
    ts.setRotation(1);
    
    // Show loading screen immediately
    showLoadingScreen("Starting...");
    delay(500); // Brief pause to show loading screen
    
    // SD card initialization
    if (initSDCard()) {
        updateLoadingProgress(0.5, "SD card ready");
        
        // Find images
        findImageFiles();
        
        if (!imageFiles.empty()) {
            updateLoadingProgress(0.8, String(imageFiles.size()) + " images found");
            
            // Initialize random slideshow order
            initRandomSlideshow();
            
            updateLoadingProgress(1.0, "Ready!");
            delay(500); // Show completion briefly
            
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
    
    // Process touch input
    processTouchInput();
    
    // Automatic slideshow
    if (!imageFiles.empty()) {
        if (millis() - lastImageChange >= slideshowInterval) {
            int nextImageIndex = getNextRandomImage();
            displayImage(nextImageIndex);
        }
    }
    
    delay(10);
}