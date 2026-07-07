#include "CredentialStore.h"

namespace {

const char *const kSsidKey = "ssid";
const char *const kPasswordKey = "pw";
const char *const kMagicKey = "magic";
const char *const kMagicValue = "v1";
const char *const kSalt = "NixieClockWiFi";
const size_t kSaltLength = 14;

char nibbleToHex(uint8_t value) {
  value &= 0x0F;
  return value < 10 ? static_cast<char>('0' + value)
                    : static_cast<char>('A' + (value - 10));
}

bool hexToNibble(char value, uint8_t &nibble) {
  if (value >= '0' && value <= '9') {
    nibble = static_cast<uint8_t>(value - '0');
    return true;
  }
  if (value >= 'a' && value <= 'f') {
    nibble = static_cast<uint8_t>(10 + value - 'a');
    return true;
  }
  if (value >= 'A' && value <= 'F') {
    nibble = static_cast<uint8_t>(10 + value - 'A');
    return true;
  }
  return false;
}

uint8_t obfuscationByte(size_t index) {
  return static_cast<uint8_t>(kSalt[index % kSaltLength]) ^
         static_cast<uint8_t>(index & 0xFF);
}

} // namespace

namespace NixieWiFiManagerDetail {

String obfuscateToHex(const String &plainText) {
  String encoded;
  encoded.reserve(plainText.length() * 2);

  for (size_t i = 0; i < plainText.length(); ++i) {
    const uint8_t value =
        static_cast<uint8_t>(plainText[i]) ^ obfuscationByte(i);
    encoded += nibbleToHex(value >> 4);
    encoded += nibbleToHex(value);
  }

  return encoded;
}

bool deobfuscateFromHex(const String &encoded, String &plainText) {
  if ((encoded.length() % 2) != 0) {
    return false;
  }

  String decoded;
  decoded.reserve(encoded.length() / 2);

  for (size_t i = 0; i < encoded.length(); i += 2) {
    uint8_t high = 0;
    uint8_t low = 0;
    if (!hexToNibble(encoded[i], high) || !hexToNibble(encoded[i + 1], low)) {
      return false;
    }

    const uint8_t obfuscated = static_cast<uint8_t>((high << 4) | low);
    const uint8_t plain =
        obfuscated ^ obfuscationByte(static_cast<size_t>(i / 2));
    decoded += static_cast<char>(plain);
  }

  plainText = decoded;
  return true;
}

} // namespace NixieWiFiManagerDetail

CredentialStore::CredentialStore(const char *preferencesNamespace)
    : _preferencesNamespace(preferencesNamespace) {}

void CredentialStore::setNamespace(const char *preferencesNamespace) {
  _preferencesNamespace = preferencesNamespace;
}

bool CredentialStore::load(String &ssid, String &password) {
  Preferences preferences;
  if (!preferences.begin(_preferencesNamespace, true)) {
    return false;
  }

  const String magic = preferences.getString(kMagicKey, "");
  const String encodedSsid = preferences.getString(kSsidKey, "");
  const String encodedPassword = preferences.getString(kPasswordKey, "");
  preferences.end();

  if (magic != kMagicValue || encodedSsid.length() == 0) {
    return false;
  }

  String decodedSsid;
  String decodedPassword;
  if (!NixieWiFiManagerDetail::deobfuscateFromHex(encodedSsid, decodedSsid) ||
      !NixieWiFiManagerDetail::deobfuscateFromHex(encodedPassword,
                                                  decodedPassword)) {
    return false;
  }

  ssid = decodedSsid;
  password = decodedPassword;
  return true;
}

bool CredentialStore::save(const String &ssid, const String &password) {
  Preferences preferences;
  if (!preferences.begin(_preferencesNamespace, false)) {
    return false;
  }

  const size_t ssidWritten =
      preferences.putString(kSsidKey, NixieWiFiManagerDetail::obfuscateToHex(ssid));
  const size_t passwordWritten = preferences.putString(
      kPasswordKey, NixieWiFiManagerDetail::obfuscateToHex(password));
  const size_t magicWritten = preferences.putString(kMagicKey, kMagicValue);
  preferences.end();

  return ssidWritten > 0 && magicWritten > 0 &&
         (password.length() == 0 || passwordWritten > 0);
}

void CredentialStore::clear() {
  Preferences preferences;
  if (!preferences.begin(_preferencesNamespace, false)) {
    return;
  }

  preferences.remove(kSsidKey);
  preferences.remove(kPasswordKey);
  preferences.remove(kMagicKey);
  preferences.end();
}
