#include "settings.h"

Settings settings;

const char *const WIDGET_IDS[] = {
    "home", "outdoor", "indoor", "forecast", "hourly", "family", "network", "power",
    "prices", "house", "calendar", "extras", "agenda", "system", "none"};
const int WIDGET_COUNT = sizeof(WIDGET_IDS) / sizeof(WIDGET_IDS[0]);

const char *const THEME_IDS[] = {"classic", "vfd", "terminal", "printout"};
const char *const THEME_NAMES[] = {"Classic", "Fluorescent VFD", "Terminal TTY", "Line Printer"};
const int THEME_COUNT = sizeof(THEME_IDS) / sizeof(THEME_IDS[0]);

static const char *const DEFAULT_SLOTS[DASH_SLOTS] = {
    "outdoor", "indoor", "forecast", "family", "network", "power"};

static const char *const DATE_FORMATS[] = {"DD.MM.YYYY", "YYYY-MM-DD", "MM/DD/YYYY", "DD/MM/YYYY"};

bool isWidgetId(const char *id)
{
  for (int i = 0; i < WIDGET_COUNT; i++)
    if (strcmp(id, WIDGET_IDS[i]) == 0)
      return true;
  return false;
}

bool isThemeId(const char *id)
{
  for (int i = 0; i < THEME_COUNT; i++)
    if (strcmp(id, THEME_IDS[i]) == 0)
      return true;
  return false;
}

static bool isDateFormat(const char *f)
{
  for (const char *d : DATE_FORMATS)
    if (strcmp(f, d) == 0)
      return true;
  return false;
}

void settingsDefaults(Settings &s)
{
  memset(&s, 0, sizeof(s));
  strlcpy(s.name, "M17", sizeof(s.name));
  strlcpy(s.hostname, "m17-epaper", sizeof(s.hostname));
  strlcpy(s.adminUser, "admin", sizeof(s.adminUser));
  strlcpy(s.language, strcmp(LANGUAGE, "LV") == 0 ? "LV" : "EN", sizeof(s.language));
  strlcpy(s.dateFormat, isDateFormat(DATE_FORMAT) ? DATE_FORMAT : "DD.MM.YYYY", sizeof(s.dateFormat));

  strlcpy(s.ssid, WIFI_SSID, sizeof(s.ssid));
  strlcpy(s.wifiPass, WIFI_PASSWORD, sizeof(s.wifiPass));

  s.powerMode = POWER_ALWAYS_ON; // deep sleep only when chosen in the admin panel
  s.intervalMin = DEFAULT_UPDATE_INTERVAL_MIN;
  s.wakeBeforeSec = DEFAULT_WAKE_BEFORE_SEC;
  s.listenWindowSec = DEFAULT_LISTEN_WINDOW_SEC;
  s.quietStart = -1;
  s.quietEnd = 6;
  strlcpy(s.timezone, "UTC0", sizeof(s.timezone));

  s.sourceMode = SOURCE_PUSH;
  strlcpy(s.haUrl, HA_URL, sizeof(s.haUrl));
  strlcpy(s.haToken, HA_TOKEN, sizeof(s.haToken));
  s.haInsecureTls = true;
  strlcpy(s.map.weather, "weather.home", ENTITY_LEN);
  strlcpy(s.map.sun, "sun.sun", ENTITY_LEN);
  strlcpy(s.map.priceSeriesAttr, "today", sizeof(s.map.priceSeriesAttr));
  s.map.autoDiscover = true;

  strlcpy(s.theme, "classic", sizeof(s.theme));
  for (int i = 0; i < DASH_SLOTS; i++)
    strlcpy(s.slots[i], DEFAULT_SLOTS[i], sizeof(s.slots[i]));
  s.systemBar = true;
  s.fastRefresh = false;
  s.skipUnchanged = true;
  strlcpy(s.priceUnit, "\xE2\x82\xAC/kWh", sizeof(s.priceUnit)); // €/kWh
  strlcpy(s.windUnit, "km/h", sizeof(s.windUnit));

  Thresholds &t = s.th;
  t.outTempLow = -10;  t.outTempHigh = 30;  t.wind = 20;  t.gust = 25;
  t.inTempLow = 19;    t.inTempHigh = 25;   t.inHumLow = 35; t.inHumHigh = 60;
  t.downLow = 10;      t.upLow = 5;         t.pingHigh = 50; t.rssiLow = -75;
  t.priceHigh = 0.20f; t.powerHigh = 5000;  t.voltLow = 220; t.voltHigh = 240;
  t.freqLow = 49.5f;   t.freqHigh = 50.5f;  t.pfLow = 0.70f; t.monthHigh = 320;
}

