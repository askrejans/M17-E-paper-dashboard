# M17-E-paper-dashboard: ESP32 GDEM075F52 E-Paper Dashboard (800×480, ACeP)

This repository contains a ESP32 firmware for driving the GDEM075F52 7.5" 4-color ACeP e-paper display and exposing it as a network-driven dashboard endpoint with HomeAssistant for a personal DIY home dashboard. Strings for output are in Latvian (remember, personal DIY project). Language/proper UTF-8 support might come later if I am bored. Also yaml configurable json data structures, panels might come. Feel free to fork and improve. Icons TO-DO.

---

## Firmware

- Wi-Fi–connected
- Receives JSON snapshots over HTTP (from Home Assistant or any client)
- Renders a full 800×480 dashboard on an ACeP e-paper panel
- Returns to deep sleep to preserve power and panel lifespan
- Includes a live HTML dashboard for debugging and inspection

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

1. Configure Wi-Fi in src/main.cpp:

const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

2. Build and upload:

pio run --target upload

3. Serial monitor:

pio device monitor

Serial output is the ground truth for parsing, memory health, and power state.

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
