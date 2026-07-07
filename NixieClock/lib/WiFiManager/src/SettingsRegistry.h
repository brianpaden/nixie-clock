#ifndef NIXIE_WIFI_MANAGER_SETTINGS_REGISTRY_H
#define NIXIE_WIFI_MANAGER_SETTINGS_REGISTRY_H

#include <Arduino.h>
#include <Preferences.h>

#include "HttpServer.h"

class SettingsRegistry {
public:
  static const uint8_t MaxSettings = 12;

  explicit SettingsRegistry(const char *preferencesNamespace = "nixie-cfg");

  bool registerInt(const char *key, const char *label, int defaultValue,
                   int minValue, int maxValue);
  bool registerBool(const char *key, const char *label, bool defaultValue);

  int getInt(const char *key) const;
  bool getBool(const char *key) const;
  bool updateFromRequest(const HttpRequest &request, String &errorMessage);
  String renderFormHtml() const;

private:
  enum class Type : uint8_t { Integer, Boolean };

  struct Setting {
    const char *key = nullptr;
    const char *label = nullptr;
    Type type = Type::Integer;
    int defaultValue = 0;
    int minValue = 0;
    int maxValue = 0;
  };

  const char *_preferencesNamespace;
  Setting _settings[MaxSettings];
  uint8_t _settingCount = 0;

  const Setting *find(const char *key) const;
};

#endif // NIXIE_WIFI_MANAGER_SETTINGS_REGISTRY_H
