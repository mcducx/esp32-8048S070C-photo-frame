# ESP32 Portrait Photo Frame

A simple digital photo frame based on ESP32 with an 800×480 display, operating in portrait mode (480×800) with automatic slideshow.
Purchase link: https://www.aliexpress.com/item/1005008069223989.html?spm=a2g0o.tesla.0.0.5146iGumiGumwB

## ✨ Key Features

- **Automatic Slideshow**: The image change timer can be changed by clicking the boot button.
- **Image Centering**: All photos are always centered on the screen
- **Minimalist Interface**: No unnecessary elements, only images
- **SD Card Support**: Read JPEG files from a memory card

## 📋 Requirements

### Hardware
- ESP32-8048S070C board
- 800×480 display with RGB interface
- GT911 touchscreen (optional, not used in current version)
- SD card (FAT32 format)

### Software
- PlatformIO
- Arduino framework
- Libraries (specified in `platformio.ini`)

## ⚙️ Setup

1. **Prepare the SD Card**:
   - Format to FAT32
   - Use my converter: (https://github.com/mcducx/imageflow/tree/main)
   - Add JPEG files to the root directory
   - Optimal image size: 480×800 pixels

## 🚀 Build and Upload

1. Clone the repository
2. Open the project in PlatformIO
3. Connect the ESP32 to your computer
4. Execute:
```bash
pio run --target upload
```
5. Insert the SD card with images
6. Reset the device

## 📁 Project Structure

```
src/
├── main.cpp          # Main slideshow logic
├── display.cpp       # Display driver
├── display.h         # Display header file
└── config.h          # Pin configuration
platformio.ini        # PlatformIO configuration
```

## 🙏 Acknowledgments

Project based on Arduino_GFX

# Resolving the Build Error

Add board to PlatformIO https://github.com/rzeldent/platformio-espressif32-sunton

## Screenshot

![Application Screenshot](./3d_model/screenshot1.jpg)
![Application Screenshot](./3d_model/screenshot2.jpg)
