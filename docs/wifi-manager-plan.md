# WiFiManager Library Plan

A self-contained WiFi provisioning and configuration-portal library for the Arduino UNO R4 WiFi, replacing the current hardcoded-credentials approach in `main.cpp`.

## Goals

1. Boot using saved credentials from `Preferences`.
2. Fall back to a captive-portal AP after ~10s of failed connection.
3. Stay in AP mode until user configures WiFi successfully or power-cycles.
4. Expose a web config portal that remains available on the LAN post-connect for future settings (12/24h mode, brightness, etc.).
5. Store web assets as `PROGMEM` strings, themable via a central header.

## Design Decisions (from clarifying Q&A)

| Concern | Decision |
| ------- | -------- |
| Portal UX | AutoConnect-style captive portal (DNS hijack) |
| AP credentials | Hardcoded SSID + password (WPA2, ≥8 chars) |
| Storage | `Preferences` API with light XOR obfuscation |
| Config scope | WiFi + extensible settings (12/24h, brightness, timezone, etc.) |
| Auth | None in v1 (open on LAN) |
| Web assets | `PROGMEM` string literals in headers |
| Networking | Built-in `WiFiS3.h` (`WiFiServer` / `WiFiClient`) |
| Packaging | Local `lib/WiFiManager/` in this repo |
| STA timeout | 10 seconds before falling back to AP mode |
| AP persistence | Stays in AP mode until success or power-cycle |

## Directory Layout

Add a local PlatformIO library — PIO's LDF picks this up automatically via `lib/`:

```text
NixieClock/
  lib/
    WiFiManager/
      library.json                # PlatformIO library manifest
      src/
        WiFiManager.h             # Public API
        WiFiManager.cpp           # State machine + orchestration
        CredentialStore.h/.cpp    # Preferences-backed storage + obfuscation
        CaptivePortal.h/.cpp      # DNS hijack + AP-mode HTTP server
        ConfigPortal.h/.cpp       # STA-mode HTTP server (post-connect)
        HttpServer.h/.cpp         # Shared minimal request parser (GET/POST, form-urlencoded)
        DnsServer.h/.cpp          # UDP DNS responder for captive portal
        SettingsRegistry.h/.cpp   # Extensible key/value settings + web-form binding
      src/web/
        Pages.h                   # PROGMEM HTML templates (index, wifi, settings, result)
        Theme.h                   # PROGMEM CSS + palette constants
        Assets.h                  # Umbrella include
```

**Why a library, not `src/`:** PlatformIO's LDF handles it, keeps `main.cpp` tiny, and makes future extraction trivial.

## Public API

```cpp
class WiFiManager {
public:
  struct Config {
    const char* apSsid = "NixieClock-Setup";     // hardcoded AP SSID
    const char* apPassword = "nixie1234";        // hardcoded AP password (>=8 chars for WPA2)
    uint32_t staTimeoutMs = 10000;               // 10s STA connect timeout
    uint16_t httpPort = 80;
    uint16_t dnsPort = 53;
    const char* preferencesNamespace = "wifimgr";
  };

  void begin(const Config& cfg = {});
  void loop();                                    // must be pumped from main loop()
  bool isConnected() const;                       // true iff STA connected
  SettingsRegistry& settings();                   // for main app to register custom settings
  IPAddress localIP() const;

private:
  enum class State { Boot, StaConnecting, StaConnected, ApPortal };
  // ...
};
```

## State Machine

- **Boot** → load creds. If present → `StaConnecting`; else → `ApPortal`.
- **StaConnecting** → `WiFi.begin()`, poll `status()` up to 10s. On success → `StaConnected` (persist creds if newly submitted); on fail → `ApPortal`.
- **StaConnected** → run `ConfigPortal` HTTP server on LAN. If STA drops, retry a few times, then → `ApPortal`.
- **ApPortal** → `WiFi.beginAP()`, start DNS hijack + HTTP server. Stays here until user submits working creds. On success → persist + `StaConnected`.

## Credential Storage