// ── JSON helpers ───────────────────────────────────────────────

static void getStr(JsonVariantConst v, char *dst, size_t n)
{
  if (v.is<const char *>())
    strlcpy(dst, v.as<const char *>(), n);
}

static void getSecret(JsonVariantConst v, char *dst, size_t n)
{
  if (v.is<const char *>() && strcmp(v.as<const char *>(), SECRET_MASK) != 0)
    strlcpy(dst, v.as<const char *>(), n);
}

template <typename T>
static void getNum(JsonVariantConst v, T &dst, T lo, T hi)
{
  if (!v.is<float>() && !v.is<int>() && !v.is<const char *>())
    return;
  double x = v.is<const char *>() ? atof(v.as<const char *>()) : v.as<double>();
  if (isnan(x))
    return;
  if (x < (double)lo) x = lo;
  if (x > (double)hi) x = hi;
  dst = (T)x;
}

static void getFloat(JsonVariantConst v, float &dst)
{
  getNum<float>(v, dst, -1e9f, 1e9f);
}

static void getBool(JsonVariantConst v, bool &dst)
{
  if (v.is<bool>())
    dst = v.as<bool>();
}

static const char *secretOut(const char *v, bool mask)
{
  return (mask && v[0]) ? SECRET_MASK : v;
}

static uint8_t getBindings(JsonVariantConst arr, Binding *b, uint8_t max, const char *ka, const char *kb)
{
  uint8_t n = 0;
  for (JsonVariantConst item : arr.as<JsonArrayConst>())
  {
    if (n >= max)
      break;
    Binding &x = b[n];
    memset(&x, 0, sizeof(x));
    getStr(item["name"], x.name, sizeof(x.name));
    getStr(item[ka], x.a, sizeof(x.a));
    if (kb)
      getStr(item[kb], x.b, sizeof(x.b));
    if (x.name[0] || x.a[0])
      n++;
  }
  return n;
}

static void putBindings(JsonArray arr, const Binding *b, uint8_t n, const char *ka, const char *kb)
{
  for (uint8_t i = 0; i < n; i++)
  {
    JsonObject o = arr.add<JsonObject>();
    o["name"] = b[i].name;
    o[ka] = b[i].a;
    if (kb)
      o[kb] = b[i].b;
  }
}

// Table of simple entity fields so (de)serialisation stays in one place.
struct MapField
{
  const char *key;
  size_t off;
};
#define MF(k, f) {k, offsetof(SourceMap, f)}
static const MapField MAP_FIELDS[] = {
    MF("weather", weather), MF("out_temp", outTemp), MF("out_hum", outHum),
    MF("net_down", netDown), MF("net_up", netUp), MF("net_ping", netPing),
    MF("price", price), MF("power", power), MF("voltage", voltage),
    MF("frequency", frequency), MF("power_factor", factor), MF("month", month),
    MF("today", today), MF("price_series", priceSeries), MF("message", message),
    MF("sun", sun), MF("thermostat", thermostat)};
#undef MF

struct ThField
{
  const char *key;
  size_t off;
};
#define TF(k, f) {k, offsetof(Thresholds, f)}
static const ThField TH_FIELDS[] = {
    TF("out_temp_low", outTempLow), TF("out_temp_high", outTempHigh),
    TF("wind", wind), TF("gust", gust),
    TF("in_temp_low", inTempLow), TF("in_temp_high", inTempHigh),
    TF("in_hum_low", inHumLow), TF("in_hum_high", inHumHigh),
    TF("down_low", downLow), TF("up_low", upLow), TF("ping_high", pingHigh),
    TF("price_high", priceHigh), TF("power_high", powerHigh),
    TF("volt_low", voltLow), TF("volt_high", voltHigh),
    TF("freq_low", freqLow), TF("freq_high", freqHigh),
    TF("pf_low", pfLow), TF("month_high", monthHigh)};
