# M17 E-Paper Dashboard

## Project Overview
ESP32-based home dashboard rendered on a 7.5" 4-color ACeP e-paper display (GoodDisplay GDEM075F52, 800x480px). Shows outdoor weather, indoor climate, family location, network stats, electricity data, and 5-day forecast. Data pushed from Home Assistant via HTTP POST.

## Tech Stack
- **Platform**: ESP32 (PlatformIO, Arduino framework)
- **Display**: 4-color e-paper (BLACK=0, WHITE=1, YELLOW=2, RED=3), 2 bits per pixel
- **Language**: C++ (embedded), bilingual support (English default, Latvian)
- **Display driver**: Modular via `epd_driver.h` — build-flag selectable

## Configuration (.env)
```bash
SSID=your_wifi_ssid
PASSWORD=your_wifi_password
LANGUAGE=EN              # EN (default) or LV
DATE_FORMAT=DD.MM.YYYY   # DD.MM.YYYY, YYYY-MM-DD, MM/DD/YYYY, DD/MM/YYYY
```
Build script `build/generate_secrets.py` generates `include/secrets.h` from `.env` at build time.

## Build & Test
```bash
~/.platformio/penv/bin/pio run          # build firmware
~/.platformio/penv/bin/pio test         # run tests (127 test cases)
~/.platformio/penv/bin/pio run -t upload # flash to device
```

## Project Structure
```
src/
  main.cpp                  # Entry point, deep sleep, WiFi, HTTP server
  display/drawing.cpp       # All rendering: layout, boxes, text, icons, dashboard
  display/epd_display.cpp   # E-paper driver (GDEM075F52)
  display/epd_spi.cpp       # SPI communication
  font_8x16.cpp             # Extended glyph bitmaps (Latvian + symbols + graphical)
  font8x16_basic.cpp        # ASCII 0x00-0x7F font (128 chars, 8x16px)
  lang.cpp                  # LV/EN translations, weather/state translators
  utils/pure_utils.cpp      # Pure functions: badTemp(), badHum(), formatDate()
  utils/utils.cpp           # Arduino-dependent: jf(), logging, sleep
  web/web.cpp               # HTTP endpoints: /update (JSON POST), / (HTML dashboard)
include/
  config.h                  # Display dimensions, timing, WiFi, DATE_FORMAT macros
  data.h                    # DashboardData struct, color enum (BLACK/WHITE/YELLOW/RED)
  font_8x16.h              # Font array extern
  icons.h                   # 24x24 weather icon bitmaps (PROGMEM)
  lang.h                    # Translations struct (display + web + serial labels)
  display/drawing.h         # Rendering function declarations
  display/epd_driver.h      # Display driver abstraction (build-flag selectable)
  display/epd_display.h     # EPD constants (Source_BITS, Gate_BITS, ALLSCREEN_BYTES)
  display/epd_spi.h         # SPI pin definitions and macros
test/
  test_config.cpp           # Test orchestrator + config/utility tests
  test_drawing.cpp          # Pixel packing, rect, fillRect tests
  test_drawing_ext.cpp      # Dithering, hline, vline, layout, clipping tests
  test_fonts.cpp            # Font init, UTF-8 latvian, degree symbol
  test_fonts_ext.cpp        # Full Latvian alphabet, graphical glyphs, rendering
  test_data.cpp             # Buffer sizes, alert thresholds, color enums, date formatting
  test_lang.cpp             # Translation coverage (all weather/family states)
  test_weather.cpp          # Icon mapping, forecast data structure
  test_web.cpp              # JSON parsing
  test_console.cpp          # Logging functions
```

## Key Architecture Details

### Font System
- Base ASCII font: `font8x16_basic[128][16]` (8px wide, 16px tall per char)
- Extended font: `font8x16[256][16]` — initialized at runtime by `font8x16_init()`
- Latvian characters: 0x80-0x97 (Ā ā Č č Ē ē Ģ ģ Ī ī Ķ ķ Ļ ļ Ņ ņ Š š Ū ū Ž ž ° €)
- Graphical glyphs: 0x98-0xB5 (▪ ▸ ─ │ ↑ ↓ → ← ● ○ ■ ▲ ▼ ✓ ✗ ◆ ⚡ ☀ 💧 🌡 🏠 📶 ≋ ☁ ◎ ∿ ⏚ ☰ ⌀ $)
- UTF-8 decoding in `next_char()` converts multi-byte sequences to glyph indices
- Glyphs can also be used directly via hex escapes: `"\xAC"` for house icon

