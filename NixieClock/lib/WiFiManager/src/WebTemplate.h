#ifndef NIXIE_WIFI_MANAGER_WEB_TEMPLATE_H
#define NIXIE_WIFI_MANAGER_WEB_TEMPLATE_H

#include <Arduino.h>

String wifiManagerStringFromProgmem(const char *value);
void wifiManagerReplaceAll(String &subject, const String &token,
                           const String &value);
String wifiManagerRenderShell(const String &title, const String &body);

#endif // NIXIE_WIFI_MANAGER_WEB_TEMPLATE_H
