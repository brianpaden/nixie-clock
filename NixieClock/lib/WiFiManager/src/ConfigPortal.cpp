#include "ConfigPortal.h"

#include "WiFiManagerNet.h"
#include "WebTemplate.h"
#include "web/Assets.h"

namespace {

String encryptionLabelForConfig(uint8_t encryptionType) {
  switch (encryptionType) {
  case ENC_TYPE_NONE:
    return F("open");
  case ENC_TYPE_WEP:
    return F("WEP");
  case ENC_TYPE_TKIP:
    return F("WPA");
  case ENC_TYPE_CCMP:
    return F("WPA2");
  case ENC_TYPE_AUTO:
    return F("auto");
  default:
    return F("secured");
  }
}

} // namespace

ConfigPortal::ConfigPortal() {}

void ConfigPortal::begin(const Config &config, CredentialStore &credentialStore,
                         SettingsRegistry &settingsRegistry) {
  _config = config;
  _credentialStore = &credentialStore;
  _settingsRegistry = &settingsRegistry;
  _connectPending = false;
  _connectStarted = false;
  _credentialsChanged = false;
  _resetRequested = false;
  _lastMessage = "";
  _lastMessageIsError = false;

  if (!_routesRegistered) {
    registerRoutes();
    _routesRegistered = true;
  }

  _http.begin(_config.httpPort);
  _running = true;
}

void ConfigPortal::end() {
  _http.end();
  _running = false;
}

void ConfigPortal::loop() {
  if (!_running) {
    return;
  }

  processConnection();
  _http.loop();
}

bool ConfigPortal::resetRequested() const { return _resetRequested; }

bool ConfigPortal::credentialsChanged() const { return _credentialsChanged; }

bool ConfigPortal::connectionInProgress() const { return _connectPending; }

String ConfigPortal::activeSsid() const { return _pendingSsid; }

String ConfigPortal::activePassword() const { return _pendingPassword; }

void ConfigPortal::registerRoutes() {
  _http.on("GET", "/", handleIndex, this);
  _http.on("GET", "/wifi", handleWifi, this);
  _http.on("POST", "/connect", handleConnect, this);
  _http.on("GET", "/connecting", handleConnecting, this);
  _http.on("GET", "/settings", handleSettings, this);
  _http.on("POST", "/settings", handleSettingsPost, this);
  _http.on("POST", "/reset", handleReset, this);
  _http.onNotFound(handleIndex, this);
}

void ConfigPortal::startConnection(const String &ssid, const String &password) {
  _pendingSsid = ssid;
  _pendingPassword = password;
  _connectPending = true;
  _connectStarted = false;
  _credentialsChanged = false;
  _lastMessage = "";
  _lastMessageIsError = false;
}

void ConfigPortal::processConnection() {
  if (!_connectPending) {
    return;
  }

  if (!_connectStarted) {
    _connectStartedAt = millis();
    _connectStarted = true;
    if (_pendingPassword.length() == 0) {
      WiFi.begin(_pendingSsid.c_str());
    } else {
      WiFi.begin(_pendingSsid.c_str(), _pendingPassword.c_str());
    }
  }

  if (wifiManagerStaIsUsable()) {
    _connectPending = false;
    _credentialsChanged = true;
    _lastMessage = F("WiFi credentials updated.");
    _lastMessageIsError = false;
    if (_credentialStore != nullptr) {
      _credentialStore->save(_pendingSsid, _pendingPassword);
    }
    return;
  }

  if (millis() - _connectStartedAt >= _config.staTimeoutMs) {
    _connectPending = false;
    _connectStarted = false;
    _lastMessage = F("Connection failed or DHCP did not assign an IP address. The previous network may need to be restored manually.");
    _lastMessageIsError = true;
  }
}

