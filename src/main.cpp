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

// Slideshow intervals
const unsigned long intervals[] = {3000, 5000, 10000, 15000, 30000, 60000, 120000};
int currentIntervalIndex = INTERVAL_DEFAULT_INDEX;
unsigned long slideshowInterval = intervals[currentIntervalIndex];

bool fatalError = false;

// Button state
bool lastButtonState = HIGH;
unsigned long lastButtonPress = 0;
const unsigned long BUTTON_COOLDOWN = 300;

// Message display
unsigned long messageStartTime = 0;
bool showingMessage = false;
const unsigned long MESSAGE_DURATION = 1000;

// Wi-Fi & OTA
bool wifiConnected = false;
bool otaEnabled = true;
WebServer server(WEB_PORT);
String wifiStatusMessage = "";
unsigned long lastWifiCheck = 0;
const unsigned long WIFI_CHECK_INTERVAL = 30000;

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

// Wi-Fi & OTA Functions
void initWiFi();
void checkWiFiConnection();
void initWebServer();
void handleRoot();
void handleConfig();
void handleFileList();
void handleFileUpload();
void handleDeleteFile();
void handleOTAUpdate();
void handleOTA();
void handleGetStatus();
void showWiFiMessage(const String& message);
String formatBytes(size_t bytes);
String getContentType(String filename);

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

// ==================== Wi-Fi Functions ====================
void initWiFi() {
    Serial.println("Initializing Wi-Fi...");
    updateLoadingProgress(0.05, "Connecting to Wi-Fi...");
    
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(OTA_HOSTNAME);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        wifiStatusMessage = "Wi-Fi Connected! IP: " + WiFi.localIP().toString();
        Serial.println("\nWi-Fi connected!");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        
        // Initialize mDNS
        if (!MDNS.begin(OTA_HOSTNAME)) {
            Serial.println("Error setting up MDNS responder!");
        } else {
            Serial.println("mDNS responder started");
            MDNS.addService("http", "tcp", 80);
        }
    } else {
        wifiStatusMessage = "Wi-Fi Failed! Check config.h";
        Serial.println("\nWi-Fi connection failed!");
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
                wifiStatusMessage = "Wi-Fi Reconnected! IP: " + WiFi.localIP().toString();
                Serial.println("Wi-Fi reconnected!");
            }
        }
    }
}

