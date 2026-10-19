// Shared fixtures for the native test suite.
#pragma once

#include "data.h"
#include "settings.h"
#include "payload.h"
#include <ArduinoJson.h>

// Full payload-v2 document exercising every section.
static const char *SAMPLE_V2 = R"JSON({
  "date": "2026-10-19", "time": "14:31", "epoch": 1792420260,
  "weather": {"state": "partlycloudy", "temp": 9.7, "humidity": 81, "pressure": 1012, "clouds": 64, "wind": 19.8, "gust": 43.6},
  "rooms": [{"name": "Viesistaba", "temp": 21.8, "hum": 46}, {"name": "Guļamistaba", "temp": 20.3, "hum": 46},
            {"name": "Ofiss", "temp": 18.2, "hum": 47}],
  "family": [{"name": "Alice", "state": "home"}, {"name": "Bob", "state": "not_home"}, {"name": "Diana", "state": "Ķengarags"}],
  "forecast": [{"condition": "rainy", "temp_low": 6, "temp_high": 11, "precipitation": 4.2},
               {"condition": "sunny", "temp_low": 0, "temp_high": 10, "precipitation": 0}],
  "internet": {"down": 457.23, "up": 92.66, "ping": 12},
  "power": {"price": 0.112, "total_power": 1322, "voltage": 236, "frequency": 49.99, "power_factor": 91,
            "monthly_consumption": 143.6, "today_consumption": 6.4},
  "prices": {"now": 2, "values": [0.06, 0.05, 0.11, 0.25]},
  "extras": [{"label": "Washer", "value": "42", "unit": "min"}],
  "hourly": {"start_hour": 15, "temp": [9.7, 9.0, 8.1], "precip": [0, 0.8, 1.6]},
  "events": [{"when": "18:00", "title": "Football"}],
  "house": {"lights_on": 5, "open_count": 1, "open": ["Garage door"], "low_battery_count": 0, "low_battery": [],
            "updates": 3, "media": "Kraftwerk - Computer Love"},
  "sun": {"rise": "2026-10-19T04:58:00+00:00", "set": "18:12"},
  "thermostat": {"name": "Living", "target": 21.5, "current": 20.8, "mode": "heat", "action": "heating"},
  "message": "Hello"
})JSON";

// Original (v1) payload shape: fixed room keys, no arrays for rooms.
static const char *SAMPLE_V1 = R"JSON({
  "date": "14.03.2026", "time": "13:00",
  "weather": {"state": "sunny", "temp": "18.5", "humidity": 55, "pressure": 1013, "clouds": 20, "wind": 12.3, "gust": 22.1},
  "rooms": {"living": {"temp": 21.5, "hum": 45}, "bedroom": {"temp": 19.2, "hum": 52}, "kids": {"temp": 20.1, "hum": 48},
            "bath": {"temp": 26.8, "hum": 58}, "office": {"temp": 21.0, "hum": 46}, "garage": {"temp": 8, "hum": 70}},
  "internet": {"down": 450.5, "up": 92.3, "ping": 12.5},
  "power": {"price": 125.5, "total_power": 850, "voltage": 230, "frequency": 50, "power_factor": 0.98, "monthly_consumption": 450.5},
  "family": [{"name": "Alice", "state": "home"}, {"name": "Bob", "state": "away"}],
  "forecast": [{"condition": "sunny", "temp_low": 10, "temp_high": 18, "precipitation": 0}]
})JSON";

static inline bool loadPayload(const char *json, DashboardData &d, const char *fmt = "DD.MM.YYYY")
{
  JsonDocument doc;
  if (deserializeJson(doc, json))
    return false;
  return parsePayload(doc.as<JsonVariantConst>(), d, fmt);
}

static inline bool applySettings(Settings &s, const char *json)
{
  JsonDocument doc;
  if (deserializeJson(doc, json))
    return false;
  return settingsFromJson(s, doc.as<JsonVariantConst>());
}

extern unsigned char *fb;
