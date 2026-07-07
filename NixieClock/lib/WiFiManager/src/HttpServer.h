#ifndef NIXIE_WIFI_MANAGER_HTTP_SERVER_H
#define NIXIE_WIFI_MANAGER_HTTP_SERVER_H

#include <Arduino.h>
#include <WiFiS3.h>

struct HttpRequest {
  String method;
  String path;
  String query;
  String body;

  String formValue(const String &key) const;
};

struct HttpResponse {
  uint16_t statusCode = 200;
  const char *statusText = "OK";
  const char *contentType = "text/html; charset=utf-8";
  String body;
  String extraHeaders;
};

typedef void (*HttpRouteHandler)(const HttpRequest &request,
                                 HttpResponse &response, void *context);

class HttpServer {
public:
  static const uint8_t MaxRoutes = 10;
  static const size_t MaxRequestBytes = 1024;
  static const uint32_t ClientTimeoutMs = 2000;

  HttpServer();

  void begin(uint16_t port);
  void end();
  void loop();

  bool on(const char *method, const char *path, HttpRouteHandler handler,
          void *context = nullptr);
  void onNotFound(HttpRouteHandler handler, void *context = nullptr);

  static String urlDecode(const String &value);
  static String htmlEscape(const String &value);
  static HttpResponse redirect(const char *location);

private:
  struct Route {
    const char *method = nullptr;
    const char *path = nullptr;
    HttpRouteHandler handler = nullptr;
    void *context = nullptr;
  };

  WiFiServer _server;
  WiFiClient _client;
  String _requestBuffer;
  uint32_t _clientStartedAt = 0;
  uint16_t _port = 80;
  bool _hasClient = false;
  bool _running = false;
  Route _routes[MaxRoutes];
  uint8_t _routeCount = 0;
  HttpRouteHandler _notFoundHandler = nullptr;
  void *_notFoundContext = nullptr;

  void resetClient();
  bool requestIsComplete(size_t &headerEnd, size_t &contentLength) const;
  bool parseRequest(HttpRequest &request) const;
  void dispatch(const HttpRequest &request, HttpResponse &response);
  void sendResponse(const HttpResponse &response);
  void sendError(uint16_t statusCode, const char *statusText);
};

#endif // NIXIE_WIFI_MANAGER_HTTP_SERVER_H
