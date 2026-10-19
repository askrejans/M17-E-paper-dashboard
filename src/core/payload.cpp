#include "payload.h"
#include "utils/utils.h"

void dataReset(DashboardData &d)
{
  memset(&d, 0, sizeof(d));
  d.weekday = -1;
  float *wf[] = {&d.weather.temp, &d.weather.hum, &d.weather.press, &d.weather.wind,
                 &d.weather.gust, &d.weather.clouds, &d.net.down, &d.net.up, &d.net.ping,
                 &d.power.price, &d.power.total, &d.power.voltage, &d.power.frequency,
                 &d.power.factor, &d.power.month, &d.power.today};
  for (float *f : wf)
    *f = NAN;
  d.sys.chipTemp = NAN;
  d.thermostat.target = d.thermostat.current = NAN;
  d.hourly.startHour = -1;
  d.house.lightsOn = d.house.openCount = d.house.lowBattery = d.house.updates = -1;
}

float jf(JsonVariantConst v)
{
  if (v.is<float>() || v.is<int>() || v.is<long>())
    return v.as<float>();
  if (v.is<const char *>())
    return parseNumber(v.as<const char *>());
  return NAN;
}

static void str(JsonVariantConst v, char *dst, size_t n, const char *def = "")
{
  if (v.is<const char *>())
    strlcpy(dst, v.as<const char *>(), n);
  else if (v.is<float>() || v.is<int>())
    snprintf(dst, n, "%g", v.as<double>());
  else
    strlcpy(dst, def, n);
}

static void parseRooms(JsonVariantConst rooms, DashboardData &d)
{
  d.roomCount = 0;
  if (rooms.is<JsonArrayConst>())
  {
    for (JsonVariantConst r : rooms.as<JsonArrayConst>())
    {
      if (d.roomCount >= MAX_ROOMS)
        break;
      Room &x = d.rooms[d.roomCount++];
      str(r["name"], x.name, sizeof(x.name), "?");
      x.temp = jf(r["temp"]);
      x.hum = jf(r["hum"].isNull() ? r["humidity"] : r["hum"]);
    }
    return;
  }
  if (!rooms.is<JsonObjectConst>())
    return;
  // v1: object keyed by room. Known keys first, in their historic order,
  // stored as '@key' so the label follows the display language.
  static const char *const known[] = {"living", "bedroom", "kids", "bath", "office"};
  JsonObjectConst obj = rooms.as<JsonObjectConst>();
  for (const char *k : known)
  {
    JsonVariantConst r = obj[k];
    if (r.isNull() || d.roomCount >= MAX_ROOMS)
      continue;
    Room &x = d.rooms[d.roomCount++];
    snprintf(x.name, sizeof(x.name), "@%s", k);
    x.temp = jf(r["temp"]);
    x.hum = jf(r["hum"]);
  }
  for (JsonPairConst kv : obj)
  {
    bool isKnown = false;
    for (const char *k : known)
      if (strcmp(kv.key().c_str(), k) == 0)
        isKnown = true;
    if (isKnown || d.roomCount >= MAX_ROOMS)
      continue;
    Room &x = d.rooms[d.roomCount++];
    strlcpy(x.name, kv.key().c_str(), sizeof(x.name));
    if (x.name[0] >= 'a' && x.name[0] <= 'z')
      x.name[0] -= 32;
    x.temp = jf(kv.value()["temp"]);
    x.hum = jf(kv.value()["hum"]);
  }
}

static void parseForecast(JsonVariantConst arr, DashboardData &d)
{
  d.forecastCount = 0;
  for (JsonVariantConst f : arr.as<JsonArrayConst>())
  {
    if (d.forecastCount >= MAX_FORECAST)
      break;
    ForecastDay &x = d.forecast[d.forecastCount++];
    str(f["condition"], x.condition, sizeof(x.condition), "unknown");
    // accept both our names and native HA forecast names
    x.tempHigh = jf(f["temp_high"].isNull() ? f["temperature"] : f["temp_high"]);
    x.tempLow = jf(f["temp_low"].isNull() ? f["templow"] : f["temp_low"]);
    if (isnan(x.tempLow))
      x.tempLow = x.tempHigh;
    x.precipitation = jf(f["precipitation"]);
  }
}

