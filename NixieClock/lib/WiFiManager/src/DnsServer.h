#ifndef NIXIE_WIFI_MANAGER_DNS_SERVER_H
#define NIXIE_WIFI_MANAGER_DNS_SERVER_H

#include <Arduino.h>
#include <WiFiS3.h>

class DnsServer {
public:
  static const size_t MaxDnsPacketSize = 512;

  DnsServer();

  bool begin(uint16_t port, const IPAddress &resolvedIp);
  void end();
  void loop();

private:
  WiFiUDP _udp;
  IPAddress _resolvedIp;
  uint16_t _port = 53;
  bool _running = false;
  uint8_t _buffer[MaxDnsPacketSize];

  int questionEndOffset(int packetSize) const;
  void writeResponse(int packetSize, int questionEnd);
};

#endif // NIXIE_WIFI_MANAGER_DNS_SERVER_H
