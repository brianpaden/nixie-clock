#ifndef NIXIE_WIFI_MANAGER_DEBUG_H
#define NIXIE_WIFI_MANAGER_DEBUG_H

#include <Arduino.h>

#if defined(WIFIMGR_DEBUG)
#define WIFIMGR_LOG(message)                                                  \
  do {                                                                        \
    Serial.print(F("[WiFiManager] "));                                        \
    Serial.println(message);                                                  \
  } while (0)
#define WIFIMGR_LOG_VALUE(label, value)                                       \
  do {                                                                        \
    Serial.print(F("[WiFiManager] "));                                        \
    Serial.print(label);                                                      \
    Serial.println(value);                                                    \
  } while (0)
#else
#define WIFIMGR_LOG(message)                                                  \
  do {                                                                        \
  } while (0)
#define WIFIMGR_LOG_VALUE(label, value)                                       \
  do {                                                                        \
  } while (0)
#endif

#endif // NIXIE_WIFI_MANAGER_DEBUG_H
