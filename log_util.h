#ifndef LOG_UTIL_H
#define LOG_UTIL_H

#include <Arduino.h>
#include <FS.h>
#include <SD_MMC.h>

static File logFile;

static inline bool logInit(const char *path = "/log.txt") {
  logFile = SD_MMC.open(path, FILE_APPEND);
  if (!logFile) {
    Serial.println("[LOG] Failed to open log file");
    return false;
  }
  return true;
}

#define LOG_PRINTLN(msg)                              \
  do {                                               \
    Serial.println(msg);                             \
    if (logFile) {                                   \
      logFile.println(msg);                          \
      logFile.flush();                               \
    }                                                \
  } while (0)

#define LOG_PRINTF(fmt, ...)                          \
  do {                                               \
    Serial.printf((fmt), ##__VA_ARGS__);             \
    if (logFile) {                                   \
      logFile.printf((fmt), ##__VA_ARGS__);          \
      logFile.flush();                               \
    }                                                \
  } while (0)

#endif // LOG_UTIL_H
