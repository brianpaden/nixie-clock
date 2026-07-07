#ifndef NIXIE_WIFI_MANAGER_WEB_THEME_H
#define NIXIE_WIFI_MANAGER_WEB_THEME_H

#include <Arduino.h>

namespace NixieWiFiManagerWeb {

const char THEME_CSS[] PROGMEM = R"CSS(
:root {
  color-scheme: light;
  --bg: #f6f7f9;
  --panel: #ffffff;
  --text: #17202a;
  --muted: #5d6875;
  --border: #d7dde5;
  --accent: #0f766e;
  --accent-strong: #0b5f59;
  --danger: #b42318;
  --shadow: 0 10px 28px rgba(23, 32, 42, 0.12);
  font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
}
* { box-sizing: border-box; }
body {
  margin: 0;
  min-height: 100vh;
  background: var(--bg);
  color: var(--text);
}
main {
  width: min(680px, calc(100vw - 32px));
  margin: 32px auto;
  background: var(--panel);
  border: 1px solid var(--border);
  border-radius: 8px;
  box-shadow: var(--shadow);
  padding: 24px;
}
h1 { margin: 0 0 8px; font-size: 1.65rem; line-height: 1.2; }
p { color: var(--muted); line-height: 1.45; }
label { display: block; margin-top: 16px; font-weight: 650; }
input, select {
  width: 100%;
  margin-top: 6px;
  padding: 11px 12px;
  border: 1px solid var(--border);
  border-radius: 6px;
  font: inherit;
}
button, .button {
  display: inline-block;
  margin-top: 20px;
  padding: 11px 16px;
  border: 0;
  border-radius: 6px;
  background: var(--accent);
  color: white;
  font: inherit;
  font-weight: 700;
  text-decoration: none;
}
button:hover, .button:hover { background: var(--accent-strong); }
.message {
  margin: 16px 0 0;
  padding: 12px;
  border-radius: 6px;
  border: 1px solid var(--border);
  background: #eef6f5;
}
.error { color: var(--danger); }
.network-meta { color: var(--muted); font-size: 0.88rem; }
.actions { display: flex; gap: 12px; flex-wrap: wrap; }
)CSS";

} // namespace NixieWiFiManagerWeb

#endif // NIXIE_WIFI_MANAGER_WEB_THEME_H