void parseHaForecast(JsonArrayConst list, DashboardData &d, const char *todayIso)
{
  d.forecastCount = 0;
  for (JsonVariantConst f : list)
  {
    if (d.forecastCount >= MAX_FORECAST)
      break;
    const char *dt = f["datetime"] | "";
    if (todayIso && todayIso[0] && strncmp(dt, todayIso, 10) <= 0 && strlen(dt) >= 10)
      continue;
    ForecastDay &x = d.forecast[d.forecastCount++];
    str(f["condition"], x.condition, sizeof(x.condition), "unknown");
    x.tempHigh = jf(f["temperature"]);
    x.tempLow = jf(f["templow"]);
    if (isnan(x.tempLow))
      x.tempLow = x.tempHigh;
    x.precipitation = jf(f["precipitation"]);
  }
}

void parseHaHourly(JsonArrayConst list, DashboardData &d)
{
  d.hourly.count = 0;
  d.hourly.startHour = -1;
  for (JsonVariantConst f : list)
  {
    if (d.hourly.count >= MAX_HOURLY)
      break;
    if (d.hourly.count == 0)
    {
      char hm[6];
      if (extractHHMM(f["datetime"] | "", hm))
        d.hourly.startHour = (hm[0] - '0') * 10 + (hm[1] - '0');
    }
    d.hourly.temp[d.hourly.count] = jf(f["temperature"]);
    d.hourly.precip[d.hourly.count] = jf(f["precipitation"]);
    d.hourly.count++;
  }
}

static void parseSeries(JsonVariantConst v, Series &s)
{
  s.count = 0;
  s.now = 0;
  JsonArrayConst arr;
  if (v.is<JsonArrayConst>())
    arr = v.as<JsonArrayConst>();
  else if (v.is<JsonObjectConst>())
  {
    arr = v["values"].as<JsonArrayConst>();
    s.now = v["now"] | 0;
  }
  for (JsonVariantConst x : arr)
  {
    if (s.count >= MAX_SERIES)
      break;
    // Nord Pool "raw_today" style objects: {"start":..., "value": 0.1}
    s.v[s.count++] = x.is<JsonObjectConst>() ? jf(x["value"]) : jf(x);
  }
  if (s.now >= s.count)
    s.now = s.count ? s.count - 1 : 0;
}

