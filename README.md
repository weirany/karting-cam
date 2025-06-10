# Karting-Cam ESP32 Project

ESP32-CAM project that captures 5 seconds of video as individual JPEG frames and serves files via a web interface.

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

### Time Synchronization

`config.h` also contains `TZ_INFO` and NTP server settings. Set `TZ_INFO` to a
POSIX TZ string such as `"EST5EDT,M3.2.0/2,M11.1.0/2"` for automatic daylight
saving adjustments. The clock is synchronized at boot whenever WiFi and Internet
access are available.

### Build and Upload

Upload the `karting-cam.ino` file to your ESP32-CAM using the Arduino IDE or PlatformIO.

## Features

- Captures 5 seconds of video as individual JPEG frames on boot
- Saves frames in a timestamped folder (e.g. `/20240101_120000/`) as
  `frame_000001.jpg`, `frame_000002.jpg`, etc. on the SD card
- Web interface to browse directories and download files
- Can operate as WiFi Access Point or connect to existing network
- Automatically syncs the on-board RTC with NTP when WiFi has internet access
