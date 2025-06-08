/********************************************************************
 *  Karting-Cam — ESP32-CAM + SD-MMC + Soft-AP HTTP file server
 *  -------------------------------------------------------------
 *  ‣ Captures a JPEG every min and stores it on the micro-SD card
 *  ‣ Creates its own Wi-Fi network  (SSID: KartCam, PW: p@ssword)
 *      OR join your home Wi-Fi by flipping USE_SOFT_AP to 0
 *  ‣ Tiny web UI   →  http://<board-ip>/        (lists files)
 *                      http://<board-ip>/f?name=<filename>  (downloads)
 ********************************************************************/

#include "config.h"
#include "log_util.h"
#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <SD_MMC.h>
#include <WiFi.h>
#include <esp_camera.h>

/* ---------------------- Camera pin assignment ---------------------- */
/*  These match the WROOM-32E + OV2640 wiring you posted.              */
#define CAM_PIN_PWDN 32  // PWDN
#define CAM_PIN_RESET -1 // (tied to EN)
#define CAM_PIN_XCLK 0   // XCLK
#define CAM_PIN_SIOD 26  // SDA (SIO_D)
#define CAM_PIN_SIOC 27  // SCL (SIO_C)
#define CAM_PIN_Y9 35
#define CAM_PIN_Y8 34
#define CAM_PIN_Y7 39
#define CAM_PIN_Y6 36
#define CAM_PIN_Y5 21
#define CAM_PIN_Y4 19
#define CAM_PIN_Y3 18
#define CAM_PIN_Y2 5
#define CAM_PIN_VSYNC 25
#define CAM_PIN_HREF 23
#define CAM_PIN_PCLK 22
/* ------------------------------------------------------------------- */

AsyncWebServer server(80);
static bool videoRecorded = false;

/* ------------------ Helper: start / join Wi-Fi --------------------- */
static void startWiFi() {
#if USE_SOFT_AP
  WiFi.softAP(AP_SSID, AP_PASS);
  LOG_PRINTF("[WiFi] Soft-AP started  SSID:%s  IP:%s\n", AP_SSID,
             WiFi.softAPIP().toString().c_str());
#else
  WiFi.mode(WIFI_STA);
  // Optimize for station mode performance
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false); // Don't save WiFi config to flash
  WiFi.begin(STA_SSID, STA_PASS);
  LOG_PRINTF("[WiFi] Connecting to %s", STA_SSID);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    LOG_PRINTF(".");
  }
  LOG_PRINTF("\n[WiFi] Connected  IP:%s  RSSI:%ddBm\n",
             WiFi.localIP().toString().c_str(), WiFi.RSSI());
#endif

  // Disable WiFi sleep for consistent performance
  WiFi.setSleep(false);

  // TCP optimizations for better throughput
  WiFi.setTxPower(WIFI_POWER_19_5dBm); // Max power

// Set larger TCP window size for better throughput
// Note: These may not be available in all ESP32 Arduino versions
#ifdef CONFIG_LWIP_TCP_WND_DEFAULT
// Already configured in sdkconfig
#endif

// Additional TCP optimizations for large chunk transfers
// Increase TCP send buffer size if available
#ifdef CONFIG_LWIP_TCP_SND_BUF_DEFAULT
// TCP send buffer already optimized in build config
#endif
}

/* ------------------ Helper: configure the camera ------------------- */
static bool initCamera() {
  camera_config_t cfg;
  cfg.ledc_channel = LEDC_CHANNEL_0;
  cfg.ledc_timer = LEDC_TIMER_0;
  cfg.pin_d0 = CAM_PIN_Y2;
  cfg.pin_d1 = CAM_PIN_Y3;
  cfg.pin_d2 = CAM_PIN_Y4;
  cfg.pin_d3 = CAM_PIN_Y5;
  cfg.pin_d4 = CAM_PIN_Y6;
  cfg.pin_d5 = CAM_PIN_Y7;
  cfg.pin_d6 = CAM_PIN_Y8;
  cfg.pin_d7 = CAM_PIN_Y9;
  cfg.pin_xclk = CAM_PIN_XCLK;
  cfg.pin_pclk = CAM_PIN_PCLK;
  cfg.pin_vsync = CAM_PIN_VSYNC;
  cfg.pin_href = CAM_PIN_HREF;
  cfg.pin_sscb_sda = CAM_PIN_SIOD;
  cfg.pin_sscb_scl = CAM_PIN_SIOC;
  cfg.pin_pwdn = CAM_PIN_PWDN;
  cfg.pin_reset = CAM_PIN_RESET;
  cfg.xclk_freq_hz = 20'000'000;
  cfg.pixel_format = PIXFORMAT_JPEG;

  // For stills a full 1600×1200 (UXGA) is handy.
  cfg.frame_size = FRAMESIZE_UXGA;
  cfg.jpeg_quality = 12; // 10-12 gives ~200 kB images
  cfg.fb_count = 1;      // single frame buffer

  esp_err_t err = esp_camera_init(&cfg);
  if (err != ESP_OK) {
    LOG_PRINTF("Camera init failed: 0x%04x\n", err);
    return false;
  }
  return true;
}