#undef TF

bool settingsFromJson(Settings &s, JsonVariantConst in)
{
  if (!in.is<JsonObjectConst>())
    return false;

  JsonVariantConst dev = in["device"];
  getStr(dev["name"], s.name, sizeof(s.name));
  getStr(dev["hostname"], s.hostname, sizeof(s.hostname));
  if (dev["language"].is<const char *>())
  {
    const char *l = dev["language"];
    if (strcmp(l, "EN") == 0 || strcmp(l, "LV") == 0)
      strlcpy(s.language, l, sizeof(s.language));
  }
  if (dev["date_format"].is<const char *>() && isDateFormat(dev["date_format"]))
    strlcpy(s.dateFormat, dev["date_format"], sizeof(s.dateFormat));
  getStr(dev["admin_user"], s.adminUser, sizeof(s.adminUser));
  if (!s.adminUser[0])
    strlcpy(s.adminUser, "admin", sizeof(s.adminUser));
  getSecret(dev["admin_password"], s.adminPass, sizeof(s.adminPass));

  JsonVariantConst wifi = in["wifi"];
  getStr(wifi["ssid"], s.ssid, sizeof(s.ssid));
  getSecret(wifi["password"], s.wifiPass, sizeof(s.wifiPass));

  JsonVariantConst pw = in["power"];
  if (pw["mode"].is<const char *>())
    s.powerMode = strcmp(pw["mode"], "always_on") == 0 ? POWER_ALWAYS_ON : POWER_SLEEP;
  getNum<uint16_t>(pw["interval_min"], s.intervalMin, 1, 1440);
  getNum<uint16_t>(pw["wake_before_s"], s.wakeBeforeSec, 0, 600);
  getNum<uint16_t>(pw["listen_window_s"], s.listenWindowSec, 30, 1800);
  getNum<int8_t>(pw["quiet_start"], s.quietStart, -1, 23);
  getNum<int8_t>(pw["quiet_end"], s.quietEnd, 0, 23);
  getStr(pw["timezone"], s.timezone, sizeof(s.timezone));
  if (s.wakeBeforeSec >= s.intervalMin * 60)
    s.wakeBeforeSec = s.intervalMin * 60 / 2;

  JsonVariantConst src = in["source"];
  if (src["mode"].is<const char *>())
    s.sourceMode = strcmp(src["mode"], "pull") == 0 ? SOURCE_PULL : SOURCE_PUSH;
  getStr(src["ha_url"], s.haUrl, sizeof(s.haUrl));
  size_t ul = strlen(s.haUrl);
  while (ul && s.haUrl[ul - 1] == '/')
    s.haUrl[--ul] = 0;
  getSecret(src["ha_token"], s.haToken, sizeof(s.haToken));
  getSecret(src["push_token"], s.pushToken, sizeof(s.pushToken));
  getBool(src["insecure_tls"], s.haInsecureTls);

  JsonVariantConst m = src["map"];
  if (m.is<JsonObjectConst>())
  {
    for (const MapField &f : MAP_FIELDS)
      getStr(m[f.key], (char *)&s.map + f.off, ENTITY_LEN);
    getStr(m["price_series_attr"], s.map.priceSeriesAttr, sizeof(s.map.priceSeriesAttr));
    getStr(m["calendars"], s.map.calendars, sizeof(s.map.calendars));
    getStr(m["normal_openings"], s.map.normalOpenings, sizeof(s.map.normalOpenings));
    getBool(m["auto_discover"], s.map.autoDiscover);
    if (m["rooms"].is<JsonArrayConst>())
      s.map.roomCount = getBindings(m["rooms"], s.map.rooms, MAX_ROOMS, "temp", "hum");
    if (m["family"].is<JsonArrayConst>())
      s.map.familyCount = getBindings(m["family"], s.map.family, MAX_FAMILY, "entity", nullptr);
    if (m["extras"].is<JsonArrayConst>())
      s.map.extraCount = getBindings(m["extras"], s.map.extras, MAX_EXTRAS, "entity", nullptr);
  }

  JsonVariantConst disp = in["display"];
  if (disp["theme"].is<const char *>() && isThemeId(disp["theme"]))
    strlcpy(s.theme, disp["theme"], sizeof(s.theme));
  if (disp["slots"].is<JsonArrayConst>())
  {
    int i = 0;
    for (JsonVariantConst v : disp["slots"].as<JsonArrayConst>())
    {
      if (i >= DASH_SLOTS)
        break;
      const char *id = v | "none";
      strlcpy(s.slots[i++], isWidgetId(id) ? id : "none", sizeof(s.slots[0]));
    }
    for (; i < DASH_SLOTS; i++)
      strlcpy(s.slots[i], "none", sizeof(s.slots[0]));
  }
  getBool(disp["system_bar"], s.systemBar);
  if (disp["refresh"].is<const char *>())
    s.fastRefresh = strcmp(disp["refresh"], "fast") == 0;
  getBool(disp["skip_unchanged"], s.skipUnchanged);
  getStr(disp["price_unit"], s.priceUnit, sizeof(s.priceUnit));
  getStr(disp["wind_unit"], s.windUnit, sizeof(s.windUnit));

  JsonVariantConst th = in["thresholds"];
  if (th.is<JsonObjectConst>())
  {
    for (const ThField &f : TH_FIELDS)
      getFloat(th[f.key], *(float *)((char *)&s.th + f.off));
    getNum<int>(th["rssi_low"], s.th.rssiLow, -120, 0);
  }
  return true;
}

