# M17 E-Paper Dashboard — technical documentation

How the firmware works, how data flows from Home Assistant to the panel, and
how to extend it. For installation and everyday use see [README.md](README.md).

- [Architecture](#architecture)
- [Runtime state machine and power](#runtime-state-machine-and-power)
- [Data sources](#data-sources)
- [Payload format (v2)](#payload-format-v2)
- [Configuration](#configuration)
- [Rendering pipeline](#rendering-pipeline)
- [Display driver](#display-driver)
- [Themes and widgets](#themes-and-widgets)
- [Fonts and icons](#fonts-and-icons)
- [HTTP API](#http-api)
- [Security model](#security-model)
- [Memory budget](#memory-budget)
- [Testing and tooling](#testing-and-tooling)
- [Extending](#extending)

---

## Architecture

```
                 ┌──────────── Home Assistant ────────────┐
   pull: POST /api/template + weather.get_forecasts        │ push: rest_command → POST /update
                 └───────────────┬─────────────────────────┘
                                 ▼
┌─────────────────────────── ESP32 firmware ───────────────────────────┐
│ app/      state machine · Wi-Fi/AP/mDNS · HA client · web server/API │  Arduino-only
│           LittleFS config · log ring buffer · OTA                     │
├───────────────────────────────────────────────────────────────────────┤
│ core/     settings (JSON) · payload parser · HA template builder     │  platform
│ utils/    dates · alerts · UTF-8 · scheduling math                   │  independent:
│ display/  widgets (view models) · themes · drawing primitives        │  builds on the
│           fonts · icons · BMP export                                  │  host for tests
├───────────────────────────────────────────────────────────────────────┤
│ display/epd_*   GDEM075F52 driver (SPI, init, banded frame stream)   │  hardware
└───────────────────────────────────────────────────────────────────────┘
```

Everything above the driver except `src/app` is plain C++17 with no Arduino
dependency (`include/compat.h` shims `PROGMEM`/`strlcpy`). The same code is
compiled for the host by the `native` (unit tests) and `preview` (renders
BMPs of every theme) PlatformIO environments.

| Path | Responsibility |
|---|---|
| `src/main.cpp` | `setup()`/`loop()` → `appSetup()`/`appLoop()` |
| `src/app/app.cpp` | boot, wake reasons, RTC state, render + panel refresh, sleep scheduling |
| `src/app/net.cpp` | station connect, setup access point with captive DNS, mDNS |
| `src/app/ha_client.cpp` | pull mode: template render, daily/hourly forecasts, connection test |
| `src/app/web.cpp` | static assets, JSON API, push endpoint, preview, OTA, background jobs |
| `src/app/storage.cpp` | `/config.json` on LittleFS (atomic write-then-rename) |
| `src/core/settings.cpp` | settings struct ⇄ JSON, validation, secret masking |
| `src/core/payload.cpp` | JSON payload (v1 + v2) → `DashboardData` and back |
| `src/core/ha_template.cpp` | builds the Jinja template from the entity mapping |
| `src/display/widgets.cpp` | widget view models: formatted values, alert flags |
| `src/display/themes/*.cpp` | Classic, VFD, Terminal, Line Printer |
| `src/display/drawing.cpp` | pixel/shape/text/7-segment primitives, band clipping, BMP |
| `src/display/epd_*.cpp` | panel init, SPI, frame streaming |
| `web/` | dashboard + admin UI (embedded gzipped at build time) |
| `tools/` | preview renderer, HA template harness, icon generator |

## Runtime state machine and power

```
 boot ─► load /config.json ─► Wi-Fi ──fail──► (3rd failure / no SSID / reset) ─► setup AP + captive portal
                                │                └─ otherwise: sleep 60 s × failures, retry
                                ▼
                         web server + mDNS
                                │
            ┌── pull ───────────┴────────── push ──┐
            ▼                                      ▼
   HA template + forecasts                listen for POST /update
            └──────────────► render ◄─────────────┘
                                │
              power mode = sleep and nothing keeps it awake?
                    ├── yes ─► deep sleep until next slot
                    └── no  ─► loop (always-on refreshes on schedule / on push)
```

**Power modes** (`power.mode`):

- `always_on` (default) — never sleeps. Pull mode refreshes every
  `interval_min`; push mode redraws whenever HA posts. Admin always reachable.
- `sleep` — periodic refresh for battery use. The device deep-sleeps between
  slots. Things that keep it awake: the first 5 minutes after reset or
  power-on (`ADMIN_WINDOW_SEC`), maintenance mode (max 60 min), admin activity
  in the last 2 minutes, an OTA upload, and the setup access point.

**Slot alignment.** The ESP32 RTC keeps time through deep sleep. The clock is
set from HA (`epoch` in pull mode and in v2 push payloads, or `date` + `time`
of older push payloads). With a clock, wake-ups are aligned to wall-clock
multiples of the interval (`secondsToSlot()`):

- pull: wake 2 s *after* each slot, fetch, render, sleep;
- push: wake `wake_before_s` (default 90 s) *before* each slot and listen for
  up to `listen_window_s` (default 180 s).

Without a clock the device falls back to "interval minus listen window".

**Quiet hours** (`quiet_start`, `quiet_end`, local hour, wraps midnight): a
wake-up that would fall inside the window is moved to its end. Local time uses
the POSIX `timezone` setting (default `UTC0`, e.g.
`EET-2EEST,M3.5.0/3,M10.5.0/4` for Riga).

**Manual updates.** Whenever an HA URL and token are configured, a pull can be
triggered by hand in either source mode: *Pull from Home Assistant now* in the
admin (Status / Data source), `POST /api/ha/fetch`, or the board's **BOOT
button (GPIO0)**. In deep-sleep mode the button also wakes the device
(`ext0`), which pulls, redraws and goes back to sleep. Without HA credentials
the button just redraws the panel. Manual pulls are recorded as source
`manual`, always redraw (no unchanged-frame skip) and do not count towards the
stale/retry logic of scheduled pulls.

**Failure handling.** Pull failures retry after 2, 4, 6 … minutes. After the
third consecutive failure the last data is re-drawn once with a STALE marker.
The last good `DashboardData` lives in RTC slow memory (`RTC_DATA_ATTR`,
< 6 KB, enforced by a `static_assert`) so the web dashboard has content
immediately after a timer wake.

## Data sources

### Pull mode (recommended)

Needs the HA URL and a long-lived access token (HA → Profile → Security). Per
refresh the device makes up to three requests:

1. `POST /api/template` with a Jinja template generated from the entity
   mapping (`buildHaTemplate()`); HA returns a ready payload-v2 JSON document.
2. `POST /api/services/weather/get_forecasts?return_response` with
   `type: daily` (today is skipped), and
3. the same with `type: hourly` (first 24 entries).

The template is the single source of truth for what is shown; view it in the
admin (Data source → *Show HA template*) or run it there (*Test mapping*).

**Auto-discovery** (`map.auto_discover`, on by default) fills everything that
is not mapped explicitly, so a fresh install shows useful data with zero
configuration:

| Data | Discovery rule |
|---|---|
| weather | first `weather.*` entity |
| people | all `person.*` (first name of the friendly name) |
| calendar | next event of every `calendar.*` (or the comma list in `calendars`) |
| thermostat | first `climate.*` (or `map.thermostat`): set/current temperature, mode, hvac_action |
| house | lights on, open `binary_sensor` with device class door/window/garage_door/opening, battery sensors < 20 %, pending `update.*`, first playing `media_player` |

Numeric states are emitted as `float(none)`, so `unknown`/`unavailable`
become JSON `null` and render as `--` instead of a misleading 0.

Price series (e.g. Nord Pool `today`) with 15- or 30-minute resolution are
averaged to 24 hourly values in the template.

### Push mode

Home Assistant POSTs the payload to `http://<device>/update` (alias
`/api/push`). The original fixed v1 payload is still accepted unchanged, so
existing automations keep working. `homeassistant/m17_epaper.yaml` is a
complete package (helper for the interval, schedule automation, script that
fetches forecasts and builds a full v2 payload). An optional push token is
checked as `Authorization: Bearer <token>` or `?token=`.

## Payload format (v2)

All sections are optional; missing values render as `--`.

```jsonc
{
  "date": "2026-10-19",            // any of YYYY-MM-DD, DD.MM.YYYY, MM/DD/YYYY, DD/MM/YYYY (+ ISO time)
  "time": "14:31",
  "epoch": 1792420260,              // optional; sets the device clock
  "weather":  { "state": "partlycloudy", "temp": 9.7, "humidity": 81, "pressure": 1012,
                "clouds": 64, "wind": 19.8, "gust": 43.6 },
  "rooms":    [ { "name": "Living", "temp": 21.8, "hum": 46 } ],          // ≤ 8
  "family":   [ { "name": "Alice", "state": "home" } ],                   // ≤ 6; zone names allowed
  "forecast": [ { "condition": "rainy", "temp_low": 6, "temp_high": 11, "precipitation": 4.2 } ], // ≤ 7
  "hourly":   { "start_hour": 15, "temp": [9.7, 9.0], "precip": [0, 0.8] },                    // ≤ 24
  "internet": { "down": 457.2, "up": 92.7, "ping": 12 },
  "power":    { "price": 0.112, "total_power": 1322, "voltage": 236, "frequency": 49.99,
                "power_factor": 0.91, "today_consumption": 6.4, "monthly_consumption": 143.6 },
  "prices":   { "now": 14, "values": [0.06, 0.05] },  // or a plain array; Nord Pool {"value":x} objects ok
  "extras":   [ { "label": "CO₂", "value": "812", "unit": "ppm" } ],      // ≤ 8
  "events":   [ { "when": "18:00", "title": "Football" } ],               // ≤ 5
  "house":    { "lights_on": 5, "open_count": 1, "open": ["Garage door"],
                "low_battery_count": 0, "low_battery": [], "updates": 3, "media": "Artist - Title" },
  "thermostat": { "name": "Living", "target": 21.5, "current": 20.8, "mode": "heat", "action": "heating" },
  "sun":      { "rise": "07:58", "set": "18:12" },  // HH:MM or ISO timestamps
  "message":  "Free text line"
}
```

Compatibility details:

- v1 `rooms` object (`living`, `bedroom`, `kids`, `bath`, `office`, plus any
  other key): stored as `@key` and translated at render time; `bath` keeps the
  historic rule (only humidity raises an alert).
- Native HA forecast fields (`temperature`, `templow`, `datetime`) are
  accepted anywhere a forecast list is.
- Numbers may be strings; `power_factor` may be 0–1 or percent.

## Configuration

Stored as `/config.json` on LittleFS and edited through the admin panel or
`POST /api/config` (partial documents are merged). On first boot (or after a
factory reset) defaults are used, seeded from the optional build-time `.env`
(`SSID`, `PASSWORD`, `LANGUAGE`, `DATE_FORMAT`, `HA_URL`, `HA_TOKEN`).

```jsonc
{
  "device":  { "name": "M17", "hostname": "m17-epaper", "language": "EN|LV",
               "date_format": "DD.MM.YYYY", "admin_user": "admin", "admin_password": "" },
  "wifi":    { "ssid": "", "password": "" },
  "power":   { "mode": "always_on|sleep", "interval_min": 15, "wake_before_s": 90,
               "listen_window_s": 180, "quiet_start": -1, "quiet_end": 6, "timezone": "UTC0" },
  "source":  { "mode": "push|pull", "ha_url": "", "ha_token": "", "push_token": "",
               "insecure_tls": true,
               "map": { "weather": "weather.home", "out_temp": "", "out_hum": "",
                        "rooms":  [ { "name": "", "temp": "", "hum": "" } ],
                        "family": [ { "name": "", "entity": "" } ],
                        "extras": [ { "name": "", "entity": "" } ],
                        "net_down": "", "net_up": "", "net_ping": "",
                        "price": "", "power": "", "voltage": "", "frequency": "", "power_factor": "",
                        "today": "", "month": "", "price_series": "", "price_series_attr": "today",
                        "calendars": "", "message": "", "sun": "sun.sun", "thermostat": "",
                        "auto_discover": true } },
  "display": { "theme": "classic|vfd|terminal|printout",
               "slots": ["outdoor","indoor","forecast","family","network","power"],
               "system_bar": true, "refresh": "quality|fast", "skip_unchanged": true,
               "price_unit": "€/kWh", "wind_unit": "km/h" },
  "thresholds": { "out_temp_low": -10, "out_temp_high": 30, "wind": 20, "gust": 25,
                  "in_temp_low": 19, "in_temp_high": 25, "in_hum_low": 35, "in_hum_high": 60,
                  "down_low": 10, "up_low": 5, "ping_high": 50, "rssi_low": -75,
                  "price_high": 0.2, "power_high": 5000, "volt_low": 220, "volt_high": 240,
                  "freq_low": 49.5, "freq_high": 50.5, "pf_low": 0.7, "month_high": 320 }
}
```

Validation: unknown themes are rejected, unknown widget ids become `none`,
numbers are clamped (`interval_min` 1–1440 …), and `wake_before_s` is kept
below the interval. Secrets (`admin_password`, `wifi.password`, `ha_token`,
`push_token`) are returned as `********` by the API; sending that mask back
leaves the stored value unchanged, an empty string clears it.

## Rendering pipeline

```
DashboardData + Settings ─► buildWidget() per slot ─► WidgetView (strings, alert flags, series)
                                                          │
                              theme renderer (layout, typography, colours)
                                                          │
                         drawing primitives (clipped to the current band)
                                                          │
                     12 KB band buffer ─► LUT ─► SPI ─► panel RAM (8 bands × 60 rows)
```

**Banded rendering.** The ESP32-D0WD has no PSRAM, and a full 800×480×2 bit
framebuffer (96 KB) left too little heap for the web server and TLS. The
firmware therefore renders the screen in 8 bands of 60 rows into a 12 KB
buffer: for each band the whole theme runs again with every primitive clipped
to the band, then the band is streamed to the panel. Rendering is
deterministic, so the bands join seamlessly; a unit test asserts that banded
output is byte-identical to a full-screen render for every theme. CPU cost is
~290 ms for all 8 bands, negligible next to the panel's waveform. The live
preview (`/api/preview.bmp`) renders bands lazily while the BMP streams.
Host builds keep a full-screen buffer.

**Skip unchanged.** A CRC over the data (minus volatile system stats) and the
display-relevant settings is kept in RTC memory; an identical frame does not
trigger the ~20 s panel refresh.

## Display driver

GoodDisplay **GDEM075F52**, 7.5", 800×480, black/white/yellow/red ACeP,
JD79665AA controller, SPI (write-only). Pixel format: 2 bits per pixel, 4
pixels per byte, MSB first — `00` black, `01` white, `10` yellow, `11` red.
There is no partial or greyscale refresh; only these four inks exist, every
intermediate tone in the themes is ordered (Bayer 16×16) dithering.

| Signal | GPIO |
|---|---|
| SCK | 18 |
| MOSI | 23 |
| CS | 27 |
| DC | 14 |
| RST | 12 |
| BUSY | 13 (low = busy) |

(The original docs listed SCK/MOSI swapped; the firmware always used the VSPI
defaults above.)

Driver improvements in 2.0:

- **Burst transfer**: the frame is streamed with one CS/DC frame per chunk
  (`SPI.writeBytes`) through a 256-entry colour LUT instead of toggling CS for
  each of the 96 000 bytes, at 4 MHz (`EPD_SPI_HZ`). Measured: ~240 ms.
- **Fast waveform** (`display.refresh = fast`): the controller's LUT
  temperature is forced (`E0 02`, `E6 5A`, `A5 00`) *before* the booster is
  powered on (`04`). The panel is specified at ~12 s fast vs ~22 s full
  refresh; fast mode may leave slight ghosting over many updates.
- **BUSY timeout** of 60 s so a disconnected panel cannot hang the firmware.
- Frame streaming API (`EPD_beginFrame` / `EPD_writeRows` / `EPD_endFrame`)
  used by the band renderer; `PIC_display()` remains for compatibility.

Other panels: implement the functions in `include/display/epd_driver.h` and
select them with a build flag (see the header).

## Themes and widgets

A **widget** turns data into a `WidgetView`: title, optional hero value with
unit/icon/caption, up to 10 rows (glyph, label, value, value2, alert flag),
and a kind (`LIST`, `TABLE`, `FORECAST`, `PEOPLE`, `CHART`, `HOURLY`).
Widgets are theme-independent; alert colouring follows `thresholds`.

| Widget id | Content |
|---|---|
| `home` | overview: people (lit = home, otherwise their zone/“away”), thermostat set + current temperature and heating state, 24 h temperature mini chart |
| `outdoor` | temperature, condition, humidity, pressure, wind/gusts, clouds |
| `indoor` | room table (temperature, humidity) |
| `forecast` | next 5 days: icon, high/low, precipitation |
| `hourly` | 24 h temperature curve + precipitation bars |
| `family` | presence per person (home / away / zone) |
| `network` | download, upload, ping |
| `power` | power with a mini 24 h price forecast beside it, price, voltage, frequency, power factor, today/month kWh |
| `prices` | hourly price chart with current hour |
| `house` | lights on, open doors/windows, low batteries, updates, media |
| `calendar` | next events |
| `extras` | any HA entities |
| `agenda` | date, sunrise/sunset, message |
| `system` | IP, RSSI, heap, wake count, firmware |

A **theme** lays out the six slots (`display.slots`) plus its own frame:

| Theme | Look |
|---|---|
| `classic` (default) | the original M17 layout: gradient header, 3×2 titled boxes, system bar |
| `vfd` | 80s hi-fi fluorescent display: black glass, 7-segment numerals (ghost segments on large digits only), segmented meters, spectrum-analyser price bars, dot-matrix icons, annunciators that reflect live status |
| `terminal` | split-pane Unix session, one shell command per widget, tmux-style status lines |
| `printout` | line-printer report on fanfold paper: tractor holes, greenbar bands, dot leaders, red-ribbon alerts |

## Fonts and icons

- 8×16 bitmap font: ASCII (`font8x16_basic.cpp`) + extended table
  (`font_8x16.cpp`): all Latvian letters, `° €`, and dashboard glyphs
  (`G_*` macros in `display/glyphs.h`). UTF-8 is decoded in `fontNextChar()`;
  typographic dashes/quotes fold to ASCII, unknown characters render `?`.
- Latvian letters share the exact metrics of their base letters (capitals keep
  the full cap height with the accent in rows 0–1, lowercase accents at
  ascender height, cedillas in rows 12–13); unit tests enforce this.
- Large text uses Scale2x edge smoothing for even scale factors.
- Weather icons (24×24) are generated by `tools/gen_icons.py` from shared
  parts (hand-drawn cloud, sun, precipitation stamps) — every HA condition has
  its own icon.

## HTTP API

All JSON. When an admin password is set, every endpoint marked 🔒 requires
HTTP Basic auth (`device.admin_user` / `device.admin_password`).

| Method | Path | Description |
|---|---|---|
| GET | `/` | live dashboard (public) |
| GET | `/admin` 🔒 | admin panel |
| GET | `/api/state` | current data + display status (public, read-only) |
| GET | `/api/status` 🔒 | device status: firmware, heap, Wi-Fi, timings, errors |
| GET | `/api/meta` 🔒 | widget and theme lists |
| GET | `/api/config` 🔒 | configuration (secrets masked; `?secrets=1` unmasked, `?download=1` as file) |
| POST | `/api/config` 🔒 | merge a (partial) configuration → `{ok, reboot_required}` |
| POST | `/api/action` 🔒 | `{"action": "render" \| "fetch" \| "reboot" \| "sleep" \| "maintenance_on" \| "maintenance_off" \| "factory_reset"}` |
| GET | `/api/preview.bmp` 🔒 | rendered panel image; `?theme=&slots=a,b,…&lang=` to try unsaved choices |
| GET | `/api/logs` 🔒 | log ring buffer (text) |
| POST | `/api/wifi/scan` 🔒 | start Wi-Fi scan job |
| POST | `/api/ha/test` 🔒 | start HA connection test job (`{ha_url, ha_token, insecure_tls}`) |
| GET | `/api/ha/entities?q=&domain=` 🔒 | start entity search job (≤ 40 results) |
| POST | `/api/ha/fetch` 🔒 | manual pull from HA now (works in push mode too), redraws the panel; result via `/api/job` |
| POST | `/api/ha/render` 🔒 | start a job rendering the mapping template on HA |
| GET | `/api/ha/template` 🔒 | the generated Jinja template (text) |
| GET | `/api/job?id=` 🔒 | job result `{done, code, body}` |
| POST | `/api/ota` 🔒 | multipart firmware upload, reboots when done |
| POST | `/update`, `/api/push` | push payload (optional push token) |

Slow operations (anything that talks to HA, Wi-Fi scans) are **jobs**: the
request returns `202 {"job": id}` immediately, the work runs on the loop task
(never inside the async TCP task), and the client polls `/api/job`.

## Security model

- The admin panel starts **open** so first setup needs no credentials; it
  shows a banner until a password is set (Security tab). Then HTTP Basic auth
  protects all admin pages and API calls. Basic auth over plain HTTP is only
  as safe as your LAN — keep the device on a trusted network/VLAN.
- Secrets never leave the device through the API unless explicitly exported
  (`?secrets=1`).
- The push endpoint can require a token.
- The public endpoints (`/`, `/api/state`) expose what the panel shows,
  including presence; restrict them at the network level if needed.
- OTA requires admin auth when a password is set.

## Memory budget

ESP32-D0WD, 4 MB flash, no PSRAM.

| Item | Size |
|---|---|
| Partition table | `min_spiffs.csv`: 2 × 1.9 MB app (OTA) + 128 KB LittleFS |
| Firmware | ~1.35 MB (≈ 69 % of an app slot), incl. ~20 KB gzipped web UI |
| Static RAM | ~108 KB (incl. 12 KB band buffer, data/settings copies, 6 KB log) |
| Free heap after Wi-Fi + server | ~128 KB (enough for HTTPS to HA) |
| RTC slow memory | last `DashboardData` (< 6 KB) + counters |

## Testing and tooling

```bash
pio test -e native                    # 80 unit tests on the host
pio run -e preview && .pio/build/preview/program [payload.json] [settings.json] [outdir]
python3 tools/ha_template_check.py    # renders the HA templates in an HA-like Jinja env
python3 tools/gen_icons.py            # regenerate include/icons.h
```

- **Unit tests** (`test/test_native`): dates, alerts, quiet hours and slot
  scheduling, UTF-8 and Latvian upper-casing, settings round-trip/validation/
  secret masking, payload v1/v2/limits/null handling, HA template generation
  and escaping, translations completeness, drawing primitives, band clipping,
  7-segment, BMP export, font metrics, every widget, every theme in both
  languages, theme determinism, banded vs full render equality.
- **Preview renderer** writes `preview/<theme>.bmp`, `<theme>_alt.bmp`
  (newer widgets), status screens and a font specimen.
- **HA template harness** (`pip install jinja2 pyyaml`) emulates HA's
  template functions/filters with a fake house, renders the pull template for
  mapped, auto-discovery and minimal configs and the push package payload,
  validates the JSON and feeds it through the firmware renderer.

## Extending

- **New widget**: add a builder in `src/display/widgets.cpp`, its id to
  `WIDGET_IDS` in `settings.cpp`, then a case in each theme if the generic
  list layout is not enough. It appears in the admin slot pickers
  automatically.
- **New theme**: implement `void themeX(const DashboardData&, const Settings&)`
  in `src/display/themes/`, register it in `theme.cpp` and `THEME_IDS`/
  `THEME_NAMES`. Use only the drawing primitives (they handle band clipping).
- **New data**: extend `DashboardData` (keep it < 6 KB), parse it in
  `payload.cpp`, emit it from `buildHaTemplate()`, serialise it in
  `dataToJson()`, and cover it with tests.
- **New language**: add a `Translations` table in `src/lang.cpp`; the test
  `test_lang_all_strings_present` fails until every field is filled.
