#ifndef NIXIE_WIFI_MANAGER_WIFI_MANAGER_H
#define NIXIE_WIFI_MANAGER_WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFiS3.h>

#include "CaptivePortal.h"
#include "ConfigPortal.h"
#include "CredentialStore.h"
#include "SettingsRegistry.h"

class WiFiManager {
public:
  struct Config {
    const char *apSsid = "NixieClock-Setup";
    const char *apPassword = "nixie1234";
    uint32_t staTimeoutMs = 10000;
    uint16_t httpPort = 80;
    uint16_t dnsPort = 53;
    const char *preferencesNamespace = "wifimgr";
  };

  WiFiManager();

  void begin();
  void begin(const Config &config);
  void loop();
  bool isConnected() const;
  SettingsRegistry &settings();
  IPAddress localIP() const;

private:
  enum class State : uint8_t { Boot, StaConnecting, StaConnected, ApPortal };

  Config _config;
  State _state = State::Boot;
  CredentialStore _credentialStore;
  SettingsRegistry _settings;
  CaptivePortal _captivePortal;
  ConfigPortal _configPortal;
  String _ssid;
  String _password;
  uint32_t _staConnectStartedAt = 0;
  uint8_t _dropRetries = 0;

  void startStaConnect(const String &ssid, const String &password);
  void startConfigPortal();
  void startApPortal();
  void handleStaConnecting();
  void handleStaConnected();
};

#endif // NIXIE_WIFI_MANAGER_WIFI_MANAGER_H
