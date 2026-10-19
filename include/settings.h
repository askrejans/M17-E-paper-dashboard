// Runtime configuration, persisted as JSON (LittleFS /config.json) and edited
// from the web admin panel. Platform independent so it can be unit-tested.
#ifndef SETTINGS_H
#define SETTINGS_H

#include "compat.h"
#include "config.h"
#include <ArduinoJson.h>

#define SECRET_MASK "********"
#define ENTITY_LEN 64

enum PowerMode : uint8_t { POWER_SLEEP = 0, POWER_ALWAYS_ON = 1 };
enum SourceMode : uint8_t { SOURCE_PUSH = 0, SOURCE_PULL = 1 };

struct Thresholds
{
  float outTempLow, outTempHigh, wind, gust;
  float inTempLow, inTempHigh, inHumLow, inHumHigh;
  float downLow, upLow, pingHigh;
  int rssiLow;
  float priceHigh, powerHigh, voltLow, voltHigh, freqLow, freqHigh, pfLow, monthHigh;
};

// Generic "named entity binding": rooms use a+b (temperature, humidity),
// people use a (person/device_tracker), extras use a (any entity).
struct Binding
{
  char name[24];
  char a[ENTITY_LEN];
  char b[ENTITY_LEN];
};

// Home Assistant entity mapping used by pull mode.
struct SourceMap
{
  char weather[ENTITY_LEN];
  char outTemp[ENTITY_LEN];
  char outHum[ENTITY_LEN];
  uint8_t roomCount, familyCount, extraCount;
  Binding rooms[MAX_ROOMS];
  Binding family[MAX_FAMILY];
  Binding extras[MAX_EXTRAS];
  char netDown[ENTITY_LEN], netUp[ENTITY_LEN], netPing[ENTITY_LEN];
  char price[ENTITY_LEN], power[ENTITY_LEN], voltage[ENTITY_LEN];
  char frequency[ENTITY_LEN], factor[ENTITY_LEN], month[ENTITY_LEN], today[ENTITY_LEN];
  char priceSeries[ENTITY_LEN];   // entity holding hourly prices in an attribute
  char priceSeriesAttr[24];       // e.g. "today" (Nord Pool) / "prices"
  char message[ENTITY_LEN];       // e.g. calendar.family or input_text.notice
  char sun[ENTITY_LEN];           // usually sun.sun
  char thermostat[ENTITY_LEN];    // climate.*; empty = first climate entity (auto-discover)
  char calendars[160];            // comma separated calendar.* ids; empty = all
  bool autoDiscover;              // fill unmapped people/weather/house from HA
};

struct Settings
{
  // device
  char name[24];
  char hostname[32];
  char language[4];
  char dateFormat[12];
  char adminUser[24];   // admin panel login; auth is off while adminPass is empty
  char adminPass[33];

  // network
  char ssid[33];
  char wifiPass[65];

  // power
  uint8_t powerMode;
  uint16_t intervalMin;
  uint16_t wakeBeforeSec;
  uint16_t listenWindowSec;
  int8_t quietStart;    // local hour quiet hours begin (-1 = off)
  int8_t quietEnd;      // local hour quiet hours end
  char timezone[48];    // POSIX TZ for quiet hours, e.g. "EET-2EEST,M3.5.0/3,M10.5.0/4"

  // data source
  uint8_t sourceMode;
  char haUrl[96];
  char haToken[256];
  char pushToken[48];   // optional shared secret required on POST /update
  bool haInsecureTls;
  SourceMap map;

  // display
  char theme[16];
  char slots[DASH_SLOTS][12];
  bool systemBar;
  bool fastRefresh;     // GoodDisplay fast waveform (shorter, slightly less saturated)
  bool skipUnchanged;   // do not refresh the panel when the frame is identical
  char priceUnit[12];
  char windUnit[8];

  Thresholds th;
};

extern Settings settings;

// Widget and theme registries (ids are stable API, used in config.json)
extern const char *const WIDGET_IDS[];
extern const int WIDGET_COUNT;
extern const char *const THEME_IDS[];
extern const char *const THEME_NAMES[];
extern const int THEME_COUNT;

bool isWidgetId(const char *id);
bool isThemeId(const char *id);

void settingsDefaults(Settings &s);
// Merge keys present in `in` into `s` (partial update). Secret fields equal to
// SECRET_MASK are ignored. Values are validated/clamped. Returns false on type
// errors that prevented applying the document.
bool settingsFromJson(Settings &s, JsonVariantConst in);
// Serialise. With maskSecrets, passwords/tokens are emitted as SECRET_MASK
// (or "" when unset) so the admin UI never receives them.
void settingsToJson(const Settings &s, JsonObject out, bool maskSecrets);

#endif
