# M17-E-paper-dashboard: ESP32 4-color GDEM075F52 7.5" E-Paper Dashboard (800×480, ACeP)

This repository contains a ESP32 firmware for driving the 4-color GDEM075F52 7.5" 4-color ACeP e-paper display and exposing it as a network-driven dashboard endpoint with HomeAssistant for a personal DIY home dashboard. Strings for output are in Latvian (remember, personal DIY project). Language/proper UTF-8 support might come later if I am bored. Also yaml configurable json data structures, panels might come. Feel free to fork and improve. Icons TO-DO.

---

## Firmware

- Wi-Fi–connected
- Receives JSON snapshots over HTTP (from Home Assistant or any client)
- Renders a full 800×480 dashboard on an ACeP e-paper panel
- Returns to deep sleep to preserve power and panel lifespan
- Includes a live HTML dashboard for debugging and inspection
- Fixed 15min deep sleep/polling cycle. Expects REST updates coming in at 00, 15, 30, 45 minutes.
- Supports only the specific GoodDisplay 4-color 7.5" e-paper screen (GDEM075F52)
---

## Hardware

MCU: ESP32-D0WD (PSRAM strongly recommended)  
Display: GDEM075F52, 7.5", 800×480, 4-color ACeP

Pin connections:
- CS: GPIO 27
- DC: GPIO 14
- RST: GPIO 12
- BUSY: GPIO 13
- CLK: GPIO 23
- MOSI: GPIO 18

MISO is not used (write-only panel).

---

## Firmware Architecture

Deep sleep → Wake on timer → Wi-Fi connect → Wait for HTTP POST (/update)
→ Parse JSON → Render framebuffer → SPI flush to e-paper
→ Panel sleep → ESP32 deep sleep

---

## Features

- Full 800×480 framebuffer rendering
- Manual SPI + CS timing (panel-safe)
- UTF-8 Latvian glyph support
- JSON REST update endpoint
- Live HTML dashboard (when not sleeping)
- Deep sleep power management
- Designed for Home Assistant integration

---

## Build and Flash

This project supports generating Wi‑Fi secrets and language configuration at build time from a local `.env` file or environment variables.

1. Create a `.env` file at the project root (ignored by git) or export the variables in your shell:

```bash
# .env
SSID=your_wifi_ssid
PASSWORD=your_wifi_password
LANGUAGE=LV

# or export in shell
export SSID=your_wifi_ssid
export PASSWORD=your_wifi_password
export LANGUAGE=LV
```

2. Build and upload (PlatformIO will run the generator to create `include/secrets.h`):

```bash
set -a; source .env; set +a   # optional: load .env into shell
platformio run --target upload
```

3. Serial monitor:

```bash
platformio device monitor
```

The build script `build/generate_secrets.py` will create `include/secrets.h` with:

```c
#define WIFI_SSID "..."
#define WIFI_PASSWORD "..."
#define LANGUAGE "..."
```

This keeps secrets out of version control. If you prefer not to use the generator, you can manually create `include/secrets.h`.

---

## Multi-Language Support

The dashboard supports multiple languages configured via the `LANGUAGE` setting in `.env`:

- `LV` - Latvian (default)
- `EN` - English

All UI text including box titles, labels, weather states, and room names are translated. The translation system is defined in `src/lang.cpp` and can be easily extended with additional languages.

See [LANGUAGE.md](LANGUAGE.md) for detailed documentation on adding new languages.

---

## HTTP Endpoints

Main dashboard:
http://<ESP32_IP>/

Data update endpoint:
POST http://<ESP32_IP>/update
Content-Type: application/json

Payload must be a full snapshot.

---

## Rendering Model

- 2 bits per pixel
- 4 pixels per byte
- Color encoding:
  - 00 Black
  - 01 White
  - 10 Yellow
  - 11 Red

Framebuffer is allocated in PSRAM when available.

---

## License

This project is licensed under the GPL-3.0 License - see the [LICENSE](LICENSE) file for details.