bool parsePayload(JsonVariantConst doc, DashboardData &d, const char *dateFormat)
{
  if (!doc.is<JsonObjectConst>())
    return false;
  dataReset(d);

  // date / time
  const char *rawDate = doc["date"] | "";
  formatDate(rawDate, d.date, sizeof(d.date), dateFormat);
  str(doc["time"], d.time, sizeof(d.time), "--:--");
  d.epoch = doc["epoch"] | (int64_t)0;
  int dd, mm, yy;
  if (parseDate(rawDate, dd, mm, yy))
    d.weekday = (int8_t)dayOfWeek(yy, mm, dd);

  JsonVariantConst w = doc["weather"];
  str(w["state"], d.weather.state, sizeof(d.weather.state), "");
  d.weather.temp = jf(w["temp"]);
  d.weather.hum = jf(w["humidity"]);
  d.weather.press = jf(w["pressure"]);
  d.weather.clouds = jf(w["clouds"]);
  d.weather.wind = jf(w["wind"]);
  d.weather.gust = jf(w["gust"]);

  parseRooms(doc["rooms"], d);

  d.familyCount = 0;
  for (JsonVariantConst p : doc["family"].as<JsonArrayConst>())
  {
    if (d.familyCount >= MAX_FAMILY)
      break;
    Person &x = d.family[d.familyCount++];
    str(p["name"], x.name, sizeof(x.name), "?");
    str(p["state"], x.state, sizeof(x.state), "unknown");
  }

  parseForecast(doc["forecast"], d);

  JsonVariantConst n = doc["internet"];
  d.net.down = jf(n["down"]);
  d.net.up = jf(n["up"]);
  d.net.ping = jf(n["ping"]);

  JsonVariantConst p = doc["power"];
  d.power.price = jf(p["price"]);
  d.power.total = jf(p["total_power"]);
  d.power.voltage = jf(p["voltage"]);
  d.power.frequency = jf(p["frequency"]);
  d.power.factor = jf(p["power_factor"]);
  d.power.month = jf(p["monthly_consumption"]);
  d.power.today = jf(p["today_consumption"]);

  parseSeries(doc["prices"], d.prices);

  d.extraCount = 0;
  for (JsonVariantConst e : doc["extras"].as<JsonArrayConst>())
  {
    if (d.extraCount >= MAX_EXTRAS)
      break;
    Extra &x = d.extras[d.extraCount++];
    str(e["label"], x.label, sizeof(x.label), "?");
    str(e["value"], x.value, sizeof(x.value), "--");
    str(e["unit"], x.unit, sizeof(x.unit), "");
  }

  // hourly forecast: {"start_hour": 15, "temp": [...], "precip": [...]}
  // or native HA list [{"datetime":..., "temperature":..., "precipitation":...}]
  JsonVariantConst hr = doc["hourly"];
  if (hr.is<JsonObjectConst>())
  {
    d.hourly.startHour = hr["start_hour"] | -1;
    JsonArrayConst t = hr["temp"].as<JsonArrayConst>(), p = hr["precip"].as<JsonArrayConst>();
    for (JsonVariantConst x : t)
      if (d.hourly.count < MAX_HOURLY)
        d.hourly.temp[d.hourly.count++] = jf(x);
    for (int i = 0; i < d.hourly.count; i++)
      d.hourly.precip[i] = i < (int)p.size() ? jf(p[i]) : NAN;
  }
  else if (hr.is<JsonArrayConst>())
    parseHaHourly(hr.as<JsonArrayConst>(), d);

  d.eventCount = 0;
  for (JsonVariantConst e : doc["events"].as<JsonArrayConst>())
  {
    if (d.eventCount >= MAX_EVENTS)
      break;
    Event &x = d.events[d.eventCount++];
    str(e["when"], x.when, sizeof(x.when), "");
    str(e["title"], x.title, sizeof(x.title), "");
  }

  JsonVariantConst h = doc["house"];
  if (h.is<JsonObjectConst>())
  {
    auto cnt = [](JsonVariantConst v) -> int16_t { return v.is<int>() ? (int16_t)v.as<int>() : -1; };
    auto names = [](JsonVariantConst arr, char (*out)[20]) {
      int i = 0;
      for (JsonVariantConst n : arr.as<JsonArrayConst>())
        if (i < MAX_NAMES)
          str(n, out[i++], 20, "");
    };
    d.house.lightsOn = cnt(h["lights_on"]);
    d.house.openCount = cnt(h["open_count"]);
    names(h["open"], d.house.open);
    d.house.lowBattery = cnt(h["low_battery_count"]);
    names(h["low_battery"], d.house.battery);
    d.house.updates = cnt(h["updates"]);
    str(h["media"], d.house.media, sizeof(d.house.media), "");
  }

  JsonVariantConst th = doc["thermostat"];
  if (th.is<JsonObjectConst>())
  {
    str(th["name"], d.thermostat.name, sizeof(d.thermostat.name), "");
    d.thermostat.target = jf(th["target"]);
    d.thermostat.current = jf(th["current"]);
    str(th["mode"], d.thermostat.mode, sizeof(d.thermostat.mode), "");
    str(th["action"], d.thermostat.action, sizeof(d.thermostat.action), "");
  }

  JsonVariantConst sun = doc["sun"];
  if (!extractHHMM(sun["rise"] | "", d.sunrise))
    d.sunrise[0] = 0;
  if (!extractHHMM(sun["set"] | "", d.sunset))
    d.sunset[0] = 0;

  str(doc["message"], d.message, sizeof(d.message), "");
  d.valid = true;
  return true;
}

static void putNum(JsonObject o, const char *k, float v)
{
  if (isnan(v))
    o[k] = nullptr;
  else
    o[k] = v;
}

