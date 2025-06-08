# Karting-Cam ESP32 Project

ESP32-CAM project that captures JPEG images and serves them via a web interface.

## Setup

### WiFi Configuration

1. Copy the example config file:
   ```bash
   cp config.h.example config.h
   ```

2. Edit `config.h` with your WiFi credentials:
   ```cpp
   static const char *STA_SSID = "YourWiFiName";
   static const char *STA_PASS = "YourWiFiPassword";
   ```

3. The `config.h` file is excluded from git to keep your credentials private.

### Build and Upload

Upload the `karting-cam.ino` file to your ESP32-CAM using the Arduino IDE or PlatformIO.

## Features

- Captures JPEG images every minute
- Stores images on SD card
- Web interface to view and download images
- Can operate as WiFi Access Point or connect to existing network