### Display Layout
- 3 equal-width columns (254px each, 8px gaps, 10px margins), 2 rows
- Header: 16x16 house icon + name, weekday + date, update time
- Header gradient: 16x16 Bayer ordered dither (257 levels), solid black → smooth fade to yellow in last 40%
- Box title bars: same smooth gradient
- Thin yellow separator lines between data rows
- Indoor box: fixed-column layout (room name, thermometer+temp, droplet+humidity)
- Every data item has a leading glyph icon

### Drawing Primitives
- `px()`, `fill()`, `fillRect()` — basic pixel/fill operations
- `fillRectDither(c1, c2)` — 50/50 checkerboard dither
- `fillRectDitherDense(c1, c2)` — 75/25 ordered dither
- `fillRectGradientH(c_left, c_right)` — smooth 16x16 Bayer horizontal gradient
- `hline()`, `vline()` — line primitives
- `rect()`, `rectColored()`, `rectWithTitle()` — box outlines and titled boxes
- `text()`, `icon()`, `icon16()` — text, 24x24 icon, 16x16 icon rendering

### Color Usage
- BLACK/YELLOW: primary theme colors (gradient headers)
- RED: alert/warning states (bad temperature, high wind, network issues, voltage)
- WHITE: background
- 16x16 Bayer dithering for smooth gradients between any two colors

### Data Flow
- ESP32 wakes from deep sleep on 15-min intervals
- Starts WiFi + HTTP server, waits for JSON POST to `/update`
- Home Assistant pushes dashboard data via REST command
- `formatDate()` reformats incoming date to configured `DATE_FORMAT`
- `draw_dashboard()` renders to framebuffer, refreshes display, enters deep sleep
- Web dashboard at `/` shows same data with violet/black theme + ESP32 system stats

### Date Format
- Configured via `DATE_FORMAT` in `.env` (default: `DD.MM.YYYY`)
- Incoming dates in any format (ISO, EU, US) are automatically parsed and reformatted
- Supported output formats: `DD.MM.YYYY`, `YYYY-MM-DD`, `MM/DD/YYYY`, `DD/MM/YYYY`

### Alert Thresholds
- Outdoor: temp < -10°C or > 30°C, wind > 20 km/h, gusts > 25 km/h
- Indoor: temp outside 19-25°C, humidity outside 35-60%
- Network: download < 10 Mbps, upload < 5 Mbps, ping > 50 ms
- Power: voltage outside 220-240V, frequency outside 48-52Hz, price > 20 €/MWh, total > 5000W
- Same thresholds apply to both e-ink display (RED color) and web dashboard (red text)

### Display Driver Modularity
- `include/display/epd_driver.h` — abstract interface, build-flag selectable
- Default: GDEM075F52. Override with `-DEPD_DRIVER_WAVESHARE75BW` etc. in `platformio.ini`
- Interface: `EPD_init()`, `PIC_display()`, `EPD_sleep()`, `EPD_refresh()`

## Conventions
- English is the default UI language; all labels need both EN and LV translations in `lang.cpp`
- Web frontend, serial output, and e-ink display all use the same `lang.*` translations
- New glyphs: add bitmap to `ext_glyphs[]` in `font_8x16.cpp`, entry to `glyph_map[]`, UTF-8 entry in `utf8_map[]` in `drawing.cpp`
- Use `strlcpy()` instead of `strcpy()` for all string copies into fixed buffers
- Coordinate system: origin (0,0) is top-left; x increases right, y increases down
- Use `G_*` macros (e.g. `G_THERMO`, `G_BOLT`, `G_WIND`) for inline glyph characters
- Date formatting: use `formatDate()` to convert incoming dates to `DATE_FORMAT`
