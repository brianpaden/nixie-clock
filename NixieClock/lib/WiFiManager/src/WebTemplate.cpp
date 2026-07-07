#include "WebTemplate.h"

#include "web/Assets.h"

String wifiManagerStringFromProgmem(const char *value) {
  return String(reinterpret_cast<const __FlashStringHelper *>(value));
}

void wifiManagerReplaceAll(String &subject, const String &token,
                           const String &value) {
  int offset = subject.indexOf(token);
  while (offset >= 0) {
    subject = subject.substring(0, offset) + value +
              subject.substring(offset + token.length());
    offset = subject.indexOf(token, offset + value.length());
  }
}

String wifiManagerRenderShell(const String &title, const String &body) {
  String page =
      wifiManagerStringFromProgmem(NixieWiFiManagerWeb::PAGE_SHELL);
  wifiManagerReplaceAll(page, F("{{TITLE}}"), title);
  wifiManagerReplaceAll(page, F("{{CSS}}"),
                        wifiManagerStringFromProgmem(
                            NixieWiFiManagerWeb::THEME_CSS));
  wifiManagerReplaceAll(page, F("{{BODY}}"), body);
  return page;
}
