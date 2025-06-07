// ---- force AI-Thinker pin map ----
#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

#include "esp_heap_caps.h" // only needed for the free-PSRAM call
#include <Arduino.h>
#include <SD_MMC.h>
#include <esp_camera.h>

// Global variables for recording
bool isRecording = false;
File videoFile;
unsigned long lastFrameTime = 0;
const unsigned long FRAME_INTERVAL = 100; // 100ms between frames (10 FPS)
int fileCounter = 0;
const size_t MAX_FILE_SIZE = 50 * 1024 * 1024; // 50MB max file size
unsigned long currentFileSize = 0;
unsigned long totalFramesCaptured = 0;
unsigned long failedFrameCaptures = 0;
unsigned long lastStatusPrint = 0;
const unsigned long STATUS_INTERVAL = 10000; // Print status every 10 seconds

// Function to print memory status
void printMemoryStatus() {
  Serial.println("=== Memory Status ===");
  Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());
  Serial.printf("Heap size: %u bytes\n", ESP.getHeapSize());
  Serial.printf("Min free heap: %u bytes\n", ESP.getMinFreeHeap());

  if (psramFound()) {
    Serial.printf("PSRAM total: %u bytes\n", ESP.getPsramSize());
    Serial.printf("PSRAM free: %u bytes\n",
                  heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
  }
  Serial.println("====================");
}

// Function to print SD card info
void printSDCardInfo() {
  Serial.println("=== SD Card Info ===");
  uint8_t cardType = SD_MMC.cardType();

  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }

  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC) {
    Serial.println("MMC");
  } else if (cardType == CARD_SD) {
    Serial.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }

  uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);

  uint64_t totalBytes = SD_MMC.totalBytes();
  uint64_t usedBytes = SD_MMC.usedBytes();
  Serial.printf("Total space: %llu MB\n", totalBytes / (1024 * 1024));
  Serial.printf("Used space: %llu MB\n", usedBytes / (1024 * 1024));
  Serial.printf("Free space: %llu MB\n",
                (totalBytes - usedBytes) / (1024 * 1024));
  Serial.println("===================");
}

