# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Arduino firmware for a Nixie tube clock running on an **Arduino UNO R4 WiFi**. The device connects to WiFi, syncs time via NTP, maintains time with a DS3231 RTC module, and will eventually drive Nixie tube displays. Hardware schematics are in KiCad.

## Build System: PlatformIO

All firmware work is inside `NixieClock/`. PlatformIO manages dependencies, compilation, and upload.

```bash
# Build
pio run -e uno_r4_wifi

# Build and upload to board
pio run -e uno_r4_wifi --target upload

# Monitor serial output (115200 baud)
pio device monitor

# Clean build artifacts
pio run -e uno_r4_wifi --target clean
```

The VS Code PlatformIO extension is the primary IDE integration (`NixieClock/.vscode/extensions.json`).

## Architecture

- `NixieClock/src/main.cpp` — all firmware logic (setup/loop)
- `NixieClock/include/secrets.hpp` — WiFi credentials (`SECRET_SSID`, `SECRET_PASS`); excluded from git
- `NixieClock/platformio.ini` — board target (`uno_r4_wifi`), framework (`arduino`), and library dependencies
- `Schematic/Nixie/` — KiCad project with schematic, PCB, and custom symbol libraries

### Firmware Flow

1. **setup()**: Serial → Preferences → WiFi → RTC init → NTP sync (up to 10 retries) → set RTC from NTP
2. **loop()**: Read RTC every second, print `YYYY-MM-DD HH:MM:SS - Temp: XX.XX°C` to serial

### Key Dependencies

| Library | Purpose |
|---|---|
| `arduino-libraries/NTPClient@^3.2.1` | NTP time sync over WiFi |
| `adafruit/RTClib@^2.1.4` | DS3231 RTC over I2C |
| `bblanchon/ArduinoJson@^7.2.2` | JSON (future use) |
| `adafruit/Adafruit GFX Library@^1.12.6` | Display graphics (future use) |

### Error Handling

Fatal errors call `panic(message)` which prints to serial and flashes the onboard LED indefinitely — the device must be power-cycled.

## Configuration

- **Timezone offset** is hardcoded in `main.cpp` (currently `-9` for Alaska). Change this constant when deploying in a different timezone.
- **WiFi credentials** go in `NixieClock/include/secrets.hpp` — this file is gitignored and must be created locally.

## Hardware

- **MCU:** Arduino UNO R4 WiFi (Renesas RA4M1 + ESP32-S3 for WiFi)
- **RTC:** DS3231 via I2C
- **Display:** Nixie tubes (driver circuit under schematic design)
- **Schematic tool:** KiCad — open `Schematic/Nixie/Nixie.kicad_pro`
