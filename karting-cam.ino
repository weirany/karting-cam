// ---- force AI-Thinker pin map ----
#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

#include <Arduino.h>
#include "esp_heap_caps.h" // only needed for the free-PSRAM call

void setup()
{
  Serial.begin(115200);
  Serial.println("Starting...");

  delay(100); // give USB time to open

  bool hasPsram = psramFound();
  Serial.printf("PSRAM found: %s\n", hasPsram ? "true" : "false");

  if (hasPsram)
  {
    size_t total = ESP.getPsramSize(); // bytes physically present
    size_t free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    Serial.printf("Size: %u bytes (%.1f MB)\n", total, total / 1048576.0);
    Serial.printf("Free: %u bytes\n", free);
  }
  else
  {
    Serial.println("ERROR: No PSRAM detected!");
    Serial.println("WARNING: PSRAM is required for decent video recording.");
    Serial.println("Without PSRAM, the ESP32 cannot handle video capture properly.");
    Serial.println("Please use an ESP32 board with PSRAM.");
    Serial.println("System halted.");

    // Halt execution
    while (true)
    {
      delay(1000);
    }
  }
}

void loop() { /* nothing */ }
