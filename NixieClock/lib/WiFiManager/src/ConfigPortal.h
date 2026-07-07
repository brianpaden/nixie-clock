#ifndef NIXIE_WIFI_MANAGER_CONFIG_PORTAL_H
#define NIXIE_WIFI_MANAGER_CONFIG_PORTAL_H

#include <Arduino.h>
#include <WiFiS3.h>

#include "CredentialStore.h"
#include "HttpServer.h"
#include "SettingsRegistry.h"

class ConfigPortal {
public:
  struct Config {
    uint16_t httpPort = 80;
    uint32_t staTimeoutMs = 10000;
  };

  ConfigPortal();

  void begin(const Config &config, CredentialStore &credentialStore,
             SettingsRegistry &settingsRegistry);
  void end();
  void loop();

  bool resetRequested() const;
  bool credentialsChanged() const;
  bool connectionInProgress() const;
  String activeSsid() const;
  String activePassword() const;

private:
  Config _config;
  CredentialStore *_credentialStore = nullptr;
  SettingsRegistry *_settingsRegistry = nullptr;
  HttpServer _http;
  bool _running = false;
  bool _routesRegistered = false;
  bool _connectPending = false;
  bool _connectStarted = false;
  bool _credentialsChanged = false;
  bool _resetRequested = false;
  uint32_t _connectStartedAt = 0;
  String _pendingSsid;
  String _pendingPassword;
  String _lastMessage;
  bool _lastMessageIsError = false;

  void registerRoutes();
  void startConnection(const String &ssid, const String &password);
  void processConnection();
  String renderStatusPage() const;
  String renderWifiPage(const String &message, bool error) const;
  String renderSettingsPage(const String &message, bool error) const;
  String renderConnectingPage() const;
  String renderNetworkField() const;

  static void handleIndex(const HttpRequest &request, HttpResponse &response,
                          void *context);
  static void handleWifi(const HttpRequest &request, HttpResponse &response,
                         void *context);
  static void handleConnect(const HttpRequest &request, HttpResponse &response,
                            void *context);
  static void handleConnecting(const HttpRequest &request,
                               HttpResponse &response, void *context);
  static void handleSettings(const HttpRequest &request, HttpResponse &response,
                             void *context);
  static void handleSettingsPost(const HttpRequest &request,
                                 HttpResponse &response, void *context);
  static void handleReset(const HttpRequest &request, HttpResponse &response,
                          void *context);
};

#endif // NIXIE_WIFI_MANAGER_CONFIG_PORTAL_H
