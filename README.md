# Karting-Cam ESP32 Project

ESP32-CAM project that records a short MJPEG video and serves files via a web
interface.

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

- Records a 10&nbsp;second MJPEG video on boot
- Stores the video on the SD card
- Web interface to view and download files
- Can operate as WiFi Access Point or connect to existing network