// Function to initialize camera
bool initCamera() {
  Serial.println("=== Camera Initialization ===");

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  Serial.println("Pin configuration:");
  Serial.printf("  PWDN: %d, RESET: %d, XCLK: %d\n", PWDN_GPIO_NUM,
                RESET_GPIO_NUM, XCLK_GPIO_NUM);
  Serial.printf("  SIOD: %d, SIOC: %d\n", SIOD_GPIO_NUM, SIOC_GPIO_NUM);
  Serial.printf("  Y2-Y9: %d,%d,%d,%d,%d,%d,%d,%d\n", Y2_GPIO_NUM, Y3_GPIO_NUM,
                Y4_GPIO_NUM, Y5_GPIO_NUM, Y6_GPIO_NUM, Y7_GPIO_NUM, Y8_GPIO_NUM,
                Y9_GPIO_NUM);
  Serial.printf("  VSYNC: %d, HREF: %d, PCLK: %d\n", VSYNC_GPIO_NUM,
                HREF_GPIO_NUM, PCLK_GPIO_NUM);

  // Frame size and quality settings
  if (psramFound()) {
    config.frame_size = FRAMESIZE_VGA; // 640x480
    config.jpeg_quality = 10;          // Lower number = higher quality (0-63)
    config.fb_count = 2;
    Serial.println("PSRAM detected - using VGA resolution (640x480)");
    Serial.printf("JPEG quality: %d, Frame buffers: %d\n", config.jpeg_quality,
                  config.fb_count);
  } else {
    config.frame_size = FRAMESIZE_QVGA; // 320x240
    config.jpeg_quality = 12;
    config.fb_count = 1;
    Serial.println("No PSRAM - using QVGA resolution (320x240)");
    Serial.printf("JPEG quality: %d, Frame buffers: %d\n", config.jpeg_quality,
                  config.fb_count);
  }

  Serial.printf("Clock frequency: %d Hz\n", config.xclk_freq_hz);
  Serial.println("Initializing camera...");

  // Initialize camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("ERROR: Camera init failed with error 0x%x\n", err);
    Serial.println("Common error codes:");
    Serial.println("  0x101 (ESP_ERR_NO_MEM): Out of memory");
    Serial.println("  0x102 (ESP_ERR_INVALID_ARG): Invalid argument");
    Serial.println("  0x103 (ESP_ERR_INVALID_STATE): Invalid state");
    Serial.println("  0x105 (ESP_ERR_NOT_FOUND): Not found");
    Serial.println("  0x106 (ESP_ERR_NOT_SUPPORTED): Not supported");
    Serial.println("  0x107 (ESP_ERR_TIMEOUT): Timeout");
    return false;
  }

  Serial.println("Camera hardware initialized successfully!");

  // Get camera sensor for additional settings
  sensor_t *s = esp_camera_sensor_get();
  if (s != NULL) {
    Serial.println("Configuring camera sensor settings...");

    // Adjust settings for better video quality
    s->set_brightness(s, 0); // -2 to 2
    s->set_contrast(s, 0);   // -2 to 2
    s->set_saturation(s, 0); // -2 to 2
    s->set_special_effect(s,
                          0); // 0 to 6 (0-No Effect, 1-Negative, 2-Grayscale,
                              // 3-Red Tint, 4-Green Tint, 5-Blue Tint, 6-Sepia)
    s->set_whitebal(s, 1);    // 0 = disable , 1 = enable
    s->set_awb_gain(s, 1);    // 0 = disable , 1 = enable
    s->set_wb_mode(s, 0); // 0 to 4 - if awb_gain enabled (0 - Auto, 1 - Sunny,
                          // 2 - Cloudy, 3 - Office, 4 - Home)
    s->set_exposure_ctrl(s, 1);              // 0 = disable , 1 = enable
    s->set_aec2(s, 0);                       // 0 = disable , 1 = enable
    s->set_ae_level(s, 0);                   // -2 to 2
    s->set_aec_value(s, 300);                // 0 to 1200
    s->set_gain_ctrl(s, 1);                  // 0 = disable , 1 = enable
    s->set_agc_gain(s, 0);                   // 0 to 30
    s->set_gainceiling(s, (gainceiling_t)0); // 0 to 6
    s->set_bpc(s, 0);                        // 0 = disable , 1 = enable
    s->set_wpc(s, 1);                        // 0 = disable , 1 = enable
    s->set_raw_gma(s, 1);                    // 0 = disable , 1 = enable
    s->set_lenc(s, 1);                       // 0 = disable , 1 = enable
    s->set_hmirror(s, 0);                    // 0 = disable , 1 = enable
    s->set_vflip(s, 0);                      // 0 = disable , 1 = enable
    s->set_dcw(s, 1);                        // 0 = disable , 1 = enable
    s->set_colorbar(s, 0);                   // 0 = disable , 1 = enable

    Serial.println("Camera sensor configured:");
    Serial.printf("  Brightness: %d, Contrast: %d, Saturation: %d\n", 0, 0, 0);
    Serial.printf("  White balance: enabled, Auto WB gain: enabled\n");
    Serial.printf("  Exposure control: enabled, AEC value: %d\n", 300);
    Serial.printf("  Gain control: enabled\n");
  } else {
    Serial.println("WARNING: Could not get camera sensor for configuration");
  }

  // Test camera by taking a test frame
  Serial.println("Testing camera with test frame...");
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("ERROR: Test frame capture failed!");
    return false;
  }

  Serial.printf("Test frame captured successfully: %d bytes, %dx%d pixels\n",
                fb->len, fb->width, fb->height);
  esp_camera_fb_return(fb);

  Serial.println("Camera initialized and tested successfully!");
  Serial.println("=============================");
  return true;
}

// Function to create a new video file
bool createNewVideoFile() {
  Serial.printf("=== Creating Video File #%d ===\n", fileCounter);

  if (videoFile) {
    Serial.println("Closing previous video file...");
    videoFile.close();
    Serial.printf("Previous file size: %lu bytes (%.2f MB)\n", currentFileSize,
                  currentFileSize / 1048576.0);
  }

  char filename[32];
  sprintf(filename, "/sdcard/video_%03d.mjpeg", fileCounter++);

  Serial.printf("Creating file: %s\n", filename);
  videoFile = SD_MMC.open(filename, FILE_WRITE);
  if (!videoFile) {
    Serial.printf("ERROR: Failed to create file: %s\n", filename);
    Serial.println("Possible causes:");
    Serial.println("  - SD card full");
    Serial.println("  - SD card write-protected");
    Serial.println("  - File system corruption");
    Serial.println("  - Too many open files");
    return false;
  }

  currentFileSize = 0;
  Serial.printf("Video file created successfully: %s\n", filename);
  Serial.println("===============================");
  return true;
}

