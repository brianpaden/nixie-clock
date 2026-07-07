#include "SettingsRegistry.h"

SettingsRegistry::SettingsRegistry(const char *preferencesNamespace)
    : _preferencesNamespace(preferencesNamespace) {}

bool SettingsRegistry::registerInt(const char *key, const char *label,
                                   int defaultValue, int minValue,
                                   int maxValue) {
  if (_settingCount >= MaxSettings || key == nullptr || label == nullptr ||
      minValue > maxValue) {
    return false;
  }

  Setting &setting = _settings[_settingCount++];
  setting.key = key;
  setting.label = label;
  setting.type = Type::Integer;
  setting.defaultValue = defaultValue;
  setting.minValue = minValue;
  setting.maxValue = maxValue;
  return true;
}

bool SettingsRegistry::registerBool(const char *key, const char *label,
                                    bool defaultValue) {
  if (_settingCount >= MaxSettings || key == nullptr || label == nullptr) {
    return false;
  }

  Setting &setting = _settings[_settingCount++];
  setting.key = key;
  setting.label = label;
  setting.type = Type::Boolean;
  setting.defaultValue = defaultValue ? 1 : 0;
  setting.minValue = 0;
  setting.maxValue = 1;
  return true;
}

int SettingsRegistry::getInt(const char *key) const {
  const Setting *setting = find(key);
  if (setting == nullptr) {
    return 0;
  }

  Preferences preferences;
  if (!preferences.begin(_preferencesNamespace, true)) {
    return setting->defaultValue;
  }
  const int value = preferences.getInt(key, setting->defaultValue);
  preferences.end();
  return value;
}

bool SettingsRegistry::getBool(const char *key) const {
  const Setting *setting = find(key);
  if (setting == nullptr) {
    return false;
  }

  Preferences preferences;
  if (!preferences.begin(_preferencesNamespace, true)) {
    return setting->defaultValue != 0;
  }
  const bool value = preferences.getBool(key, setting->defaultValue != 0);
  preferences.end();
  return value;
}

bool SettingsRegistry::updateFromRequest(const HttpRequest &request,
                                         String &errorMessage) {
  Preferences preferences;
  if (!preferences.begin(_preferencesNamespace, false)) {
    errorMessage = F("Unable to open settings storage.");
    return false;
  }

  for (uint8_t i = 0; i < _settingCount; ++i) {
    const Setting &setting = _settings[i];
    if (setting.type == Type::Boolean) {
      const bool value = request.formValue(setting.key) == F("1");
      preferences.putBool(setting.key, value);
      continue;
    }

    const String rawValue = request.formValue(setting.key);
    if (rawValue.length() == 0) {
      preferences.end();
      errorMessage = F("Missing value for ");
      errorMessage += setting.label;
      errorMessage += '.';
      return false;
    }

    const int value = rawValue.toInt();
    if (value < setting.minValue || value > setting.maxValue) {
      preferences.end();
      errorMessage = setting.label;
      errorMessage += F(" must be between ");
      errorMessage += setting.minValue;
      errorMessage += F(" and ");
      errorMessage += setting.maxValue;
      errorMessage += '.';
      return false;
    }

    preferences.putInt(setting.key, value);
  }

  preferences.end();
  return true;
}

String SettingsRegistry::renderFormHtml() const {
  String html;
  html.reserve(256);
  html += F("<form method=\"post\" action=\"/settings\">");

  for (uint8_t i = 0; i < _settingCount; ++i) {
    const Setting &setting = _settings[i];
    html += F("<label for=\"");
    html += setting.key;
    html += F("\">");
    html += HttpServer::htmlEscape(setting.label);
    html += F("</label>");

    if (setting.type == Type::Boolean) {
      html += F("<select id=\"");
      html += setting.key;
      html += F("\" name=\"");
      html += setting.key;
      html += F("\">");
      const bool value = getBool(setting.key);
      html += F("<option value=\"1\"");
      if (value) {
        html += F(" selected");
      }
      html += F(">On</option><option value=\"0\"");
      if (!value) {
        html += F(" selected");
      }
      html += F(">Off</option></select>");
      continue;
    }

    html += F("<input id=\"");
    html += setting.key;
    html += F("\" name=\"");
    html += setting.key;
    html += F("\" type=\"number\" min=\"");
    html += setting.minValue;
    html += F("\" max=\"");
    html += setting.maxValue;
    html += F("\" value=\"");
    html += getInt(setting.key);
    html += F("\">");
  }

  html += F("<button type=\"submit\">Save</button></form>");
  return html;
}

const SettingsRegistry::Setting *SettingsRegistry::find(const char *key) const {
  if (key == nullptr) {
    return nullptr;
  }

  for (uint8_t i = 0; i < _settingCount; ++i) {
    if (strcmp(_settings[i].key, key) == 0) {
      return &_settings[i];
    }
  }

  return nullptr;
}
