# M17 E-Paper Dashboard

ESP32-powered 4-color e-paper home dashboard (800x480 ACeP) with Home Assistant integration.

Displays weather, indoor climate, family presence, network stats, electricity prices, and 5-day forecast on a 7.5" e-paper panel. Updates every 15 minutes via deep sleep cycle.

---

## Hardware

| Component | Specification |
|-----------|--------------|
| MCU | ESP32-D0WD (PSRAM recommended) |
| Display | GoodDisplay GDEM075F52, 7.5", 800x480, 4-color ACeP |
| Colors | Black, White, Yellow, Red (2-bit per pixel) |
| Interface | SPI (write-only, no MISO) |

### Wiring

| Signal | ESP32 GPIO |
|--------|-----------|
| MOSI | 18 |
| CLK | 23 |
| CS | 27 |
| DC | 14 |
| RST | 12 |
| BUSY | 13 |

### Display Driver Modularity

The display driver is abstracted behind a common interface (`EpdDriver`), making it possible to support different e-paper panels. See [Adding a New Display Driver](#adding-a-new-display-driver) for details.

Currently supported:
- **GDEM075F52** — GoodDisplay 7.5" 4-color ACeP (800x480)

---

## Quick Start

### 1. Clone and configure

```bash
git clone https://github.com/your-username/M17-E-paper-dashboard.git
cd M17-E-paper-dashboard
cp .env.example .env
```

Edit `.env` with your settings:

```bash
SSID=your_wifi_ssid
PASSWORD=your_wifi_password
LANGUAGE=EN          # EN (English) or LV (Latvian)
DATE_FORMAT=DD.MM.YYYY  # DD.MM.YYYY, YYYY-MM-DD, MM/DD/YYYY, or DD/MM/YYYY
```

### 2. Build and flash

```bash
# Load env vars and build
set -a; source .env; set +a
platformio run --target upload

# Monitor serial output
platformio device monitor
```

The build script (`build/generate_secrets.py`) creates `include/secrets.h` automatically — secrets never enter version control.

### 3. Find the ESP32 IP

After flashing, watch the serial monitor for:

```
WiFi              : connecting... OK
IP                : 192.168.1.xxx
```

Open `http://192.168.1.xxx/` in a browser to see the live HTML dashboard.

### 4. Test with curl

Send a test payload to verify the display updates:

```bash
curl -X POST http://192.168.1.xxx/update \
  -H "Content-Type: application/json" \
  -d '{
    "date": "14.03.2026",
    "time": "13:00",
    "weather": {
      "state": "sunny",
      "temp": 18.5,
      "humidity": 55,
      "pressure": 1013,
      "clouds": 20,
      "wind": 12.3,
      "gust": 22.1
    },
    "rooms": {
      "living": {"temp": 21.5, "hum": 45},
      "bedroom": {"temp": 19.2, "hum": 52},
      "kids": {"temp": 20.1, "hum": 48},
      "bath": {"temp": 22.8, "hum": 58},
      "office": {"temp": 21.0, "hum": 46}
    },
    "internet": {"down": 450.5, "up": 92.3, "ping": 12.5},
    "power": {
      "price": 125.5,
      "total_power": 850,
      "voltage": 230,
      "frequency": 50,
      "power_factor": 0.98,
      "monthly_consumption": 450.5
    },
    "family": [
      {"name": "Alice", "state": "home"},
      {"name": "Bob", "state": "away"},
      {"name": "Charlie", "state": "home"},
      {"name": "Diana", "state": "home"}
    ],
    "forecast": [
      {"condition": "sunny", "temp_low": 10, "temp_high": 18, "precipitation": 0},
      {"condition": "cloudy", "temp_low": 8, "temp_high": 14, "precipitation": 2.5},
      {"condition": "rainy", "temp_low": 6, "temp_high": 11, "precipitation": 12},
      {"condition": "partly_cloudy", "temp_low": 9, "temp_high": 16, "precipitation": 0.5},
      {"condition": "sunny", "temp_low": 12, "temp_high": 20, "precipitation": 0}
    ]
  }'
```

You should get `OK` back and the e-paper will refresh within seconds.

---

## Home Assistant Setup

This is the primary intended use case. Home Assistant sends a JSON snapshot to the ESP32 every 15 minutes.

### Prerequisites

- Home Assistant instance running on your network
- The following HA integrations configured:
  - **Weather** integration (e.g., Met.no, OpenWeatherMap) providing `weather.home`
  - **Temperature/humidity sensors** for each room (e.g., Zigbee, Z-Wave, or ESPHome sensors)
  - **Speedtest.net** integration for network stats
  - **Nord Pool** integration (or similar) for electricity prices
  - **Power monitoring** sensor (e.g., Shelly EM, Emporia Vue)
  - **Person** entities with device trackers for family presence

### Step 1: Create the forecast template sensor

The weather forecast needs a template sensor because HA's `weather.get_forecasts` action returns data that must be stored for the REST command to access.

Add to your `configuration.yaml`:

```yaml
template:
  - trigger:
      - platform: time_pattern
        minutes: "/15"
      - platform: homeassistant
        event: start
    action:
      - action: weather.get_forecasts
        data:
          type: daily
        target:
          entity_id: weather.home    # <-- your weather entity
        response_variable: daily_forecast
    sensor:
      - name: "Weather Forecast Data"
        unique_id: weather_forecast_data
        state: "{{ now().isoformat() }}"
        attributes:
          forecast: "{{ daily_forecast['weather.home'].forecast }}"
```

### Step 2: Create the REST command

This defines the HTTP POST that sends all dashboard data to the ESP32:

```yaml
rest_command:
  epaper_update:
    url: "http://ESP32_IP_ADDRESS/update"    # <-- replace with your ESP32 IP
    method: POST
    content_type: "application/json"
    payload: >
      {
        "date": "{{ now().strftime('%d.%m.%Y') }}",
        "time": "{{ now().strftime('%H:%M') }}",

        "weather": {
          "state": "{{ states('weather.home') }}",
          "temp": {{ states('sensor.outside_temperature') | float(0) }},
          "humidity": {{ states('sensor.outside_humidity') | float(0) }},
          "pressure": {{ state_attr('weather.home','pressure') | float(0) }},
          "clouds": {{ state_attr('weather.home','cloud_coverage') | float(0) }},
          "wind": {{ state_attr('weather.home','wind_speed') | float(0) }},
          "gust": {{ state_attr('weather.home','wind_gust_speed') | float(0) }}
        },

        "rooms": {
          "living":  { "temp": {{ states('sensor.livingroom_temperature') | float(0) }},
                       "hum":  {{ states('sensor.livingroom_humidity') | float(0) }} },
          "bedroom": { "temp": {{ states('sensor.bedroom_temperature') | float(0) }},
                       "hum":  {{ states('sensor.bedroom_humidity') | float(0) }} },
          "kids":    { "temp": {{ states('sensor.kids_room_temperature') | float(0) }},
                       "hum":  {{ states('sensor.kids_room_humidity') | float(0) }} },
          "bath":    { "temp": {{ states('sensor.bathroom_temperature') | float(0) }},
                       "hum":  {{ states('sensor.bathroom_humidity') | float(0) }} },
          "office":  { "temp": {{ states('sensor.office_temperature') | float(0) }},
                       "hum":  {{ states('sensor.office_humidity') | float(0) }} }
        },

        "internet": {
          "down": {{ states('sensor.speedtest_download') | float(0) }},
          "up":   {{ states('sensor.speedtest_upload') | float(0) }},
          "ping": {{ states('sensor.speedtest_ping') | float(0) }}
        },

        "power": {
          "price":               {{ states('sensor.nordpool_kwh_lv') | float(0) }},
          "total_power":         {{ states('sensor.your_power_sensor_watts') | float(0) }},
          "voltage":             {{ states('sensor.your_voltage_sensor') | float(0) }},
          "frequency":           {{ states('sensor.your_frequency_sensor') | float(0) }},
          "power_factor":        {{ states('sensor.your_power_factor_sensor') | float(0) }},
          "monthly_consumption": {{ states('sensor.your_monthly_kwh_sensor') | float(0) }}
        },

        "family": [
          { "name": "Person1", "state": "{{ states('person.person1') }}" },
          { "name": "Person2", "state": "{{ states('person.person2') }}" },
          { "name": "Person3", "state": "{{ states('person.person3') }}" },
          { "name": "Person4", "state": "{{ states('person.person4') }}" }
        ],

        "forecast": [
          {% set forecast = state_attr('sensor.weather_forecast_data', 'forecast') %}
          {% if forecast %}
            {% for day in forecast[1:6] %}
          {
            "condition": "{{ day.condition }}",
            "temp_low": {{ day.templow | float(day.temperature) | float(0) }},
            "temp_high": {{ day.temperature | float(0) }},
            "precipitation": {{ day.precipitation | float(0) }}
          }{{ "," if not loop.last else "" }}
            {% endfor %}
          {% else %}
          {"condition":"unknown","temp_low":0,"temp_high":0,"precipitation":0},
          {"condition":"unknown","temp_low":0,"temp_high":0,"precipitation":0},
          {"condition":"unknown","temp_low":0,"temp_high":0,"precipitation":0},
          {"condition":"unknown","temp_low":0,"temp_high":0,"precipitation":0},
          {"condition":"unknown","temp_low":0,"temp_high":0,"precipitation":0}
          {% endif %}
        ]
      }
```

### Step 3: Create the automation

```yaml
automation:
  - alias: "Update E-Paper Dashboard"
    description: "Send dashboard data to ESP32 e-paper display every 15 minutes"
    trigger:
      - platform: time_pattern
        minutes: "/15"
    action:
      - service: rest_command.epaper_update
        data: {}
```

### Step 4: Restart and verify

1. Restart Home Assistant to load the new configuration
2. Check **Developer Tools > States** for `sensor.weather_forecast_data` — it should have forecast attributes
3. Go to **Developer Tools > Services**, search for `rest_command.epaper_update`, and call it manually
4. Watch the ESP32 serial monitor — you should see the JSON payload arrive and the display refresh
5. The automation will now fire every 15 minutes automatically

### Adapting to your sensors

Replace the entity IDs in the REST command with your actual sensor entities. Common alternatives:

| Data | Example entities |
|------|-----------------|
| Outdoor weather | `weather.home`, `weather.openweathermap` |
| Room sensors | `sensor.livingroom_temperature_2` (Zigbee), `sensor.esphome_bedroom_temp` |
| Network | `sensor.speedtest_download`, `sensor.fritz_box_download_throughput` |
| Electricity price | `sensor.nordpool_kwh_lv`, `sensor.tibber_price` |
| Power meter | `sensor.shelly_em_power`, `sensor.emporia_vue_total_watts` |
| Family presence | `person.alice`, `device_tracker.alice_phone` |

### Weather condition mapping

The firmware maps these HA weather states to icons: `sunny`, `cloudy`, `partlycloudy`, `rainy`, `snowy`, `lightning`, `fog`, `windy`, `clear-night`. Unknown states show a default icon.

### Timing and deep sleep

The ESP32 follows this cycle:

```
[Deep sleep] → Wake 90s before update → Connect WiFi → Listen for POST
→ Receive JSON → Render display → Deep sleep for ~15 min
```

- The ESP32 wakes **90 seconds early** to allow WiFi connection time
- It stays awake for a maximum of **180 seconds** (3 min listen window)
- If no update arrives, it goes back to sleep and retries next cycle
- HA should send updates at `:00`, `:15`, `:30`, `:45` minutes

---

## JSON Payload Reference

Full payload structure accepted by `POST /update`:

| Field | Type | Description |
|-------|------|------------|
| `date` | string | Display date, e.g. `"14.03.2026"` |
| `time` | string | Display time, e.g. `"13:00"` |
| `weather.state` | string | Weather condition code |
| `weather.temp` | float | Outdoor temperature (°C) |
| `weather.humidity` | float | Outdoor humidity (%) |
| `weather.pressure` | float | Atmospheric pressure (hPa) |
| `weather.clouds` | float | Cloud coverage (%) |
| `weather.wind` | float | Wind speed (km/h) |
| `weather.gust` | float | Wind gust speed (km/h) |
| `rooms.{room}.temp` | float | Room temperature (°C) |
| `rooms.{room}.hum` | float | Room humidity (%) |
| `internet.down` | float | Download speed (Mbps) |
| `internet.up` | float | Upload speed (Mbps) |
| `internet.ping` | float | Ping latency (ms) |
| `power.price` | float | Electricity price (€/MWh) |
| `power.total_power` | float | Current power draw (W) |
| `power.voltage` | float | Mains voltage (V) |
| `power.frequency` | float | AC frequency (Hz) |
| `power.power_factor` | float | Power factor (0-1) |
| `power.monthly_consumption` | float | Monthly consumption (kWh) |
| `family[].name` | string | Person name |
| `family[].state` | string | `"home"` or `"away"` |
| `forecast[].condition` | string | Weather condition code |
| `forecast[].temp_low` | float | Min temperature (°C) |
| `forecast[].temp_high` | float | Max temperature (°C) |
| `forecast[].precipitation` | float | Precipitation (mm) |

Room keys: `living`, `bedroom`, `kids`, `bath`, `office`

---

## HTTP Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | Live HTML dashboard with all current data + ESP32 system stats |
| `/update` | POST | Accepts JSON payload, updates display, returns `200 OK` |

---

## Display Rendering

- 2 bits per pixel, 4 pixels per byte
- Colors: `00` Black, `01` White, `10` Yellow, `11` Red
- Framebuffer: 96 KB, allocated in PSRAM when available
- Full UTF-8 Latvian character support + graphical glyphs
- Chequered gradient effects for section headers

### Redraw Speed

ACeP (Advanced Color e-Paper) displays are inherently slow due to the multi-step waveform required to drive 4 colors. A full refresh takes **15-25 seconds** — this is a hardware limitation of the panel technology, not a firmware bottleneck.

What the firmware already optimizes:
- Framebuffer is rendered entirely in RAM before SPI transfer begins
- `EPD_init_Fast()` is available for faster updates (reduced color accuracy)
- Display sleeps immediately after refresh to minimize power

The SPI transfer itself (~96 KB) takes under 1 second. The 15-25s delay is the panel's internal waveform driving cycle and cannot be shortened by firmware.

---

## Adding a New Display Driver

The display driver layer is separated into three concerns:

1. **SPI transport** (`epd_spi.h/cpp`) — pin definitions and raw SPI byte transfer
2. **Display controller** (`epd_display.h/cpp`) — initialization sequences, refresh commands, sleep
3. **Drawing primitives** (`drawing.h/cpp`) — framebuffer rendering, text, shapes, dashboard layout

To add support for a different e-paper panel:

1. Create new files in `src/display/` and `include/display/` for your panel's init sequences (e.g., `epd_display_waveshare75bw.cpp`)
2. Match the existing function signatures: `EPD_init()`, `PIC_display()`, `EPD_sleep()`, `EPD_refresh()`
3. Update `Source_BITS`, `Gate_BITS`, and color definitions in your header if the resolution or color depth differs
4. For monochrome/3-color panels, update the `Color_get()` mapping
5. Select the driver at build time via `platformio.ini` build flags or `build_src_filter`

The drawing layer and web server are display-agnostic — they only write to the framebuffer and call the display API.

---

## Multi-Language Support

Configured via `LANGUAGE` in `.env`:

- `EN` — English (default)
- `LV` — Latvian

All UI text is translated: box titles, labels, weather states, room names, weekday abbreviations. See [LANGUAGE.md](LANGUAGE.md) for adding new languages.

---

## Date Format

Configured via `DATE_FORMAT` in `.env`:

| Format | Example | Description |
|--------|---------|-------------|
| `DD.MM.YYYY` | 14.03.2026 | European (default) |
| `YYYY-MM-DD` | 2026-03-14 | ISO 8601 |
| `MM/DD/YYYY` | 03/14/2026 | US |
| `DD/MM/YYYY` | 14/03/2026 | UK |

The date format is applied consistently across the e-paper display, the web dashboard, and serial output. Input dates from Home Assistant are automatically parsed from any of the supported formats and reformatted to match your configured preference.

---

## Project Structure

```
├── include/
│   ├── config.h              # Timing, resolution, WiFi config
│   ├── data.h                # DashboardData struct
│   ├── lang.h                # Translation interface
│   ├── icons.h               # Weather icon bitmaps
│   ├── font_8x16.h           # Extended font with Latvian + glyphs
│   ├── display/
│   │   ├── epd_display.h     # Display controller API
│   │   ├── epd_spi.h         # SPI pin definitions
│   │   └── drawing.h         # Drawing primitives API
│   ├── web/web.h             # Web server
│   ├── init/init.h           # Hardware init
│   └── utils/utils.h         # Logging utilities
├── src/
│   ├── main.cpp              # Setup/loop, deep sleep logic
│   ├── lang.cpp              # Translation tables
│   ├── font_8x16.cpp         # Glyph bitmaps (Latvian + symbols)
│   ├── display/
│   │   ├── epd_display.cpp   # GDEM075F52 driver
│   │   ├── epd_spi.cpp       # SPI transfer
│   │   └── drawing.cpp       # Dashboard rendering
│   ├── web/web.cpp           # REST + HTML endpoints
│   ├── init/init.cpp         # WiFi + hardware init
│   └── utils/
├── test/                     # Unity test suite (127 tests)
├── build/
│   └── generate_secrets.py   # Build-time secrets generator
├── homeassistant_example.yaml
├── platformio.ini
└── .env.example
```

---

## Testing

```bash
platformio test    # Runs 127 unit tests on-device
```

Tests cover: font rendering, drawing primitives, UTF-8 decoding, data structures, layout constants, weather icons, language translations, color enums, alert thresholds, date format parsing.

---

## License

GPL-3.0 — see [LICENSE](LICENSE).