void dataToJson(const DashboardData &d, JsonObject out)
{
  out["valid"] = d.valid;
  out["stale"] = d.stale;
  out["date"] = d.date;
  out["time"] = d.time;
  out["weekday"] = d.weekday;

  JsonObject w = out["weather"].to<JsonObject>();
  w["state"] = d.weather.state;
  putNum(w, "temp", d.weather.temp);
  putNum(w, "humidity", d.weather.hum);
  putNum(w, "pressure", d.weather.press);
  putNum(w, "clouds", d.weather.clouds);
  putNum(w, "wind", d.weather.wind);
  putNum(w, "gust", d.weather.gust);

  JsonArray rooms = out["rooms"].to<JsonArray>();
  for (int i = 0; i < d.roomCount; i++)
  {
    JsonObject r = rooms.add<JsonObject>();
    r["name"] = d.rooms[i].name;
    putNum(r, "temp", d.rooms[i].temp);
    putNum(r, "hum", d.rooms[i].hum);
  }

  JsonArray fam = out["family"].to<JsonArray>();
  for (int i = 0; i < d.familyCount; i++)
  {
    JsonObject p = fam.add<JsonObject>();
    p["name"] = d.family[i].name;
    p["state"] = d.family[i].state;
  }

  JsonArray fc = out["forecast"].to<JsonArray>();
  for (int i = 0; i < d.forecastCount; i++)
  {
    JsonObject f = fc.add<JsonObject>();
    f["condition"] = d.forecast[i].condition;
    putNum(f, "temp_low", d.forecast[i].tempLow);
    putNum(f, "temp_high", d.forecast[i].tempHigh);
    putNum(f, "precipitation", d.forecast[i].precipitation);
  }

  JsonObject n = out["internet"].to<JsonObject>();
  putNum(n, "down", d.net.down);
  putNum(n, "up", d.net.up);
  putNum(n, "ping", d.net.ping);

  JsonObject p = out["power"].to<JsonObject>();
  putNum(p, "price", d.power.price);
  putNum(p, "total_power", d.power.total);
  putNum(p, "voltage", d.power.voltage);
  putNum(p, "frequency", d.power.frequency);
  putNum(p, "power_factor", d.power.factor);
  putNum(p, "monthly_consumption", d.power.month);
  putNum(p, "today_consumption", d.power.today);

  JsonObject pr = out["prices"].to<JsonObject>();
  pr["now"] = d.prices.now;
  JsonArray pv = pr["values"].to<JsonArray>();
  for (int i = 0; i < d.prices.count; i++)
    pv.add(d.prices.v[i]);

  JsonArray ex = out["extras"].to<JsonArray>();
  for (int i = 0; i < d.extraCount; i++)
  {
    JsonObject e = ex.add<JsonObject>();
    e["label"] = d.extras[i].label;
    e["value"] = d.extras[i].value;
    e["unit"] = d.extras[i].unit;
  }

  JsonObject sun = out["sun"].to<JsonObject>();
  sun["rise"] = d.sunrise;
  sun["set"] = d.sunset;
  out["message"] = d.message;

  JsonObject hr = out["hourly"].to<JsonObject>();
  hr["start_hour"] = d.hourly.startHour;
  JsonArray ht = hr["temp"].to<JsonArray>(), hp = hr["precip"].to<JsonArray>();
  for (int i = 0; i < d.hourly.count; i++)
  {
    if (isnan(d.hourly.temp[i])) ht.add(nullptr); else ht.add(d.hourly.temp[i]);
    if (isnan(d.hourly.precip[i])) hp.add(nullptr); else hp.add(d.hourly.precip[i]);
  }

  JsonArray ev = out["events"].to<JsonArray>();
  for (int i = 0; i < d.eventCount; i++)
  {
    JsonObject e = ev.add<JsonObject>();
    e["when"] = d.events[i].when;
    e["title"] = d.events[i].title;
  }

  JsonObject h = out["house"].to<JsonObject>();
  h["lights_on"] = d.house.lightsOn;
  h["open_count"] = d.house.openCount;
  JsonArray op = h["open"].to<JsonArray>();
  for (int i = 0; i < MAX_NAMES && d.house.open[i][0]; i++)
    op.add(d.house.open[i]);
  h["low_battery_count"] = d.house.lowBattery;
  JsonArray lb = h["low_battery"].to<JsonArray>();
  for (int i = 0; i < MAX_NAMES && d.house.battery[i][0]; i++)
    lb.add(d.house.battery[i]);
  h["updates"] = d.house.updates;
  h["media"] = d.house.media;

  JsonObject t = out["thermostat"].to<JsonObject>();
  t["name"] = d.thermostat.name;
  putNum(t, "target", d.thermostat.target);
  putNum(t, "current", d.thermostat.current);
  t["mode"] = d.thermostat.mode;
  t["action"] = d.thermostat.action;
}