// ==================== Web Server Handlers ====================
void handleRoot() {
    String html = R"rawliteral(
    <!DOCTYPE html>
    <html>
    <head>
        <title>ESP32 Photo Frame Control</title>
        <meta name="viewport" content="width=device-width, initial-scale=1">
        <style>
            body { font-family: Arial, sans-serif; margin: 20px; background: #f0f0f0; }
            .container { max-width: 800px; margin: auto; background: white; padding: 20px; border-radius: 10px; }
            .card { background: #f9f9f9; padding: 15px; margin: 10px 0; border-radius: 5px; }
            .status { color: #4CAF50; font-weight: bold; }
            .error { color: #f44336; }
            input[type=file] { margin: 10px 0; }
            button { background: #4CAF50; color: white; border: none; padding: 10px 20px; border-radius: 5px; cursor: pointer; }
            button:hover { background: #45a049; }
            .tab { overflow: hidden; border: 1px solid #ccc; background-color: #f1f1f1; }
            .tab button { background-color: inherit; float: left; border: none; outline: none; cursor: pointer; padding: 14px 16px; transition: 0.3s; }
            .tab button:hover { background-color: #ddd; }
            .tab button.active { background-color: #ccc; }
            .tabcontent { display: none; padding: 6px 12px; border: 1px solid #ccc; border-top: none; }
        </style>
        <script>
            function showTab(tabName) {
                var i, tabcontent, tablinks;
                tabcontent = document.getElementsByClassName("tabcontent");
                for (i = 0; i < tabcontent.length; i++) {
                    tabcontent[i].style.display = "none";
                }
                tablinks = document.getElementsByClassName("tablink");
                for (i = 0; i < tablinks.length; i++) {
                    tablinks[i].className = tablinks[i].className.replace(" active", "");
                }
                document.getElementById(tabName).style.display = "block";
                event.currentTarget.className += " active";
            }
            
            function updateStatus() {
                fetch('/status')
                    .then(response => response.json())
                    .then(data => {
                        document.getElementById('wifiStatus').innerHTML = 
                            data.wifi_connected ? 
                            `<span class="status">Connected (${data.ip})</span>` : 
                            `<span class="error">Disconnected</span>`;
                        document.getElementById('currentImage').textContent = data.current_image;
                        document.getElementById('totalImages').textContent = data.total_images;
                        document.getElementById('interval').textContent = data.interval;
                        document.getElementById('freeHeap').textContent = data.free_heap;
                    });
            }
            
            function uploadFile() {
                var formData = new FormData();
                var fileInput = document.getElementById('fileInput');
                var progressBar = document.getElementById('progressBar');
                var progressText = document.getElementById('progressText');
                
                if(fileInput.files.length === 0) {
                    alert("Please select a file first!");
                    return;
                }
                
                formData.append("file", fileInput.files[0]);
                
                var xhr = new XMLHttpRequest();
                xhr.open("POST", "/upload", true);
                
                xhr.upload.onprogress = function(e) {
                    if (e.lengthComputable) {
                        var percentComplete = (e.loaded / e.total) * 100;
                        progressBar.style.width = percentComplete + '%';
                        progressText.textContent = Math.round(percentComplete) + '%';
                    }
                };
                
                xhr.onload = function() {
                    if (xhr.status === 200) {
                        alert("File uploaded successfully!");
                        progressBar.style.width = '0%';
                        progressText.textContent = '';
                        fileInput.value = '';
                        loadFileList();
                    } else {
                        alert("Upload failed: " + xhr.responseText);
                    }
                };
                
                xhr.send(formData);
            }
            
            function loadFileList() {
                fetch('/files')
                    .then(response => response.json())
                    .then(data => {
                        var fileList = document.getElementById('fileList');
                        fileList.innerHTML = '';
                        data.files.forEach(function(file) {
                            var li = document.createElement('li');
                            li.innerHTML = `${file.name} (${file.size}) 
                                <button onclick="deleteFile('${file.name}')">Delete</button>`;
                            fileList.appendChild(li);
                        });
                    });
            }
            
            function deleteFile(filename) {
                if(confirm("Are you sure you want to delete " + filename + "?")) {
                    fetch('/delete?file=' + encodeURIComponent(filename), {method: 'DELETE'})
                        .then(response => {
                            if(response.ok) {
                                alert("File deleted!");
                                loadFileList();
                            } else {
                                alert("Delete failed!");
                            }
                        });
                }
            }
            
            function setIntervalValue() {
                var interval = document.getElementById('intervalInput').value;
                fetch('/config?interval=' + interval)
                    .then(response => {
                        if(response.ok) {
                            alert("Interval updated!");
                            updateStatus();
                        }
                    });
            }
            
            window.onload = function() {
                showTab('status');
                updateStatus();
                loadFileList();
                setInterval(updateStatus, 5000);
            };
        </script>
    </head>
    <body>
        <div class="container">
            <h1>ESP32 Photo Frame Control Panel</h1>
            
            <div class="tab">
                <button class="tablink" onclick="showTab('status')">Status</button>
                <button class="tablink" onclick="showTab('files')">File Management</button>
                <button class="tablink" onclick="showTab('upload')">Upload</button>
                <button class="tablink" onclick="showTab('config')">Configuration</button>
                <button class="tablink" onclick="showTab('ota')">OTA Update</button>
            </div>
            
            <div id="status" class="tabcontent">
                <div class="card">
                    <h3>System Status</h3>
                    <p>Wi-Fi Status: <span id="wifiStatus"></span></p>
                    <p>Current Image: <span id="currentImage"></span></p>
                    <p>Total Images: <span id="totalImages"></span></p>
                    <p>Interval: <span id="interval"></span> seconds</p>
                    <p>Free Memory: <span id="freeHeap"></span> bytes</p>
                </div>
            </div>
            
            <div id="files" class="tabcontent">
                <div class="card">
                    <h3>Files on SD Card</h3>
                    <ul id="fileList"></ul>
                </div>
            </div>
            
            <div id="upload" class="tabcontent">
                <div class="card">
                    <h3>Upload New Image</h3>
                    <input type="file" id="fileInput" accept=".jpg,.jpeg">
                    <button onclick="uploadFile()">Upload</button>
                    <div style="margin-top: 10px;">
                        <div style="width: 100%; background: #ddd; border-radius: 5px;">
                            <div id="progressBar" style="width: 0%; height: 20px; background: #4CAF50; border-radius: 5px;"></div>
                        </div>
                        <div id="progressText"></div>
                    </div>
                </div>
            </div>
            
            <div id="config" class="tabcontent">
                <div class="card">
                    <h3>Configuration</h3>
                    <p>Slideshow Interval (seconds):</p>
                    <input type="number" id="intervalInput" min="3" max="120" value="10">
                    <button onclick="setIntervalValue()">Set Interval</button>
                </div>
            </div>
            
            <div id="ota" class="tabcontent">
                <div class="card">
                    <h3>OTA Firmware Update</h3>
                    <p>Upload new firmware (.bin file):</p>
                    <form method="POST" action="/update" enctype="multipart/form-data">
                        <input type="file" name="update">
                        <input type="submit" value="Update">
                    </form>
                </div>
            </div>
        </div>
    </body>
    </html>
    )rawliteral";
    
    server.send(200, "text/html", html);
}

void handleConfig() {
    if (server.hasArg("interval")) {
        int intervalSec = server.arg("interval").toInt();
        // Find closest interval
        for (int i = 0; i < sizeof(intervals)/sizeof(intervals[0]); i++) {
            if (intervals[i] / 1000 == intervalSec) {
                currentIntervalIndex = i;
                slideshowInterval = intervals[currentIntervalIndex];
                saveIntervalToSD();
                showIntervalMessage();
                break;
            }
        }
    }
    server.send(200, "text/plain", "OK");
}

void handleFileList() {
    DynamicJsonDocument doc(4096);
    JsonArray files = doc.createNestedArray("files");
    
    File root = SD.open("/");
    while (true) {
        File entry = root.openNextFile();
        if (!entry) break;
        
        if (!entry.isDirectory()) {
            String filename = entry.name();
            String ext = filename.substring(filename.lastIndexOf('.'));
            ext.toLowerCase();
            
            if (ext == ".jpg" || ext == ".jpeg") {
                JsonObject file = files.createNestedObject();
                file["name"] = filename;
                file["size"] = formatBytes(entry.size());
            }
        }
        entry.close();
    }
    root.close();
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleFileUpload() {
    static File uploadFile;  // Статическая переменная для файла
    
    HTTPUpload& upload = server.upload();
    
    if (upload.status == UPLOAD_FILE_START) {
        String filename = "/" + upload.filename;
        Serial.printf("Upload start: %s\n", filename.c_str());
        
        // Закрыть предыдущий файл, если открыт
        if (uploadFile) {
            uploadFile.close();
        }
        
        SD.remove(filename);
        uploadFile = SD.open(filename, FILE_WRITE);
        if (!uploadFile) {
            Serial.println("Failed to open file for writing");
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (uploadFile) {
            uploadFile.write(upload.buf, upload.currentSize);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (uploadFile) {
            uploadFile.close();
            Serial.printf("Upload complete: %s, Size: %u\n", 
                         upload.filename.c_str(), upload.totalSize);
            
            // Refresh image list
            findImageFiles();
            initRandomSlideshow();
        }
    }
}

void handleDeleteFile() {
    if (server.hasArg("file")) {
        String filename = "/" + server.arg("file");
        if (SD.exists(filename)) {
            SD.remove(filename);
            Serial.printf("Deleted file: %s\n", filename.c_str());
            
            // Refresh image list
            findImageFiles();
            initRandomSlideshow();
            
            server.send(200, "text/plain", "File deleted");
        } else {
            server.send(404, "text/plain", "File not found");
        }
    } else {
        server.send(400, "text/plain", "Missing file parameter");
    }
}

void handleOTAUpdate() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", 
        "<!DOCTYPE html><html><head><title>OTA Update</title></head><body>"
        "<h1>OTA Update</h1>"
        "<form method='POST' action='/ota' enctype='multipart/form-data'>"
        "<input type='file' name='update'>"
        "<input type='submit' value='Update'>"
        "</form>"
        "</body></html>");
}

void handleOTA() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        Serial.printf("Update: %s\n", upload.filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
            Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
            server.sendHeader("Location", "/");
            server.send(303);
            delay(1000);
            ESP.restart();
        } else {
            Update.printError(Serial);
        }
    }
}

void handleGetStatus() {
    DynamicJsonDocument doc(512);
    doc["wifi_connected"] = wifiConnected;
    doc["ip"] = wifiConnected ? WiFi.localIP().toString() : "N/A";
    doc["current_image"] = imageFiles.empty() ? "None" : 
        String(currentImageIndex + 1) + "/" + String(imageFiles.size());
    doc["total_images"] = imageFiles.size();
    doc["interval"] = String(slideshowInterval / 1000) + "s";
    doc["free_heap"] = ESP.getFreeHeap();
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void initWebServer() {
    server.on("/", HTTP_GET, handleRoot);
    server.on("/config", HTTP_GET, handleConfig);
    server.on("/files", HTTP_GET, handleFileList);
    server.on("/status", HTTP_GET, handleGetStatus);
    server.on("/upload", HTTP_POST, []() {
        server.send(200, "text/plain", "Upload complete");
    }, handleFileUpload);
    server.on("/delete", HTTP_DELETE, handleDeleteFile);
    server.on("/update", HTTP_GET, handleOTAUpdate);
    server.on("/ota", HTTP_POST, []() {
        server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
        ESP.restart();
    }, handleOTA);
    
    server.begin();
    Serial.println("HTTP server started");
}

// ==================== Utility Functions ====================
String formatBytes(size_t bytes) {
    if (bytes < 1024) return String(bytes) + " B";
    else if (bytes < (1024 * 1024)) return String(bytes / 1024.0) + " KB";
    else return String(bytes / 1024.0 / 1024.0) + " MB";
}

String getContentType(String filename) {
    if (filename.endsWith(".html")) return "text/html";
    else if (filename.endsWith(".css")) return "text/css";
    else if (filename.endsWith(".js")) return "application/javascript";
    else if (filename.endsWith(".jpg") || filename.endsWith(".jpeg")) return "image/jpeg";
    else if (filename.endsWith(".png")) return "image/png";
    else if (filename.endsWith(".gif")) return "image/gif";
    else if (filename.endsWith(".ico")) return "image/x-icon";
    else if (filename.endsWith(".xml")) return "text/xml";
    else if (filename.endsWith(".pdf")) return "application/pdf";
    else if (filename.endsWith(".zip")) return "application/zip";
    else if (filename.endsWith(".gz")) return "application/x-gzip";
    return "text/plain";
}

void showWiFiMessage(const String& message) {
    gfx.fillRect(0, 750, 480, 50, BLACK);
    gfx.setCursor(10, 760);
    gfx.setTextSize(1);
    gfx.setTextColor(CYAN);
    gfx.print(message);
    
    showingMessage = true;
    messageStartTime = millis();
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
    
    gfx.fillRect(102, 397, 260, 16, BLACK);
    gfx.fillRect(102, 397, barWidth, 16, GREEN);
    gfx.drawRect(100, 395, 280, 20, WHITE);
    
    if (barWidth > 40) {
        gfx.setCursor(102 + barWidth/2 - 15, 398);
        gfx.setTextSize(1);
        gfx.setTextColor(BLACK);
        gfx.printf("%.0f%%", progress * 100);
    }
    
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
    
    sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    delay(100);
    
    if (!SD.begin(SD_CS, sdSPI, 40000000)) {
        if (!SD.begin(SD_CS, sdSPI, 20000000)) {
            Serial.println("SD card initialization failed!");
            return false;
        }
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
    
    // Show Wi-Fi status at bottom if connected
    if (wifiConnected) {
        gfx.fillRect(0, 750, 480, 50, BLACK);
        gfx.setCursor(10, 760);
        gfx.setTextSize(1);
        gfx.setTextColor(CYAN);
        gfx.print("Wi-Fi: ");
        gfx.print(WiFi.localIP().toString());
    }
    
    lastImageChange = millis();
}

// ==================== Setup ====================
void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n" + String(60, '='));
    Serial.println("ESP32 Photo Frame - Wi-Fi + OTA Enabled");
    Serial.println("Press BOOT button to change interval");
    Serial.println("Interval saved to SD card (interval.txt)");
    Serial.println(String(60, '='));
    
    randomSeed(micros());
    
    // Initialize BOOT button
    pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
    
    // Initialize display
    setup_display();
    
    // Set portrait mode
    gfx.setRotation(1);
    
    // Initialize JPG decoder
    TJpgDec.setCallback(tft_output);
    
    // Show loading screen
    showLoadingScreen("Starting...");
    delay(300);
    
    // Initialize Wi-Fi
    initWiFi();
    
    // SD card initialization
    if (initSDCard()) {
        findImageFiles();
        
        if (!imageFiles.empty()) {
            initRandomSlideshow();
            
            // Initialize web server if Wi-Fi is connected
            if (wifiConnected) {
                initWebServer();
                updateLoadingProgress(0.95, "Web server started");
                delay(300);
            }
            
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
            
            if (wifiConnected) {
                Serial.print("Web interface: http://");
                Serial.print(WiFi.localIP());
                Serial.println("/");
                Serial.print("mDNS: http://");
                Serial.print(OTA_HOSTNAME);
                Serial.println(".local/");
            }
            
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
    
    // Check Wi-Fi connection periodically
    if (wifiConnected) {
        checkWiFiConnection();
        server.handleClient();
        // MDNS.update();  // Удалено - не требуется для ESP32
    }
    
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