String ConfigPortal::renderStatusPage() const {
  String body =
      wifiManagerStringFromProgmem(NixieWiFiManagerWeb::STATUS_BODY);
  wifiManagerReplaceAll(body, F("{{HEADING}}"), F("Nixie Clock"));

  String message = F("Connected to ");
  message += HttpServer::htmlEscape(WiFi.SSID());
  message += F(" at ");
  message += WiFi.localIP().toString();
  message += '.';
  wifiManagerReplaceAll(body, F("{{MESSAGE}}"), message);

  body += F("<form method=\"post\" action=\"/reset\">"
            "<button type=\"submit\">Reset WiFi</button></form>");
  return wifiManagerRenderShell(F("Nixie Clock"), body);
}

String ConfigPortal::renderWifiPage(const String &message, bool error) const {
  String body =
      wifiManagerStringFromProgmem(NixieWiFiManagerWeb::WIFI_FORM_BODY);
  wifiManagerReplaceAll(body, F("{{HEADING}}"), F("Change WiFi"));
  wifiManagerReplaceAll(body, F("{{INTRO}}"),
                        F("Choose a new network for the clock."));

  String messageHtml;
  if (message.length() > 0) {
    messageHtml = F("<div class=\"message");
    if (error) {
      messageHtml += F(" error");
    }
    messageHtml += F("\">");
    messageHtml += HttpServer::htmlEscape(message);
    messageHtml += F("</div>");
  }
  wifiManagerReplaceAll(body, F("{{MESSAGE}}"), messageHtml);
  wifiManagerReplaceAll(body, F("{{NETWORK_FIELD}}"), renderNetworkField());
  return wifiManagerRenderShell(F("Change WiFi"), body);
}

String ConfigPortal::renderSettingsPage(const String &message, bool error) const {
  String body;
  body += F("<h1>Settings</h1>");
  if (message.length() > 0) {
    body += F("<div class=\"message");
    if (error) {
      body += F(" error");
    }
    body += F("\">");
    body += HttpServer::htmlEscape(message);
    body += F("</div>");
  }

  if (_settingsRegistry == nullptr) {
    body += F("<p>No settings are registered.</p>");
  } else {
    body += _settingsRegistry->renderFormHtml();
  }

  body += F("<div class=\"actions\"><a class=\"button\" href=\"/\">Back</a></div>");
  return wifiManagerRenderShell(F("Settings"), body);
}

String ConfigPortal::renderConnectingPage() const {
  String body =
      wifiManagerStringFromProgmem(NixieWiFiManagerWeb::RESULT_BODY);

  if (_connectPending) {
    String message = F("Trying to connect to ");
    message += HttpServer::htmlEscape(_pendingSsid);
    message += F(". Refresh this page in a few seconds.");
    wifiManagerReplaceAll(body, F("{{HEADING}}"), F("Connecting"));
    wifiManagerReplaceAll(body, F("{{MESSAGE}}"), message);
  } else if (_lastMessageIsError) {
    wifiManagerReplaceAll(body, F("{{HEADING}}"), F("Connection failed"));
    wifiManagerReplaceAll(body, F("{{MESSAGE}}"),
                          HttpServer::htmlEscape(_lastMessage));
  } else {
    String message = F("Connected to ");
    message += HttpServer::htmlEscape(WiFi.SSID());
    message += F(". IP address: ");
    message += WiFi.localIP().toString();
    message += '.';
    wifiManagerReplaceAll(body, F("{{HEADING}}"), F("Connected"));
    wifiManagerReplaceAll(body, F("{{MESSAGE}}"), message);
  }

  return wifiManagerRenderShell(F("WiFi Connection"), body);
}