// Function to capture and save frame
void captureFrame() {
  unsigned long startTime = millis();

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    failedFrameCaptures++;
    Serial.printf("ERROR: Camera capture failed (Total failures: %lu)\n",
                  failedFrameCaptures);

    if (failedFrameCaptures % 10 == 0) {
      Serial.println("Multiple capture failures detected. Possible causes:");
      Serial.println("  - Camera hardware issue");
      Serial.println("  - Insufficient memory");
      Serial.println("  - Power supply problems");
      printMemoryStatus();
    }
    return;
  }

  unsigned long captureTime = millis() - startTime;

  // Check if we need to create a new file
  if (!videoFile || currentFileSize >= MAX_FILE_SIZE) {
    Serial.printf("File size limit reached (%lu bytes), creating new file...\n",
                  currentFileSize);
    if (!createNewVideoFile()) {
      esp_camera_fb_return(fb);
      return;
    }
  }

  // Write frame to file
  unsigned long writeStartTime = millis();
  size_t written = videoFile.write(fb->buf, fb->len);
  unsigned long writeTime = millis() - writeStartTime;

  if (written != fb->len) {
    Serial.printf(
        "ERROR: Failed to write complete frame! Expected: %d, Written: %d\n",
        fb->len, written);
    Serial.println("Possible causes:");
    Serial.println("  - SD card full");
    Serial.println("  - SD card error");
    Serial.println("  - File system corruption");
  } else {
    currentFileSize += written;
    totalFramesCaptured++;

    // Print detailed frame info every 50 frames or if there were recent errors
    if (totalFramesCaptured % 50 == 0 || failedFrameCaptures > 0) {
      Serial.printf("Frame #%lu: %d bytes (%dx%d), Capture: %lums, Write: "
                    "%lums, File: %lu bytes\n",
                    totalFramesCaptured, fb->len, fb->width, fb->height,
                    captureTime, writeTime, currentFileSize);
    }
  }

  // Flush data to SD card periodically
  static int frameCount = 0;
  if (++frameCount % 10 == 0) {
    unsigned long flushStartTime = millis();
    videoFile.flush();
    unsigned long flushTime = millis() - flushStartTime;

    if (flushTime > 100) { // Warn if flush takes too long
      Serial.printf(
          "WARNING: SD card flush took %lums (may indicate SD card issues)\n",
          flushTime);
    }
  }

  esp_camera_fb_return(fb);
}

