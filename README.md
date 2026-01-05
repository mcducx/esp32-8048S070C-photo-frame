# ESP32 Photo Frame

A feature-rich digital photo frame based on ESP32 with touch display, SD card support, Wi-Fi connectivity, and OTA updates.
Purchase link: https://www.aliexpress.com/item/1005008069223989.html?spm=a2g0o.tesla.0.0.5146iGumiGumwB


## Features

- **Slideshow Mode**: Automatic image rotation with adjustable intervals (5s, 30s, 1m, 5m, 15m, 30m, 60m)
- **Random Play**: Images are shuffled for varied viewing
- **Brightness Control**: Adjustable backlight brightness (20-255)
- **Physical Controls**: Button for menu navigation and settings
- **Wi-Fi Connectivity**: Built-in web server for remote management
- **OTA Updates**: Firmware updates via web interface
- **API Support**: REST API for file management
- **System Info**: Display device status and storage information

## Hardware Requirements

- ESP32 development board
- RGB display (480x800 resolution)
- SD card module
- Boot button for navigation
- SD card with JPG images

## Pin Configuration

- Display: Custom RGB panel pins (see display.h)
- SD Card: SCK=12, MISO=13, MOSI=11, CS=10
- Button: GPIO0 (BOOT button)

## Quick Start

1. **Clone the repository**
   ```bash
   git clone https://github.com/yourusername/esp32-photo-frame.git
   ```

2. **Install required libraries**
   - Arduino_GFX_Library
   - TJpg_Decoder
   - ArduinoJson
   - ESP32WebServer

3. **Configure settings**
   - Update Wi-Fi credentials in `config.h`

4. **Upload firmware**
   - Add board to PlatformIO https://github.com/rzeldent/platformio-espressif32-sunton
   - Build and Upload the sketch

5. **Prepare SD card**
   - Format SD card as FAT32
   - Add JPG images to root directory
   - Insert into SD card moduleFuture Features

## Usage

- **Short press**: Open menu / Select option
- **Long press**: Change interval / Navigate menu
- **Menu auto-close**: Returns to slideshow after inactivity

## Web Interface

Once connected to Wi-Fi:
- Access via `http://[ESP32_IP]`
- Monitor system status
- Perform OTA updates

## Prepare the SD Card:

- Format to FAT32
- Use my converter: (https://github.com/mcducx/imageflow/tree/main)
- Add JPEG files to the root directory
- Optimal image size: 480×800 pixels

## API Endpoints Documentation

### Current API Endpoints

#### 1. **List Files**
**Endpoint:** `GET /api/files`

**Description:** Returns a JSON list of all image files on the SD card.

**Response Format:**
```json
{
  "status": "success",
  "files": ["image1.jpg", "image2.jpg", ...],
  "count": 15
}
```

**Example:**
```bash
curl -X GET http://192.168.1.100/api/files
```

---

#### 2. **Upload File**
**Endpoint:** `POST /api/files`

**Description:** Upload a new image file to the SD card. Supports multipart form-data.

**Parameters:**
- `file` (binary): The image file to upload (JPG format recommended)

**Response Format:**
```json
{
  "status": "success",
  "message": "File uploaded successfully",
  "filename": "uploaded-image.jpg",
  "size": 123456
}
```

**Example:**
```bash
curl -X POST \
  -F "file=@/path/to/image.jpg" \
  http://192.168.1.100/api/files
```

---

#### 3. **Delete File**
**Endpoint:** `DELETE /api/files`

**Description:** Delete a specific file from the SD card.

**Parameters:**
- `filename` (string): Name of the file to delete

**Response Format:**
```json
{
  "status": "success",
  "message": "File deleted successfully",
  "filename": "deleted-image.jpg"
}
```

**Example:**
```bash
curl -X DELETE \
  "http://192.168.1.100/api/files?filename=image.jpg"
```

---

#### 4. **Download File**
**Endpoint:** `GET /api/download`

**Description:** Download a specific file from the SD card.

**Parameters:**
- `filename` (string): Name of the file to download

**Response:** Binary file with appropriate Content-Type header

**Example:**
```bash
curl -O -J "http://192.168.1.100/api/download?filename=image.jpg"
```

**CURL example with output:**
```bash
curl -X GET "http://192.168.1.100/api/download?filename=image.jpg" --output downloaded-image.jpg
```

---

## Configuration

Edit `config.h` to customize:
- Wi-Fi credentials
- OTA settings
- Display parameters
- Button timing

## 📁 Project Structure

```
src/
├── main.cpp          # Main slideshow logic
├── display.cpp       # Display driver
├── display.h         # Display header file
└── config.h          # Pin configuration
platformio.ini        # PlatformIO configuration
```

# Future Features
## ImageFlow Converter Integration

- I plan to integrate ImageFlow for automatic optimization, image conversion and batch upload via REST API.


## Screenshot

![Application Screenshot](./3d_model/screenshot1.jpg)
![Application Screenshot](./3d_model/screenshot2.jpg)
