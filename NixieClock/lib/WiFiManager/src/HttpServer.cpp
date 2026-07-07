#include "HttpServer.h"

#include "WiFiManagerDebug.h"

namespace {

int hexValue(char value) {
  if (value >= '0' && value <= '9') {
    return value - '0';
  }
  if (value >= 'a' && value <= 'f') {
    return 10 + value - 'a';
  }
  if (value >= 'A' && value <= 'F') {
    return 10 + value - 'A';
  }
  return -1;
}

String statusBody(uint16_t statusCode, const char *statusText) {
  String body = F("<!doctype html><html><head><meta charset=\"utf-8\"><title>");
  body += statusCode;
  body += ' ';
  body += statusText;
  body += F("</title></head><body><h1>");
  body += statusCode;
  body += ' ';
  body += statusText;
  body += F("</h1></body></html>");
  return body;
}

bool splitRequestTarget(const String &target, String &path, String &query) {
  if (target.length() == 0 || target[0] != '/') {
    return false;
  }

  const int queryStart = target.indexOf('?');
  if (queryStart < 0) {
    path = target;
    query = "";
    return true;
  }

  path = target.substring(0, queryStart);
  query = target.substring(queryStart + 1);
  return true;
}

String formValueFromBody(const String &body, const String &key) {
  int offset = 0;
  while (offset <= body.length()) {
    int next = body.indexOf('&', offset);
    if (next < 0) {
      next = body.length();
    }

    const String pair = body.substring(offset, next);
    const int equals = pair.indexOf('=');
    const String rawKey = equals < 0 ? pair : pair.substring(0, equals);
    if (HttpServer::urlDecode(rawKey) == key) {
      const String rawValue = equals < 0 ? "" : pair.substring(equals + 1);
      return HttpServer::urlDecode(rawValue);
    }

    if (next == body.length()) {
      break;
    }
    offset = next + 1;
  }

  return "";
}

} // namespace

String HttpRequest::formValue(const String &key) const {
  return formValueFromBody(body, key);
}

HttpServer::HttpServer() : _server(80) {
  _requestBuffer.reserve(MaxRequestBytes);
}

void HttpServer::begin(uint16_t port) {
  _port = port;
  _server.begin(static_cast<int>(_port));
  _running = true;
  WIFIMGR_LOG_VALUE(F("HTTP server listening on port "), _port);
}

void HttpServer::end() {
  resetClient();
  _server.end();
  _running = false;
  WIFIMGR_LOG(F("HTTP server stopped"));
}

void HttpServer::loop() {
  if (!_running) {
    return;
  }

  if (!_hasClient) {
    WiFiClient nextClient = _server.available();
    if (nextClient) {
      _client = nextClient;
      _requestBuffer = "";
      _clientStartedAt = millis();
      _hasClient = true;
      WIFIMGR_LOG(F("HTTP client accepted"));
    }
  }

  if (!_hasClient) {
    return;
  }

  while (_client.available() > 0) {
    const int next = _client.read();
    if (next < 0) {
      break;
    }

    if (_requestBuffer.length() >= MaxRequestBytes) {
      sendError(413, "Payload Too Large");
      resetClient();
      return;
    }

    _requestBuffer += static_cast<char>(next);
  }

  size_t headerEnd = 0;
  size_t contentLength = 0;
  if (requestIsComplete(headerEnd, contentLength)) {
    HttpRequest request;
    HttpResponse response;
    if (!parseRequest(request)) {
      WIFIMGR_LOG(F("HTTP bad request"));
      sendError(400, "Bad Request");
      resetClient();
      return;
    }

    WIFIMGR_LOG_VALUE(F("HTTP request: "), request.method + F(" ") + request.path);
    dispatch(request, response);
    sendResponse(response);
    resetClient();
    return;
  }

  if (millis() - _clientStartedAt > ClientTimeoutMs || !_client.connected()) {
    WIFIMGR_LOG(F("HTTP request timeout"));
    sendError(408, "Request Timeout");
    resetClient();
  }
}

bool HttpServer::on(const char *method, const char *path,
                    HttpRouteHandler handler, void *context) {
  if (_routeCount >= MaxRoutes || method == nullptr || path == nullptr ||
      handler == nullptr) {
    return false;
  }

  _routes[_routeCount].method = method;
  _routes[_routeCount].path = path;
  _routes[_routeCount].handler = handler;
  _routes[_routeCount].context = context;
  ++_routeCount;
  return true;
}

void HttpServer::onNotFound(HttpRouteHandler handler, void *context) {
  _notFoundHandler = handler;
  _notFoundContext = context;
}

