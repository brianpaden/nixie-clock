#ifndef NIXIE_WIFI_MANAGER_WEB_PAGES_H
#define NIXIE_WIFI_MANAGER_WEB_PAGES_H

#include <Arduino.h>

namespace NixieWiFiManagerWeb {

const char PAGE_SHELL[] PROGMEM = R"HTML(<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>{{TITLE}}</title>
  <style>{{CSS}}</style>
</head>
<body>
  <main>{{BODY}}</main>
</body>
</html>)HTML";

const char WIFI_FORM_BODY[] PROGMEM = R"HTML(
<h1>{{HEADING}}</h1>
<p>{{INTRO}}</p>
{{MESSAGE}}
<form method="post" action="/connect">
  <label for="ssid">WiFi network</label>
  {{NETWORK_FIELD}}
  <label for="password">Password</label>
  <input id="password" name="password" type="password" autocomplete="current-password">
  <button type="submit">Connect</button>
</form>)HTML";

const char STATUS_BODY[] PROGMEM = R"HTML(
<h1>{{HEADING}}</h1>
<p>{{MESSAGE}}</p>
<div class="actions">
  <a class="button" href="/wifi">Change WiFi</a>
  <a class="button" href="/settings">Settings</a>
</div>)HTML";

const char RESULT_BODY[] PROGMEM = R"HTML(
<h1>{{HEADING}}</h1>
<p>{{MESSAGE}}</p>
<div class="actions">
  <a class="button" href="/">Continue</a>
</div>)HTML";

} // namespace NixieWiFiManagerWeb

#endif // NIXIE_WIFI_MANAGER_WEB_PAGES_H