void settingsToJson(const Settings &s, JsonObject out, bool mask)
{
  JsonObject dev = out["device"].to<JsonObject>();
  dev["name"] = s.name;
  dev["hostname"] = s.hostname;
  dev["language"] = s.language;
  dev["date_format"] = s.dateFormat;
  dev["admin_user"] = s.adminUser;
  dev["admin_password"] = secretOut(s.adminPass, mask);

  JsonObject wifi = out["wifi"].to<JsonObject>();
  wifi["ssid"] = s.ssid;
  wifi["password"] = secretOut(s.wifiPass, mask);

  JsonObject pw = out["power"].to<JsonObject>();
  pw["mode"] = s.powerMode == POWER_ALWAYS_ON ? "always_on" : "sleep";
  pw["interval_min"] = s.intervalMin;
  pw["wake_before_s"] = s.wakeBeforeSec;
  pw["listen_window_s"] = s.listenWindowSec;
  pw["quiet_start"] = s.quietStart;
  pw["quiet_end"] = s.quietEnd;
  pw["timezone"] = s.timezone;

  JsonObject src = out["source"].to<JsonObject>();
  src["mode"] = s.sourceMode == SOURCE_PULL ? "pull" : "push";
  src["ha_url"] = s.haUrl;
  src["ha_token"] = secretOut(s.haToken, mask);
  src["push_token"] = secretOut(s.pushToken, mask);
  src["insecure_tls"] = s.haInsecureTls;
  JsonObject m = src["map"].to<JsonObject>();
  for (const MapField &f : MAP_FIELDS)
    m[f.key] = (const char *)&s.map + f.off;
  m["price_series_attr"] = s.map.priceSeriesAttr;
  m["calendars"] = s.map.calendars;
  m["normal_openings"] = s.map.normalOpenings;
  m["auto_discover"] = s.map.autoDiscover;
  putBindings(m["rooms"].to<JsonArray>(), s.map.rooms, s.map.roomCount, "temp", "hum");
  putBindings(m["family"].to<JsonArray>(), s.map.family, s.map.familyCount, "entity", nullptr);
  putBindings(m["extras"].to<JsonArray>(), s.map.extras, s.map.extraCount, "entity", nullptr);

  JsonObject disp = out["display"].to<JsonObject>();
  disp["theme"] = s.theme;
  JsonArray slots = disp["slots"].to<JsonArray>();
  for (int i = 0; i < DASH_SLOTS; i++)
    slots.add(s.slots[i]);
  disp["system_bar"] = s.systemBar;
  disp["refresh"] = s.fastRefresh ? "fast" : "quality";
  disp["skip_unchanged"] = s.skipUnchanged;
  disp["price_unit"] = s.priceUnit;
  disp["wind_unit"] = s.windUnit;

  JsonObject th = out["thresholds"].to<JsonObject>();
  for (const ThField &f : TH_FIELDS)
    th[f.key] = *(const float *)((const char *)&s.th + f.off);
  th["rssi_low"] = s.th.rssiLow;
}