String ConfigPortal::renderNetworkField() const {
  String field;
  const int8_t networkCount = WiFi.scanNetworks();

  if (networkCount > 0) {
    field += F("<select id=\"ssid\" name=\"ssid\">");
    for (int8_t i = 0; i < networkCount; ++i) {
      const String ssid = WiFi.SSID(static_cast<uint8_t>(i));
      field += F("<option value=\"");
      field += HttpServer::htmlEscape(ssid);
      field += F("\">");
      field += HttpServer::htmlEscape(ssid);
      field += F(" (");
      field += WiFi.RSSI(static_cast<uint8_t>(i));
      field += F(" dBm, ");
      field += encryptionLabelForConfig(WiFi.encryptionType(static_cast<uint8_t>(i)));
      field += F(")</option>");
    }
    field += F("</select>");
    field += F("<label for=\"ssid_text\">Other network</label>");
    field += F("<input id=\"ssid_text\" name=\"ssid_text\" type=\"text\" autocomplete=\"off\">");
  } else {
    field += F("<input id=\"ssid\" name=\"ssid\" type=\"text\" autocomplete=\"off\" required>");
    field += F("<p class=\"network-meta\">No scan results are available. Enter the network name manually.</p>");
  }

  return field;
}

void ConfigPortal::handleIndex(const HttpRequest &request,
                               HttpResponse &response, void *context) {
  (void)request;
  ConfigPortal *portal = static_cast<ConfigPortal *>(context);
  response.body = portal->renderStatusPage();
}

void ConfigPortal::handleWifi(const HttpRequest &request,
                              HttpResponse &response, void *context) {
  (void)request;
  ConfigPortal *portal = static_cast<ConfigPortal *>(context);
  response.body =
      portal->renderWifiPage(portal->_lastMessage, portal->_lastMessageIsError);
}

void ConfigPortal::handleConnect(const HttpRequest &request,
                                 HttpResponse &response, void *context) {
  ConfigPortal *portal = static_cast<ConfigPortal *>(context);

  String ssid = request.formValue(F("ssid_text"));
  if (ssid.length() == 0) {
    ssid = request.formValue(F("ssid"));
  }
  ssid.trim();

  if (ssid.length() == 0) {
    response.body = portal->renderWifiPage(F("Enter a WiFi network name."), true);
    return;
  }

  portal->startConnection(ssid, request.formValue(F("password")));
  response = HttpServer::redirect("/connecting");
}

void ConfigPortal::handleConnecting(const HttpRequest &request,
                                    HttpResponse &response, void *context) {
  (void)request;
  ConfigPortal *portal = static_cast<ConfigPortal *>(context);
  response.body = portal->renderConnectingPage();
  if (portal->_connectPending) {
    response.extraHeaders += F("Refresh: 3\r\n");
  }
}

void ConfigPortal::handleSettings(const HttpRequest &request,
                                  HttpResponse &response, void *context) {
  (void)request;
  ConfigPortal *portal = static_cast<ConfigPortal *>(context);
  response.body = portal->renderSettingsPage("", false);
}

void ConfigPortal::handleSettingsPost(const HttpRequest &request,
                                      HttpResponse &response, void *context) {
  ConfigPortal *portal = static_cast<ConfigPortal *>(context);
  String errorMessage;

  if (portal->_settingsRegistry == nullptr ||
      !portal->_settingsRegistry->updateFromRequest(request, errorMessage)) {
    response.body = portal->renderSettingsPage(errorMessage, true);
    return;
  }

  response.body = portal->renderSettingsPage(F("Settings saved."), false);
}

void ConfigPortal::handleReset(const HttpRequest &request,
                               HttpResponse &response, void *context) {
  (void)request;
  ConfigPortal *portal = static_cast<ConfigPortal *>(context);
  if (portal->_credentialStore != nullptr) {
    portal->_credentialStore->clear();
  }
  portal->_resetRequested = true;

  String body =
      wifiManagerStringFromProgmem(NixieWiFiManagerWeb::RESULT_BODY);
  wifiManagerReplaceAll(body, F("{{HEADING}}"), F("WiFi reset"));
  wifiManagerReplaceAll(body, F("{{MESSAGE}}"),
                        F("Saved WiFi credentials were cleared. The setup network will start."));
  response.body = wifiManagerRenderShell(F("WiFi reset"), body);
}
