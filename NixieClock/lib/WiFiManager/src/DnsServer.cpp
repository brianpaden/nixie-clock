#include "DnsServer.h"

#include "WiFiManagerDebug.h"

namespace {

const uint8_t kDnsHeaderSize = 12;
const uint16_t kTypeA = 0x0001;
const uint16_t kClassIn = 0x0001;
const uint32_t kTtlSeconds = 60;

void write16(WiFiUDP &udp, uint16_t value) {
  udp.write(static_cast<uint8_t>((value >> 8) & 0xFF));
  udp.write(static_cast<uint8_t>(value & 0xFF));
}

void write32(WiFiUDP &udp, uint32_t value) {
  udp.write(static_cast<uint8_t>((value >> 24) & 0xFF));
  udp.write(static_cast<uint8_t>((value >> 16) & 0xFF));
  udp.write(static_cast<uint8_t>((value >> 8) & 0xFF));
  udp.write(static_cast<uint8_t>(value & 0xFF));
}

uint16_t read16(const uint8_t *buffer, size_t offset) {
  return static_cast<uint16_t>((buffer[offset] << 8) | buffer[offset + 1]);
}

} // namespace

DnsServer::DnsServer() : _resolvedIp(192, 168, 4, 1) {}

bool DnsServer::begin(uint16_t port, const IPAddress &resolvedIp) {
  _port = port;
  _resolvedIp = resolvedIp;
  _running = _udp.begin(_port) == 1;
  WIFIMGR_LOG_VALUE(F("DNS server port: "), _port);
  WIFIMGR_LOG_VALUE(F("DNS resolves to: "), _resolvedIp);
  WIFIMGR_LOG_VALUE(F("DNS started: "), _running ? F("yes") : F("no"));
  return _running;
}

void DnsServer::end() {
  _udp.stop();
  _running = false;
  WIFIMGR_LOG(F("DNS server stopped"));
}

void DnsServer::loop() {
  if (!_running) {
    return;
  }

  const int packetSize = _udp.parsePacket();
  if (packetSize <= 0) {
    return;
  }

  if (packetSize > static_cast<int>(MaxDnsPacketSize)) {
    _udp.flush();
    return;
  }

  const int bytesRead = _udp.read(_buffer, static_cast<size_t>(packetSize));
  if (bytesRead != packetSize) {
    _udp.flush();
    return;
  }

  const int questionEnd = questionEndOffset(packetSize);
  if (questionEnd < 0) {
    WIFIMGR_LOG(F("DNS packet ignored"));
    return;
  }

  WIFIMGR_LOG(F("DNS captive response"));
  writeResponse(packetSize, questionEnd);
}

int DnsServer::questionEndOffset(int packetSize) const {
  if (packetSize < static_cast<int>(kDnsHeaderSize + 5)) {
    return -1;
  }

  const uint16_t questionCount = read16(_buffer, 4);
  if (questionCount == 0) {
    return -1;
  }

  int offset = kDnsHeaderSize;
  while (offset < packetSize) {
    const uint8_t labelLength = _buffer[offset++];
    if (labelLength == 0) {
      break;
    }

    if ((labelLength & 0xC0) != 0 || offset + labelLength >= packetSize) {
      return -1;
    }
    offset += labelLength;
  }

  if (offset + 4 > packetSize) {
    return -1;
  }

  const uint16_t questionType = read16(_buffer, static_cast<size_t>(offset));
  const uint16_t questionClass =
      read16(_buffer, static_cast<size_t>(offset + 2));
  if (questionType != kTypeA || questionClass != kClassIn) {
    return -1;
  }

  return offset + 4;
}

void DnsServer::writeResponse(int packetSize, int questionEnd) {
  (void)packetSize;

  _udp.beginPacket(_udp.remoteIP(), _udp.remotePort());

  _udp.write(_buffer[0]);
  _udp.write(_buffer[1]);
  write16(_udp, 0x8180);
  write16(_udp, 1);
  write16(_udp, 1);
  write16(_udp, 0);
  write16(_udp, 0);

  _udp.write(_buffer + kDnsHeaderSize,
             static_cast<size_t>(questionEnd - kDnsHeaderSize));

  write16(_udp, 0xC00C);
  write16(_udp, kTypeA);
  write16(_udp, kClassIn);
  write32(_udp, kTtlSeconds);
  write16(_udp, 4);
  _udp.write(_resolvedIp[0]);
  _udp.write(_resolvedIp[1]);
  _udp.write(_resolvedIp[2]);
  _udp.write(_resolvedIp[3]);

  _udp.endPacket();
}