- Uses existing `Preferences` API (already used by `main.cpp`).
- Namespace: `"wifimgr"` (separate from `main.cpp`'s `"nixie"` to avoid collision).
- Keys: `ssid`, `pw`, `magic` (validity marker).
- **Light obfuscation:** XOR each byte of SSID/password with a fixed key derived from a compile-time salt + byte index (`salt[i % len] ^ i`). Not security — just prevents casual flash-dump snooping. Documented as such in the header.
- Public methods: `bool load(String& ssid, String& pw)`, `void save(const String& ssid, const String& pw)`, `void clear()`.

## Captive Portal (DNS + HTTP)

**DNS hijack:** Minimal UDP responder on port 53. Answers every A-query with the AP IP (`192.168.4.1`). ~50 lines. This triggers "sign in to WiFi network" prompts on iOS/Android/Windows automatically — the AutoConnect-style UX.

**HTTP server:** Hand-rolled on `WiFiServer` / `WiFiClient` from `WiFiS3.h`. Justification: the R4 Arduino core has no bundled `WebServer` class equivalent to ESP32's, and pulling in a third-party HTTP lib means dependency risk (this is exactly what r4-wifi-manager was trying to solve). We only need GET, POST with `application/x-www-form-urlencoded`, and basic routing — a few hundred lines total.

### Routes (AP mode)

| Route | Purpose |
| ----- | ------- |
| `GET /` | SSID scan list + form (SSID dropdown/text + password field) |
| `POST /connect` | Attempt `WiFi.begin(submitted)`; on success, persist, show "connected, IP=x.x.x.x, rebooting to STA mode"; on failure, redirect back with error |
| `GET *` | Catch-all — captive-portal detection endpoints (`/generate_204`, `/hotspot-detect.html`, `/connecttest.txt`) all get the config page |

### Routes (STA mode via ConfigPortal)

| Route | Purpose |
| ----- | ------- |
| `GET /` | Status page + links |
| `GET /wifi` | Change WiFi (same form as AP) |
| `GET /settings` | Dynamic form generated from `SettingsRegistry` |
| `POST /settings` | Validate + persist via registry |
| `POST /reset` | Clear creds + reboot into AP mode |

## SettingsRegistry (extension point)

Lets `main.cpp` register settings without touching the library:

```cpp
mgr.settings().registerInt("tz_offset", "Timezone offset (hours)", -9, -12, 14);
mgr.settings().registerBool("hour24", "24-hour mode", true);
mgr.settings().registerInt("brightness", "Nixie brightness", 128, 0, 255);
```

Registry renders the settings form automatically and persists via `Preferences` under its own namespace (`"nixie-cfg"`). This is the "future extensible" hook — v1 ships the plumbing but doesn't force `main.cpp` to use it.

## Web Assets (PROGMEM)

- **`Theme.h`**: CSS variables (colors, font stack, spacing). One central palette so restyling = editing one file.
- **`Pages.h`**: HTML templates with `{{PLACEHOLDER}}` tokens. Tiny template engine (`String replaceAll(String, key, val)`) — no Mustache, just linear substitution. Templates share a `HEAD` fragment that includes the theme CSS inline.
- All strings declared as `const char PAGE_INDEX[] PROGMEM = R"HTML(...)HTML";` — cheap to add pages, easy to diff.

Estimated size: ~4–8KB of PROGMEM total for portal + config pages. Well within 256KB.

## Integration with main.cpp

`main.cpp` shrinks: remove hardcoded `ssid`/`pass`, delete `connectToWiFi()`, replace with:

```cpp
WiFiManager wifiMgr;

void setup() {
  Serial.begin(115200);
  // ...
  wifiMgr.settings().registerInt("tz_offset", "Timezone (hours)", -9, -12, 14);
  wifiMgr.begin();

  // Block until connected — keeps existing NTP flow working
  while (!wifiMgr.isConnected()) { wifiMgr.loop(); delay(10); }

  // NTP + RTC init as today
}

void loop() {
  wifiMgr.loop();   // services HTTP/DNS
  // existing clock display logic
}
```

`secrets.hpp` becomes optional — kept only for a compile-time default fallback if a build flag is set, otherwise unused.

## Cross-cutting Concerns

- **Non-blocking:** all server work is polled from `loop()`; no `delay()` inside the library. Existing 1s `delay(1000)` in `main.cpp`'s loop needs to shrink or be replaced with a millis-based tick so HTTP stays responsive (small edit to `main.cpp`).
- **Memory:** `String` used sparingly for request parsing; PROGMEM reads via `strcpy_P` / `FPSTR`. Keep request buffer bounded (~1KB) to survive on 32KB SRAM.
- **Logging:** Serial-only, gated by a `WIFIMGR_DEBUG` build flag.
- **No external deps added** to `platformio.ini` — everything uses what the R4 core + `Preferences` already provide.

## Build Order (suggested implementation sequence)

1. `CredentialStore` + unit-testable obfuscation (compiles standalone)
2. `HttpServer` (request parser, routing, PROGMEM response helpers)
3. `DnsServer` (captive-portal hijack)
4. `Pages.h` + `Theme.h` scaffolding with one working page
5. `CaptivePortal` wiring (AP mode + SSID scan + connect flow)
6. `WiFiManager` state machine + `Preferences` persistence
7. `SettingsRegistry` + dynamic settings form
8. `ConfigPortal` (STA-mode routes)
9. Integrate into `main.cpp`, retire hardcoded `secrets.hpp` path

## Open Risks / Things to Validate During Build

- **`WiFi.beginAP()` + concurrent DNS + HTTP** on R4's WiFiS3 — this is the exact ground where r4-wifi-manager stumbled. Plan to smoke-test AP + DNS + HTTP together as soon as steps 2–3 are working, before building more on top.
- **Captive-portal OS quirks:** Android/iOS use different probe URLs. Catch-all route covers most, but may need a `Location:` 302 to the portal on those specific paths — will confirm empirically.
- **`WiFi.scanNetworks()` while in AP mode** — may or may not work on WiFiS3; fallback is a manual SSID text field.