String HttpServer::urlDecode(const String &value) {
  String decoded;
  decoded.reserve(value.length());

  for (size_t i = 0; i < value.length(); ++i) {
    const char current = value[i];
    if (current == '+') {
      decoded += ' ';
      continue;
    }

    if (current == '%' && i + 2 < value.length()) {
      const int high = hexValue(value[i + 1]);
      const int low = hexValue(value[i + 2]);
      if (high >= 0 && low >= 0) {
        decoded += static_cast<char>((high << 4) | low);
        i += 2;
        continue;
      }
    }

    decoded += current;
  }

  return decoded;
}

String HttpServer::htmlEscape(const String &value) {
  String escaped;
  escaped.reserve(value.length());

  for (size_t i = 0; i < value.length(); ++i) {
    switch (value[i]) {
    case '&':
      escaped += F("&amp;");
      break;
    case '<':
      escaped += F("&lt;");
      break;
    case '>':
      escaped += F("&gt;");
      break;
    case '"':
      escaped += F("&quot;");
      break;
    case '\'':
      escaped += F("&#39;");
      break;
    default:
      escaped += value[i];
      break;
    }
  }

  return escaped;
}

HttpResponse HttpServer::redirect(const char *location) {
  HttpResponse response;
  response.statusCode = 302;
  response.statusText = "Found";
  response.body = statusBody(302, "Found");
  response.extraHeaders = F("Location: ");
  response.extraHeaders += location;
  response.extraHeaders += F("\r\n");
  return response;
}

void HttpServer::resetClient() {
  if (_hasClient) {
    _client.flush();
    _client.stop();
  }
  _requestBuffer = "";
  _hasClient = false;
}

bool HttpServer::requestIsComplete(size_t &headerEnd,
                                   size_t &contentLength) const {
  const int marker = _requestBuffer.indexOf(F("\r\n\r\n"));
  if (marker < 0) {
    return false;
  }

  headerEnd = static_cast<size_t>(marker + 4);
  contentLength = 0;

  int lineStart = 0;
  while (lineStart < marker) {
    int lineEnd = _requestBuffer.indexOf(F("\r\n"), lineStart);
    if (lineEnd < 0 || lineEnd > marker) {
      lineEnd = marker;
    }

    String line = _requestBuffer.substring(lineStart, lineEnd);
    line.trim();
    if (line.startsWith(F("Content-Length:")) ||
        line.startsWith(F("content-length:"))) {
      line = line.substring(line.indexOf(':') + 1);
      line.trim();
      contentLength = static_cast<size_t>(line.toInt());
      break;
    }

    lineStart = lineEnd + 2;
  }

  return _requestBuffer.length() >= headerEnd + contentLength;
}

bool HttpServer::parseRequest(HttpRequest &request) const {
  const int lineEnd = _requestBuffer.indexOf(F("\r\n"));
  if (lineEnd <= 0) {
    return false;
  }

  const String requestLine = _requestBuffer.substring(0, lineEnd);
  const int firstSpace = requestLine.indexOf(' ');
  const int secondSpace = requestLine.indexOf(' ', firstSpace + 1);
  if (firstSpace <= 0 || secondSpace <= firstSpace) {
    return false;
  }

  request.method = requestLine.substring(0, firstSpace);

  const String target = requestLine.substring(firstSpace + 1, secondSpace);
  if (!splitRequestTarget(target, request.path, request.query)) {
    return false;
  }

  size_t headerEnd = 0;
  size_t contentLength = 0;
  if (!requestIsComplete(headerEnd, contentLength)) {
    return false;
  }

  request.body = _requestBuffer.substring(static_cast<int>(headerEnd),
                                          static_cast<int>(headerEnd + contentLength));
  return true;
}

void HttpServer::dispatch(const HttpRequest &request, HttpResponse &response) {
  for (uint8_t i = 0; i < _routeCount; ++i) {
    if (request.method == _routes[i].method && request.path == _routes[i].path) {
      _routes[i].handler(request, response, _routes[i].context);
      return;
    }
  }

  if (_notFoundHandler != nullptr) {
    _notFoundHandler(request, response, _notFoundContext);
    return;
  }

  response.statusCode = 404;
  response.statusText = "Not Found";
  response.body = statusBody(404, "Not Found");
}

void HttpServer::sendResponse(const HttpResponse &response) {
  WIFIMGR_LOG_VALUE(F("HTTP response: "), response.statusCode);
  _client.print(F("HTTP/1.1 "));
  _client.print(response.statusCode);
  _client.print(' ');
  _client.print(response.statusText);
  _client.print(F("\r\nContent-Type: "));
  _client.print(response.contentType);
  _client.print(F("\r\nContent-Length: "));
  _client.print(response.body.length());
  _client.print(F("\r\nConnection: close\r\n"));
  _client.print(response.extraHeaders);
  _client.print(F("\r\n"));
  _client.print(response.body);
}

void HttpServer::sendError(uint16_t statusCode, const char *statusText) {
  HttpResponse response;
  response.statusCode = statusCode;
  response.statusText = statusText;
  response.body = statusBody(statusCode, statusText);
  sendResponse(response);
}
