#include "CaptivePortal.h"

#include "WiFiManagerDebug.h"
#include "WiFiManagerNet.h"
#include "WebTemplate.h"
#include "web/Assets.h"

namespace {

String encryptionLabel(uint8_t encryptionType) {
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

void appendApPageStart(String &page, const __FlashStringHelper *title) {
  page += F("<!doctype html><html><head><meta charset=\"utf-8\">"
            "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
            "<title>");
  page += title;
  page += F("</title><style>"
            "body{font-family:sans-serif;margin:0;background:#f6f7f9;color:#17202a}"
            "main{max-width:560px;margin:28px auto;padding:20px;background:#fff;"
            "border:1px solid #d7dde5;border-radius:8px}"
            "label{display:block;margin-top:14px;font-weight:700}"
            "input,select{width:100%;box-sizing:border-box;margin-top:6px;"
            "padding:11px;border:1px solid #d7dde5;border-radius:6px;font:inherit}"
            "input[type=radio]{width:auto;margin:0 8px 0 0}"
            "button,.button{display:inline-block;margin-top:18px;padding:11px 16px;"
            "border:0;border-radius:6px;background:#0f766e;color:#fff;"
            "font:inherit;font-weight:700;text-decoration:none}"
            "table{width:100%;border-collapse:collapse;margin-top:14px}"
            "th,td{text-align:left;border-bottom:1px solid #d7dde5;padding:8px 4px}"
            ".message{margin:14px 0;padding:10px;border:1px solid #d7dde5;"
            "border-radius:6px;background:#eef6f5}.error{color:#b42318}"
            ".network-meta{color:#5d6875;font-size:.9rem}"
            "</style></head><body><main>");
}

void appendApPageEnd(String &page) {
  page += F("</main></body></html>");
}

} // namespace

CaptivePortal::CaptivePortal() : _connectedIp(0, 0, 0, 0) {}

bool CaptivePortal::begin(const Config &config,
                          CredentialStore &credentialStore) {
  _config = config;
  _credentialStore = &credentialStore;
  _lastError = "";
  _connected = false;
  _connectPending = false;
  _connectStarted = false;
  _testOnly = false;
  _testComplete = false;
  _testSucceeded = false;
  _lastMessage = "";

  if (!startAccessPoint()) {
    _lastError = F("Unable to start setup network.");
    WIFIMGR_LOG(_lastError);
    return false;
  }

  if (!_routesRegistered) {
    registerRoutes();
    _routesRegistered = true;
  }

  const IPAddress portalIp = WiFi.softAPIP();
  const bool dnsStarted = _dns.begin(_config.dnsPort, portalIp);
  _http.begin(_config.httpPort);
  _running = true;
  WIFIMGR_LOG_VALUE(F("AP portal IP: "), portalIp);
  WIFIMGR_LOG_VALUE(F("DNS started: "), dnsStarted ? F("yes") : F("no"));
  WIFIMGR_LOG_VALUE(F("HTTP port: "), _config.httpPort);
  return true;
}

void CaptivePortal::end() {
  _dns.end();
  _http.end();
  _running = false;
}

void CaptivePortal::loop() {
  if (!_running) {
    return;
  }

  processConnection();
  _dns.loop();
  _http.loop();
}

bool CaptivePortal::hasConnected() const { return _connected; }

IPAddress CaptivePortal::connectedIP() const { return _connectedIp; }

String CaptivePortal::connectedSsid() const { return _pendingSsid; }

String CaptivePortal::connectedPassword() const { return _pendingPassword; }

void CaptivePortal::registerRoutes() {
  _http.on("GET", "/", handleIndex, this);
  _http.on("GET", "/scan", handleScan, this);
  _http.on("POST", "/test", handleTest, this);
  _http.on("POST", "/connect", handleConnect, this);
  _http.on("GET", "/connecting", handleConnecting, this);
  _http.on("GET", "/testing", handleTesting, this);
  _http.onNotFound(handleIndex, this);
}

void CaptivePortal::startConnection(const String &ssid,
                                    const String &password, bool testOnly) {
  _pendingSsid = ssid;
  _pendingPassword = password;
  _testOnly = testOnly;
  _testComplete = false;
  _testSucceeded = false;
  _connectPending = true;
  _connectStarted = false;
  _connectStartedAt = 0;
  _connected = false;
  _lastError = "";
  _lastMessage = "";
}

void CaptivePortal::processConnection() {
  if (!_connectPending) {
    return;
  }

  if (!_connectStarted) {
    _connectStartedAt = millis();
    _connectStarted = true;
    WIFIMGR_LOG_VALUE(F("Trying submitted SSID: "), _pendingSsid);
    if (_pendingPassword.length() == 0) {
      WiFi.begin(_pendingSsid.c_str());
    } else {
      WiFi.begin(_pendingSsid.c_str(), _pendingPassword.c_str());
    }
  }

  if (wifiManagerStaIsUsable()) {
    _connected = true;
    _connectPending = false;
    _connectedIp = WiFi.localIP();
    WIFIMGR_LOG_VALUE(F("AP credential attempt succeeded, IP: "), _connectedIp);

    if (_testOnly) {
      _connected = false;
      _testComplete = true;
      _testSucceeded = true;
      _lastMessage = F("Credentials are valid. Test IP: ");
      _lastMessage += _connectedIp.toString();
      restoreAccessPointAfterAttempt();
      return;
    }

    _dns.end();
    if (_credentialStore != nullptr) {
      _credentialStore->save(_pendingSsid, _pendingPassword);
    }
    return;
  }

  if (millis() - _connectStartedAt >= _config.staTimeoutMs) {
    _connectPending = false;
    _connectStarted = false;
    _testComplete = _testOnly;
    _testSucceeded = false;
    _lastError = F("Connection failed or DHCP did not assign an IP address.");
    WIFIMGR_LOG_VALUE(F("Credential attempt failed; status: "), WiFi.status());
    WIFIMGR_LOG_VALUE(F("Credential attempt local IP: "), WiFi.localIP());
    restoreAccessPointAfterAttempt();
  }
}

String CaptivePortal::renderWifiPage(const String &message, bool error) {
  WIFIMGR_LOG(F("Rendering AP WiFi page"));
  String page;
  page.reserve(1400);
  appendApPageStart(page, F("Nixie Clock WiFi"));
  page += F("<h1>Nixie Clock WiFi</h1>"
            "<p>Choose a network and enter its password to connect the clock.</p>");

  if (message.length() > 0) {
    page += F("<div class=\"message");
    if (error) {
      page += F(" error");
    }
    page += F("\">");
    page += HttpServer::htmlEscape(message);
    page += F("</div>");
  }

  page += F("<form method=\"post\" action=\"/connect\">"
            "<label for=\"ssid\">WiFi network</label>");
  page += renderManualNetworkField();
  page += F("<label for=\"password\">Password</label>"
            "<input id=\"password\" name=\"password\" type=\"password\" "
            "autocomplete=\"current-password\">"
            "<button type=\"submit\" formaction=\"/test\">Test credentials</button>"
            "<button type=\"submit\">Connect</button></form>");
  page += F("<a class=\"button\" href=\"/scan\">Scan networks</a>");
  appendApPageEnd(page);
  WIFIMGR_LOG_VALUE(F("AP WiFi page bytes: "), page.length());
  return page;
}

String CaptivePortal::renderScanPage() {
  WIFIMGR_LOG(F("Rendering AP scan page"));
  String page;
  page.reserve(2600);
  appendApPageStart(page, F("Scan Networks"));
  page += F("<h1>Scan Networks</h1>"
            "<p>Select a network or enter one manually.</p>");

  WIFIMGR_LOG(F("Scanning networks while setup AP is active"));
  const int8_t networkCount = WiFi.scanNetworks();
  WIFIMGR_LOG_VALUE(F("Scan result count: "), networkCount);

  page += F("<form method=\"post\" action=\"/connect\">");
  if (networkCount > 0) {
    page += F("<table><thead><tr><th></th><th>Network</th><th>RSSI</th>"
              "<th>Security</th></tr></thead><tbody>");
    page += renderScanNetworkRows(networkCount);
    page += F("</tbody></table>");
  } else {
    page += F("<div class=\"message error\">No networks were found. Enter the "
              "network name manually.</div>");
  }

  page += F("<label for=\"ssid_text\">Manual network</label>"
            "<input id=\"ssid_text\" name=\"ssid_text\" type=\"text\" "
            "autocomplete=\"off\">"
            "<label for=\"password\">Password</label>"
            "<input id=\"password\" name=\"password\" type=\"password\" "
            "autocomplete=\"current-password\">"
            "<button type=\"submit\" formaction=\"/test\">Test credentials</button>"
            "<button type=\"submit\">Connect</button></form>"
            "<a class=\"button\" href=\"/\">Back</a>");
  appendApPageEnd(page);
  WIFIMGR_LOG_VALUE(F("AP scan page bytes: "), page.length());
  return page;
}

String CaptivePortal::renderConnectingPage() {
  String page;
  page.reserve(900);
  appendApPageStart(page, F("WiFi Connection"));

  if (_connected) {
    page += F("<h1>Connected</h1><p>Connected to ");
    page += HttpServer::htmlEscape(_pendingSsid);
    page += F(". IP address: ");
    page += _connectedIp.toString();
    page += F(".</p>");
  } else if (_connectPending) {
    page += F("<h1>Connecting</h1><p>Trying to connect to ");
    page += HttpServer::htmlEscape(_pendingSsid);
    page += F(". Refresh this page in a few seconds.</p>");
  } else {
    page += F("<h1>Connection failed</h1><p>");
    page += _lastError.length() > 0 ? HttpServer::htmlEscape(_lastError)
                                    : F("The clock could not connect.");
    page += F("</p>");
  }

  page += F("<a class=\"button\" href=\"/\">Continue</a>");
  appendApPageEnd(page);
  return page;
}

String CaptivePortal::renderTestingPage() {
  String page;
  page.reserve(1000);
  appendApPageStart(page, F("Test WiFi"));

  if (_connectPending) {
    page += F("<h1>Testing credentials</h1><p>Trying to connect to ");
    page += HttpServer::htmlEscape(_pendingSsid);
    page += F(". This setup network may briefly disconnect.</p>");
  } else if (_testComplete && _testSucceeded) {
    page += F("<h1>Credentials valid</h1><p>");
    page += HttpServer::htmlEscape(_lastMessage);
    page += F("</p><form method=\"post\" action=\"/connect\">"
              "<input type=\"hidden\" name=\"ssid\" value=\"");
    page += HttpServer::htmlEscape(_pendingSsid);
    page += F("\"><input type=\"hidden\" name=\"password\" value=\"");
    page += HttpServer::htmlEscape(_pendingPassword);
    page += F("\"><button type=\"submit\">Save and connect</button></form>");
  } else {
    page += F("<h1>Test failed</h1><p class=\"error\">");
    page += _lastError.length() > 0 ? HttpServer::htmlEscape(_lastError)
                                    : F("The clock could not connect.");
    page += F("</p>");
  }

  page += F("<a class=\"button\" href=\"/\">Back</a>");
  appendApPageEnd(page);
  return page;
}

String CaptivePortal::renderManualNetworkField() {
  String field;
  field += F("<input id=\"ssid\" name=\"ssid\" type=\"text\" "
             "autocomplete=\"off\" required>");
  field += F("<p class=\"network-meta\">Enter the network name manually.</p>");
  return field;
}

String CaptivePortal::renderScanNetworkRows(int8_t networkCount) {
  String rows;
  rows.reserve(static_cast<size_t>(networkCount) * 120);

  for (int8_t i = 0; i < networkCount; ++i) {
    const uint8_t index = static_cast<uint8_t>(i);
    const String ssid = WiFi.SSID(index);
    rows += F("<tr><td><input type=\"radio\" name=\"ssid\" value=\"");
    rows += HttpServer::htmlEscape(ssid);
    rows += F("\"></td><td>");
    rows += HttpServer::htmlEscape(ssid);
    rows += F("</td><td>");
    rows += WiFi.RSSI(index);
    rows += F("</td><td>");
    rows += encryptionLabel(WiFi.encryptionType(index));
    rows += F("</td></tr>");
  }

  return rows;
}

bool CaptivePortal::startAccessPoint() {
  WIFIMGR_LOG_VALUE(F("Starting setup AP: "), _config.apSsid);
  if (_config.apPassword != nullptr && strlen(_config.apPassword) > 0) {
    const uint8_t status = WiFi.beginAP(_config.apSsid, _config.apPassword);
    WIFIMGR_LOG_VALUE(F("beginAP status: "), status);
    return status == WL_AP_LISTENING;
  }

  const uint8_t status = WiFi.beginAP(_config.apSsid);
  WIFIMGR_LOG_VALUE(F("beginAP status: "), status);
  return status == WL_AP_LISTENING;
}

void CaptivePortal::restoreAccessPointAfterAttempt() {
  WiFi.disconnect();
  startAccessPoint();
  _dns.begin(_config.dnsPort, WiFi.softAPIP());
}

void CaptivePortal::handleIndex(const HttpRequest &request,
                                HttpResponse &response, void *context) {
  (void)request;
  CaptivePortal *portal = static_cast<CaptivePortal *>(context);
  WIFIMGR_LOG(F("Serving AP portal page"));
  response.body = portal->renderWifiPage(portal->_lastError,
                                         portal->_lastError.length() > 0);
}

void CaptivePortal::handleScan(const HttpRequest &request,
                               HttpResponse &response, void *context) {
  (void)request;
  CaptivePortal *portal = static_cast<CaptivePortal *>(context);
  WIFIMGR_LOG(F("Serving AP portal scan page"));
  response.body = portal->renderScanPage();
}

void CaptivePortal::handleConnect(const HttpRequest &request,
                                  HttpResponse &response, void *context) {
  CaptivePortal *portal = static_cast<CaptivePortal *>(context);

  String ssid = request.formValue(F("ssid_text"));
  if (ssid.length() == 0) {
    ssid = request.formValue(F("ssid"));
  }
  ssid.trim();

  if (ssid.length() == 0) {
    response.body =
        portal->renderWifiPage(F("Enter a WiFi network name."), true);
    return;
  }

  portal->startConnection(ssid, request.formValue(F("password")), false);
  response = HttpServer::redirect("/connecting");
}

void CaptivePortal::handleTest(const HttpRequest &request,
                               HttpResponse &response, void *context) {
  CaptivePortal *portal = static_cast<CaptivePortal *>(context);

  String ssid = request.formValue(F("ssid_text"));
  if (ssid.length() == 0) {
    ssid = request.formValue(F("ssid"));
  }
  ssid.trim();

  if (ssid.length() == 0) {
    response.body =
        portal->renderWifiPage(F("Enter a WiFi network name."), true);
    return;
  }

  portal->startConnection(ssid, request.formValue(F("password")), true);
  response = HttpServer::redirect("/testing");
}

void CaptivePortal::handleConnecting(const HttpRequest &request,
                                     HttpResponse &response, void *context) {
  (void)request;
  CaptivePortal *portal = static_cast<CaptivePortal *>(context);
  response.body = portal->renderConnectingPage();
  if (portal->_connectPending) {
    response.extraHeaders += F("Refresh: 3\r\n");
  }
}

void CaptivePortal::handleTesting(const HttpRequest &request,
                                  HttpResponse &response, void *context) {
  (void)request;
  CaptivePortal *portal = static_cast<CaptivePortal *>(context);
  response.body = portal->renderTestingPage();
  if (portal->_connectPending) {
    response.extraHeaders += F("Refresh: 3\r\n");
  }
}