// Function to print recording statistics
void printRecordingStats() {
  Serial.println("=== Recording Statistics ===");
  Serial.printf("Total frames captured: %lu\n", totalFramesCaptured);
  Serial.printf("Failed captures: %lu\n", failedFrameCaptures);
  Serial.printf("Success rate: %.2f%%\n",
                totalFramesCaptured > 0
                    ? (float)(totalFramesCaptured * 100) /
                          (totalFramesCaptured + failedFrameCaptures)
                    : 0);
  Serial.printf("Current file: video_%03d.mjpeg\n", fileCounter - 1);
  Serial.printf("Current file size: %lu bytes (%.2f MB)\n", currentFileSize,
                currentFileSize / 1048576.0);
  Serial.printf("Recording time: %.2f minutes\n",
                totalFramesCaptured * FRAME_INTERVAL / 60000.0);
  Serial.printf("Average FPS: %.2f\n",
                millis() > 0 ? (totalFramesCaptured * 1000.0) / millis() : 0);
  Serial.println("============================");
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n\n=== ESP32-CAM Karting Recorder Starting ===");
  Serial.printf("Compile time: %s %s\n", __DATE__, __TIME__);
  Serial.printf("ESP32 Chip ID: %llX\n", ESP.getEfuseMac());
  Serial.printf("CPU Frequency: %d MHz\n", ESP.getCpuFreqMHz());

  delay(100); // give USB time to open

  Serial.println("\n=== PSRAM Check ===");
  bool hasPsram = psramFound();
  Serial.printf("PSRAM found: %s\n", hasPsram ? "true" : "false");
  if (hasPsram) {
    size_t total = ESP.getPsramSize(); // bytes physically present
    size_t free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    Serial.printf("PSRAM Size: %u bytes (%.1f MB)\n", total, total / 1048576.0);
    Serial.printf("PSRAM Free: %u bytes (%.1f MB)\n", free, free / 1048576.0);
    Serial.printf("PSRAM Usage: %.1f%%\n",
                  ((float)(total - free) / total) * 100);
  } else {
    Serial.println("ERROR: No PSRAM detected!");
    Serial.println("WARNING: PSRAM is required for decent video recording.");
    Serial.println(
        "Without PSRAM, the ESP32 cannot handle video capture properly.");
    Serial.println("Please use an ESP32 board with PSRAM.");
    Serial.println("System halted.");

    // Halt execution
    while (true) {
      delay(1000);
    }
  }
  Serial.println("==================");

  printMemoryStatus();

  Serial.println("\n=== SD Card Initialization ===");
  Serial.println("Performing GPIO diagnostics...");

  // Check GPIO states before SD card init
  Serial.println("GPIO pin states before SD init:");
  Serial.printf("  GPIO2 (D0): %d\n", digitalRead(2));
  Serial.printf("  GPIO4 (D1): %d\n", digitalRead(4));
  Serial.printf("  GPIO12 (D2): %d\n", digitalRead(12));
  Serial.printf("  GPIO13 (D3): %d\n", digitalRead(13));
  Serial.printf("  GPIO14 (CLK): %d\n", digitalRead(14));
  Serial.printf("  GPIO15 (CMD): %d\n", digitalRead(15));

  Serial.println("\nUsing ORIGINAL working SD card initialization...");
  Serial.println("SD_MMC.begin() - same as working version");

  // Use the EXACT same initialization that was working before
  // This matches the original simple code that worked
  int succ = SD_MMC.begin();

  if (succ == 0) {
    Serial.println("SD card mounted successfully!");
    printSDCardInfo();

    // Initialize camera after SD card is mounted
    if (initCamera()) {
      Serial.println("\n=== Starting Video Recording ===");
      Serial.printf("Frame interval: %lu ms (%.1f FPS)\n", FRAME_INTERVAL,
                    1000.0 / FRAME_INTERVAL);
      Serial.printf("Max file size: %.1f MB\n", MAX_FILE_SIZE / 1048576.0);
      Serial.println("Recording will start in 3 seconds...");

      for (int i = 3; i > 0; i--) {
        Serial.printf("Starting in %d...\n", i);
        delay(1000);
      }

      isRecording = true;
      lastFrameTime = millis();
      lastStatusPrint = millis();

      // Create first video file
      if (!createNewVideoFile()) {
        Serial.println("FATAL ERROR: Failed to create initial video file!");
        isRecording = false;
      } else {
        Serial.println("RECORDING STARTED!");
        Serial.println("================================");
      }
    } else {
      Serial.println("FATAL ERROR: Camera initialization failed!");
    }
  } else {
    Serial.printf("ERROR: SD card mount failed with code: %d\n", succ);
    Serial.println("Common SD card issues:");
    Serial.println("  - No SD card inserted");
    Serial.println("  - SD card not formatted (try FAT32)");
    Serial.println("  - Faulty SD card");
    Serial.println("  - Poor connection/wiring");
    Serial.println("  - Insufficient power supply");
    Serial.println("System halted.");
    while (true) {
      delay(1000);
    }
  }
}

void loop() {
  if (isRecording) {
    unsigned long currentTime = millis();

    // Capture frame at specified interval
    if (currentTime - lastFrameTime >= FRAME_INTERVAL) {
      captureFrame();
      lastFrameTime = currentTime;
    }

    // Print status periodically
    if (currentTime - lastStatusPrint >= STATUS_INTERVAL) {
      printRecordingStats();
      printMemoryStatus();
      lastStatusPrint = currentTime;
    }

    // Small delay to prevent watchdog timer issues
    delay(1);
  } else {
    // If not recording, print error status periodically
    static unsigned long lastErrorPrint = 0;
    if (millis() - lastErrorPrint > 5000) {
      Serial.println("ERROR: Not recording - check previous error messages");
      lastErrorPrint = millis();
    }
    delay(1000);
  }
}
