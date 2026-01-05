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
- **File Management**: Upload/delete images through web interface
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
   - Adjust display pins if needed

4. **Upload firmware**
   - Select ESP32 board in Arduino IDE
   - Upload the sketch

5. **Prepare SD card**
   - Format SD card as FAT32
   - Add JPG images to root directory
   - Insert into SD card module

## Usage

- **Short press**: Open menu / Select option
- **Long press**: Change interval / Navigate menu
- **Menu auto-close**: Returns to slideshow after inactivity

## Web Interface

Once connected to Wi-Fi:
- Access via `http://[ESP32_IP]`
- Upload/delete images
- Monitor system status
- Perform OTA updates

## API Endpoints

- `GET /api/files` - List all images
- `POST /api/files` - Upload image
- `DELETE /api/files` - Delete image
- `GET /api/download` - Download image

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

# Resolving the Build Error

Add board to PlatformIO https://github.com/rzeldent/platformio-espressif32-sunton

## Screenshot

![Application Screenshot](./3d_model/screenshot1.jpg)
![Application Screenshot](./3d_model/screenshot2.jpg)
