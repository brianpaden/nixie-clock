#ifndef NIXIE_WIFI_MANAGER_CREDENTIAL_STORE_H
#define NIXIE_WIFI_MANAGER_CREDENTIAL_STORE_H

#include <Arduino.h>
#include <Preferences.h>

namespace NixieWiFiManagerDetail {

// Light obfuscation only. This deters casual flash dumps; it is not encryption.
String obfuscateToHex(const String &plainText);
bool deobfuscateFromHex(const String &encoded, String &plainText);

} // namespace NixieWiFiManagerDetail

class CredentialStore {
public:
  explicit CredentialStore(const char *preferencesNamespace = "wifimgr");

  void setNamespace(const char *preferencesNamespace);
  bool load(String &ssid, String &password);
  bool save(const String &ssid, const String &password);
  void clear();

private:
  const char *_preferencesNamespace;
};

#endif // NIXIE_WIFI_MANAGER_CREDENTIAL_STORE_H
