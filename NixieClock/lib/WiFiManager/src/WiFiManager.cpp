#include "WiFiManager.h"

#include "WiFiManagerDebug.h"
#include "WiFiManagerNet.h"

namespace {

const uint8_t kMaxStaDropRetries = 3;
const uint32_t kDropRetryDelayMs = 1000;

} // namespace

WiFiManager::WiFiManager()
    : _credentialStore("wifimgr"), _settings("nixie-cfg") {}

void WiFiManager::begin() { begin(Config()); }

void WiFiManager::begin(const Config &config) {
  _config = config;
  _credentialStore.setNamespace(_config.preferencesNamespace);
  _state = State::Boot;
  WIFIMGR_LOG(F("begin"));

  if (_credentialStore.load(_ssid, _password)) {
    WIFIMGR_LOG_VALUE(F("Saved credentials found for SSID: "), _ssid);
    startStaConnect(_ssid, _password);
    return;
  }

  WIFIMGR_LOG(F("No saved credentials; starting setup AP"));
  startApPortal();
}

void WiFiManager::loop() {
  switch (_state) {
  case State::Boot:
    begin(_config);
    break;
  case State::StaConnecting:
    handleStaConnecting();
    break;
  case State::StaConnected:
    _configPortal.loop();
    if (_configPortal.resetRequested()) {
      WiFi.disconnect();
      startApPortal();
      break;
    }
    if (_configPortal.credentialsChanged()) {
      _ssid = _configPortal.activeSsid();
      _password = _configPortal.activePassword();
    }
    if (_configPortal.connectionInProgress()) {
      break;
    }
    handleStaConnected();
    break;
  case State::ApPortal:
    _captivePortal.loop();
    if (_captivePortal.hasConnected()) {
      _ssid = _captivePortal.connectedSsid();
      _password = _captivePortal.connectedPassword();
      _captivePortal.end();
      startConfigPortal();
      _dropRetries = 0;
    }
    break;
  }
}

bool WiFiManager::isConnected() const {
  return _state == State::StaConnected && wifiManagerStaIsUsable();
}

SettingsRegistry &WiFiManager::settings() { return _settings; }

IPAddress WiFiManager::localIP() const { return WiFi.localIP(); }

void WiFiManager::startStaConnect(const String &ssid, const String &password) {
  _configPortal.end();
  _ssid = ssid;
  _password = password;
  _staConnectStartedAt = millis();
  _state = State::StaConnecting;
  WIFIMGR_LOG_VALUE(F("STA connecting to SSID: "), _ssid);

  if (_password.length() == 0) {
    WiFi.begin(_ssid.c_str());
  } else {
    WiFi.begin(_ssid.c_str(), _password.c_str());
  }
}

void WiFiManager::startApPortal() {
  _configPortal.end();
  WIFIMGR_LOG(F("Switching to AP portal"));

  CaptivePortal::Config portalConfig;
  portalConfig.apSsid = _config.apSsid;
  portalConfig.apPassword = _config.apPassword;
  portalConfig.httpPort = _config.httpPort;
  portalConfig.dnsPort = _config.dnsPort;
  portalConfig.staTimeoutMs = _config.staTimeoutMs;

  _captivePortal.begin(portalConfig, _credentialStore);
  _state = State::ApPortal;
}

void WiFiManager::startConfigPortal() {
  _captivePortal.end();

  ConfigPortal::Config portalConfig;
  portalConfig.httpPort = _config.httpPort;
  portalConfig.staTimeoutMs = _config.staTimeoutMs;

  _configPortal.begin(portalConfig, _credentialStore, _settings);
  _state = State::StaConnected;
  WIFIMGR_LOG_VALUE(F("LAN config portal available at IP: "), WiFi.localIP());
}

void WiFiManager::handleStaConnecting() {
  if (wifiManagerStaIsUsable()) {
    WIFIMGR_LOG_VALUE(F("STA connected, IP: "), WiFi.localIP());
    startConfigPortal();
    _dropRetries = 0;
    return;
  }

  if (millis() - _staConnectStartedAt >= _config.staTimeoutMs) {
    WIFIMGR_LOG(F("STA connect timeout; falling back to AP portal"));
    WiFi.disconnect();
    startApPortal();
  }
}

void WiFiManager::handleStaConnected() {
  if (wifiManagerStaIsUsable()) {
    _dropRetries = 0;
    return;
  }

  if (_dropRetries >= kMaxStaDropRetries) {
    WIFIMGR_LOG(F("STA dropped repeatedly; falling back to AP portal"));
    startApPortal();
    return;
  }

  ++_dropRetries;
  WIFIMGR_LOG_VALUE(F("STA dropped; retry "), _dropRetries);
  _staConnectStartedAt = millis() - (_config.staTimeoutMs - kDropRetryDelayMs);
  startStaConnect(_ssid, _password);
}
