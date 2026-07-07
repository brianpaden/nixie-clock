#ifndef NIXIE_WIFI_MANAGER_NET_H
#define NIXIE_WIFI_MANAGER_NET_H

#include <Arduino.h>
#include <WiFiS3.h>

inline bool wifiManagerIpIsUsable(const IPAddress &ip) {
  return ip != IPAddress(0, 0, 0, 0);
}

inline bool wifiManagerStaIsUsable() {
  return WiFi.status() == WL_CONNECTED && wifiManagerIpIsUsable(WiFi.localIP());
}

#endif // NIXIE_WIFI_MANAGER_NET_H
