/**
 *  ESP32-WROOM-32E  + “ESP32 Camera Extension”
 *  SD-MMC   ↔   camera coexistence demo
 *  Core  = Arduino-ESP32 v2.0.11 (or newer)
 *  Board = “ESP32 Dev Module” (NOT “ESP32-CAM”)
 */

#include "SD_MMC.h"
#include "esp_camera.h"

/* ---------- Camera pin map (your FFC table) ---------- */
#define CAM_PIN_PWDN 32  // PWDN
#define CAM_PIN_RESET -1 // hard-wired
#define CAM_PIN_XCLK 0   // XCLK
#define CAM_PIN_SIOD 26  // SIO_D
#define CAM_PIN_SIOC 27  // SIO_C
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

/* ---------- SD-MMC pin map (your micro-SD table) ------ */
#define SD_PIN_CLK 14
#define SD_PIN_CMD 15
#define SD_PIN_D0 2
#define SD_PIN_D1 4
#define SD_PIN_D2 12
#define SD_PIN_D3 13

/* ------------------------------------------------------ */
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== ESP32 SD-MMC + Camera test ===");

  /* ---- Camera init (comment section out if not needed) */
  camera_config_t cam;
  cam.ledc_channel = LEDC_CHANNEL_0;
  cam.ledc_timer = LEDC_TIMER_0;
  cam.pin_d0 = CAM_PIN_Y2;
  cam.pin_d1 = CAM_PIN_Y3;
  cam.pin_d2 = CAM_PIN_Y4;
  cam.pin_d3 = CAM_PIN_Y5;
  cam.pin_d4 = CAM_PIN_Y6;
  cam.pin_d5 = CAM_PIN_Y7;
  cam.pin_d6 = CAM_PIN_Y8;
  cam.pin_d7 = CAM_PIN_Y9;
  cam.pin_xclk = CAM_PIN_XCLK;
  cam.pin_pclk = CAM_PIN_PCLK;
  cam.pin_vsync = CAM_PIN_VSYNC;
  cam.pin_href = CAM_PIN_HREF;
  cam.pin_sccb_sda = CAM_PIN_SIOD;
  cam.pin_sccb_scl = CAM_PIN_SIOC;
  cam.pin_pwdn = CAM_PIN_PWDN;
  cam.pin_reset = CAM_PIN_RESET;
  cam.xclk_freq_hz = 20'000'000;
  cam.pixel_format = PIXFORMAT_JPEG;
  cam.frame_size = FRAMESIZE_VGA;
  cam.jpeg_quality = 12;
  cam.fb_count = 2;

  if (esp_camera_init(&cam) == ESP_OK)
    Serial.println("Camera initialised ✔");
  else
    Serial.println("Camera initialisation FAILED ✖");

  /* ---- SD-MMC init ----------------------------------- */
  SD_MMC.setPins(SD_PIN_CLK, SD_PIN_CMD, SD_PIN_D0, SD_PIN_D1, SD_PIN_D2,
                 SD_PIN_D3);

  bool sd_ok = SD_MMC.begin("/sdcard", /*1-bit?*/ false);
  // if (!sd_ok) {
  //   Serial.println("1-bit mount failed → trying 4-bit …");
  //   sd_ok = SD_MMC.begin("/sdcard", /*1-bit*/false);
  // }

  if (!sd_ok) {
    Serial.println("SD mount FAILED – check wiring / pull-ups / format");
    return;
  }
  Serial.println("SD mounted 🎉");
  Serial.printf("Card type : %u\n", SD_MMC.cardType());
  Serial.printf("Card size : %llu MB\n", SD_MMC.cardSize() >> 20);

  /* quick R/W sanity check */
  File f = SD_MMC.open("/test.txt", FILE_WRITE);
  if (f) {
    f.println("Hello SD-MMC!");
    f.close();
  }
}

void loop() {
  /* capture & save a JPEG every 5 s */
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return;
  }

  static uint32_t frame = 0;
  char path[32];
  sprintf(path, "/cap%05u.jpg", frame++);
  File img = SD_MMC.open(path, FILE_WRITE);
  if (img) {
    img.write(fb->buf, fb->len);
    img.close();
    Serial.printf("Saved %s (%u bytes)\n", path, fb->len);
  } else {
    Serial.printf("File open failed for %s\n", path);
  }
  esp_camera_fb_return(fb);
  delay(5000);
}