/* ------------- HTTP handlers: list root + stream file -------------- */
static void startWebServer() {
  /* / -> HTML directory listing  */
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
    String html = "<h2>Captured JPEGs</h2><ul>";
    File dir = SD_MMC.open("/");
    while (File f = dir.openNextFile()) {
      if (!f.isDirectory()) {
        String name = String(f.name());
        html += "<li><a href=\"/f?name=" + name + "\">";
        html += name + " (" + String(f.size()) + " B)</a></li>";
      }
      f.close();
    }
    html += "</ul>";
    req->send(200, "text/html", html);
  });

  /* /f?name=cap00042.jpg -> custom high-performance streaming */
  server.on("/f", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (!req->hasParam("name")) {
      req->send(400, "text/plain", "Missing ?name=");
      return;
    }
    String fname = req->getParam("name")->value();
    if (!fname.startsWith("/"))
      fname = "/" + fname;

    File file = SD_MMC.open(fname, FILE_READ);
    if (!file) {
      req->send(404, "text/plain", "File not found");
      return;
    }

    // Determine MIME type based on file extension
    String mimeType = "application/octet-stream"; // default
    if (fname.endsWith(".jpg") || fname.endsWith(".jpeg")) {
      mimeType = "image/jpeg";
    } else if (fname.endsWith(".mjpeg") || fname.endsWith(".mjpg")) {
      mimeType = "video/x-motion-jpeg";
    } else if (fname.endsWith(".txt") || fname.endsWith(".log")) {
      mimeType = "text/plain";
    }

    size_t fileSize = file.size();
    file.close();

    // Use a streaming response with larger buffer
    AsyncWebServerResponse *response = req->beginResponse(
        mimeType, fileSize,
        [fname](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
          static File streamFile;

          // Open file on first call
          if (index == 0) {
            streamFile = SD_MMC.open(fname, FILE_READ);
            if (!streamFile)
              return 0;
          }

          if (!streamFile.available()) {
            streamFile.close();
            return 0;
          }

          // Read up to 64KB chunks for maximum performance
          size_t available = (size_t)streamFile.available();
          size_t toRead = min(maxLen, min((size_t)65536, available));
          size_t bytesRead = streamFile.read(buffer, toRead);

          if (!streamFile.available()) {
            streamFile.close();
          }

          return bytesRead;
        });

    response->addHeader("Accept-Ranges", "bytes");
    response->addHeader("Cache-Control", "public, max-age=3600");

    req->send(response);
  });

  // Configure server for better performance
  server.onNotFound([](AsyncWebServerRequest *req) {
    req->send(404, "text/plain", "Not Found");
  });

  server.begin();
  LOG_PRINTLN("[HTTP] Server started with 64KB chunk streaming");
}

/* ------------------------------ setup() ---------------------------- */
void setup() {
  Serial.begin(115200);
  LOG_PRINTLN("\n=== ESP32 Karting-Cam boot ===");

  if (!initCamera()) {
    LOG_PRINTLN("Fatal camera error — halting.");
    while (true)
      delay(1000);
  }

  // 4-bit, no format, 52MHz
  if (!SD_MMC.begin("/sd", /*mode1bit =*/false,
                    /*format_if_mount_failed =*/false,
                    /*max_freq =*/SDMMC_FREQ_52M)) {
    LOG_PRINTLN("SD_MMC mount failed — halting.");
    while (true)
      delay(1000);
  }
  uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
  LOG_PRINTF("SD card OK — %llu MB\n", cardSize);
  logInit();

  startWiFi();
  startWebServer();
}

/* ------------------------------ loop() ----------------------------- */
void loop() {
  if (videoRecorded) {
    delay(1000);
    return;
  }

  File vid = SD_MMC.open("/video.mjpeg", FILE_WRITE);
  if (!vid) {
    LOG_PRINTLN("Video file open failed");
    videoRecorded = true;
    return;
  }

  LOG_PRINTLN("[Video] Recording 10 seconds …");
  uint32_t start = millis();
  while (millis() - start < 10 * 1000) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      LOG_PRINTLN("Camera capture failed");
      break;
    }
    vid.write(fb->buf, fb->len);
    esp_camera_fb_return(fb);
    delay(100); // ~10 FPS
  }
  vid.close();
  LOG_PRINTLN("[Video] Saved /video.mjpeg");
  videoRecorded = true;
}
