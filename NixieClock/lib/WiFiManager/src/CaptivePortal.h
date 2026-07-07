#ifndef NIXIE_WIFI_MANAGER_CAPTIVE_PORTAL_H
#define NIXIE_WIFI_MANAGER_CAPTIVE_PORTAL_H

#include <Arduino.h>
#include <WiFiS3.h>

#include "CredentialStore.h"
#include "DnsServer.h"
#include "HttpServer.h"

class CaptivePortal {
public:
  struct Config {
    const char *apSsid = "NixieClock-Setup";
    const char *apPassword = "nixie1234";
    uint16_t httpPort = 80;
    uint16_t dnsPort = 53;
    uint32_t staTimeoutMs = 10000;
  };

  CaptivePortal();

  bool begin(const Config &config, CredentialStore &credentialStore);
  void end();
  void loop();

  bool hasConnected() const;
  IPAddress connectedIP() const;
  String connectedSsid() const;
  String connectedPassword() const;

private:
  Config _config;
  CredentialStore *_credentialStore = nullptr;
  HttpServer _http;
  DnsServer _dns;
  bool _running = false;
  bool _routesRegistered = false;
  bool _connectPending = false;
  bool _connectStarted = false;
  bool _connected = false;
  bool _testOnly = false;
  bool _testComplete = false;
  bool _testSucceeded = false;
  uint32_t _connectStartedAt = 0;
  String _pendingSsid;
  String _pendingPassword;
  String _lastError;
  String _lastMessage;
  IPAddress _connectedIp;

  void registerRoutes();
  void startConnection(const String &ssid, const String &password,
                       bool testOnly);
  void processConnection();
  String renderWifiPage(const String &message, bool error);
  String renderScanPage();
  String renderConnectingPage();
  String renderTestingPage();
  String renderManualNetworkField();
  String renderScanNetworkRows(int8_t networkCount);
  bool startAccessPoint();
  void restoreAccessPointAfterAttempt();

  static void handleIndex(const HttpRequest &request, HttpResponse &response,
                          void *context);
  static void handleScan(const HttpRequest &request, HttpResponse &response,
                         void *context);
  static void handleTest(const HttpRequest &request, HttpResponse &response,
                         void *context);
  static void handleConnect(const HttpRequest &request, HttpResponse &response,
                            void *context);
  static void handleConnecting(const HttpRequest &request,
                               HttpResponse &response, void *context);
  static void handleTesting(const HttpRequest &request, HttpResponse &response,
                            void *context);
};

#endif // NIXIE_WIFI_MANAGER_CAPTIVE_PORTAL_H
