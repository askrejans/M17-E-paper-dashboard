# M17 E-Paper Dashboard

ESP32 (Arduino-ESP32 3.x via pioarduino) firmware for a GoodDisplay GDEM075F52
7.5" 800×480 four-colour e-paper (BLACK=0, WHITE=1, YELLOW=2, RED=3, 2 bits per
pixel), fed by Home Assistant (pull via REST template or push via POST
`/update`). Public OSS project: keep everything universal (no personal
entities, names or IPs in defaults). Full architecture: `TECHNICAL.md`.

## Build & test

```bash
~/.platformio/penv/bin/pio test -e native      # host unit tests (no hardware)
~/.platformio/penv/bin/pio run                 # firmware (esp32dev)
~/.platformio/penv/bin/pio run -e preview && .pio/build/preview/program   # BMP previews in preview/
python3 tools/ha_template_check.py             # HA Jinja harness (jinja2, pyyaml)
python3 tools/gen_icons.py                     # regenerate include/icons.h
~/.platformio/penv/bin/pio run -t upload       # flash (separate, deliberate action)
```

## Layout

- `src/app/` — Arduino-only: state machine/power (`app.cpp`), Wi-Fi/AP/mDNS
  (`net.cpp`), HA client (`ha_client.cpp`), web server + JSON API + OTA
  (`web.cpp`), LittleFS config (`storage.cpp`), log ring (`log.cpp`).
- `src/core/` — platform independent: settings ⇄ JSON (`settings.cpp`),
  payload parser (`payload.cpp`), HA template builder (`ha_template.cpp`).
- `src/display/` — drawing primitives with band clipping (`drawing.cpp`),
  widget view models (`widgets.cpp`), themes (`themes/*.cpp`), panel driver
  (`epd_display.cpp`, `epd_spi.cpp`).
- `web/` — dashboard + admin (vanilla JS), embedded gzipped by
  `build/embed_web.py` into `include/generated/web_assets.h` (generated).
- `build/generate_secrets.py` — optional `.env` → `include/secrets.h`
  first-boot defaults (generated, git-ignored).
- `test/test_native/` — Unity tests; register new tests in `test_main.cpp`.

## Rules

- Only `src/app` may include Arduino/ESP headers; everything else must build
  for the host (`include/compat.h`).
- The firmware renders in 60-row bands (12 KB buffer, no PSRAM). Draw only via
  the primitives in `display/drawing.h`; never assume `fb` holds the whole
  screen. Rendering must be deterministic (a test compares banded vs full).
- Four inks only; intermediate tones are dithering.
- Every UI label needs EN and LV in `src/lang.cpp` with correct diacritics;
  Latvian glyphs are static bitmaps in `src/font_8x16.cpp` with base-letter
  metrics (tests enforce this).
- Use `strlcpy`/`snprintf` into fixed buffers; NAN means "no value" ("--").
- Web handlers must not block: slow work (HA calls, scans) goes through the
  job queue executed in `webLoop()`.
- Secrets are masked in API output (`SECRET_MASK`) and never logged.
- `DashboardData` must stay < 6 KB (kept in RTC memory; `static_assert`).
- Large seven-segment numerals (≥ 36 px) show ghost segments, small ones not.
