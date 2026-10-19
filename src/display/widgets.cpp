#include "display/widgets.h"
#include "display/glyphs.h"
#include "display/drawing.h"
#include "utils/utils.h"
#include "lang.h"

const char *weekdayName(const DashboardData &d, int offset, bool longName)
{
  if (d.weekday < 0)
    return "";
  int w = ((d.weekday + offset) % 7 + 7) % 7;
  return longName ? lang.weekdays_long[w] : lang.weekdays[w];
}

void fmtTemp(char *out, size_t n, float v, int decimals, bool unit)
{
  if (isnan(v))
    snprintf(out, n, "--");
  else
    snprintf(out, n, "%.*f%s", decimals, v, unit ? "\xC2\xB0" "C" : "\xC2\xB0");
}

static WRow &addRow(WidgetView &v, const char *glyph, const char *label, bool alert = false)
{
  WRow &r = v.rows[v.n < WIDGET_MAX_ROWS ? v.n++ : WIDGET_MAX_ROWS - 1];
  memset(&r, 0, sizeof(r));
  r.glyph = glyph;
  strlcpy(r.label, label, sizeof(r.label));
  r.alert = alert;
  return r;
}

static void numRow(WidgetView &v, const char *glyph, const char *label, float val, int dec,
                   const char *unit, bool alert)
{
  WRow &r = addRow(v, glyph, label, alert);
  fmtNum(r.value, sizeof(r.value), val, dec, unit);
}

static void hero(WidgetView &v, float val, int dec, const char *unit, bool alert)
{
  if (isnan(val))
    strlcpy(v.hero, "--", sizeof(v.hero));
  else
    snprintf(v.hero, sizeof(v.hero), "%.*f", dec, val);
  strlcpy(v.heroUnit, unit, sizeof(v.heroUnit));
  v.heroAlert = alert;
}

// ── builders ───────────────────────────────────────────────────

static void buildOutdoor(const DashboardData &d, const Settings &s, WidgetView &v)
{
  const Thresholds &t = s.th;
  v.title = lang.title_outdoor;
  v.heroIcon = getWeatherIcon(d.weather.state);
  strlcpy(v.caption, d.weather.state[0] ? translateWeatherState(d.weather.state) : "", sizeof(v.caption));
  hero(v, d.weather.temp, 1, "\xC2\xB0" "C", alertOutTemp(t, d.weather.temp));
  numRow(v, G_DROP, lang.label_humidity, d.weather.hum, 0, "%", false);
  numRow(v, G_PRESS, lang.label_pressure, d.weather.press, 0, "hPa", false);
  numRow(v, G_WIND, lang.label_wind, d.weather.wind, 1, s.windUnit, alertWind(t, d.weather.wind));
  numRow(v, G_WIND, lang.label_gusts, d.weather.gust, 1, s.windUnit, alertGust(t, d.weather.gust));
  numRow(v, G_CLOUD, lang.label_clouds, d.weather.clouds, 0, "%", false);
  v.empty = isnan(d.weather.temp) && !d.weather.state[0];
}

static void buildIndoor(const DashboardData &d, const Settings &s, WidgetView &v)
{
  v.title = lang.title_indoor;
  v.kind = WK_TABLE;
  float sum = 0;
  int cnt = 0;
  for (int i = 0; i < d.roomCount; i++)
  {
    const Room &rm = d.rooms[i];
    // Historic rule for the v1 "bath" key: bathrooms are warm, only humidity alerts.
    bool bath = strcmp(rm.name, "@bath") == 0;
    bool alert = (!bath && alertInTemp(s.th, rm.temp)) || alertInHum(s.th, rm.hum);
    WRow &r = addRow(v, G_HOUSE, translateName(rm.name), alert);
    fmtTemp(r.value, sizeof(r.value), rm.temp, 1, false);
    fmtNum(r.value2, sizeof(r.value2), rm.hum, 0, "");
    if (!isnan(rm.hum))
      strlcat(r.value2, "%", sizeof(r.value2));
    if (!isnan(rm.temp))
    {
      sum += rm.temp;
      cnt++;
    }
  }
  hero(v, cnt ? sum / cnt : NAN, 1, "\xC2\xB0" "C", false);
  v.empty = d.roomCount == 0;
}

static void buildForecast(const DashboardData &d, const Settings &, WidgetView &v)
{
  v.title = lang.title_forecast;
  v.kind = WK_FORECAST;
  for (int i = 0; i < d.forecastCount; i++)
  {
    const ForecastDay &f = d.forecast[i];
    WRow &r = addRow(v, "", weekdayName(d, i + 1), false);
    if (!r.label[0])
      snprintf(r.label, sizeof(r.label), "+%d", i + 1);
    r.icon = getWeatherIcon(f.condition);
    if (isnan(f.tempHigh))
      strlcpy(r.value, "--", sizeof(r.value));
    else
      snprintf(r.value, sizeof(r.value), "%.0f..%.0f\xC2\xB0", f.tempLow, f.tempHigh);
    fmtNum(r.value2, sizeof(r.value2), f.precipitation, f.precipitation < 10 && f.precipitation > 0 ? 1 : 0, "mm");
    strlcpy(v.caption, "", sizeof(v.caption));
  }
  v.empty = d.forecastCount == 0;
}

static void buildFamily(const DashboardData &d, const Settings &, WidgetView &v)
{
  v.title = lang.title_location;
  v.kind = WK_PEOPLE;
  int home = 0;
  for (int i = 0; i < d.familyCount; i++)
  {
    const Person &p = d.family[i];
    bool h = strcmp(p.state, "home") == 0;
    home += h;
    WRow &r = addRow(v, h ? G_FILLED : G_EMPTY, p.name, !h);
    r.on = h;
    strlcpy(r.value, translateFamilyState(p.state), sizeof(r.value));
  }
  snprintf(v.hero, sizeof(v.hero), "%d/%d", home, d.familyCount);
  strlcpy(v.caption, home == d.familyCount ? lang.label_all_home : (home == 0 ? lang.label_nobody_home : ""),
          sizeof(v.caption));
  v.empty = d.familyCount == 0;
}

static void buildNetwork(const DashboardData &d, const Settings &s, WidgetView &v)
{
  const Thresholds &t = s.th;
  v.title = lang.title_network;
  hero(v, d.net.down, d.net.down >= 100 ? 0 : 1, "Mbps", alertDown(t, d.net.down));
  numRow(v, G_DOWN, lang.label_download, d.net.down, 2, "Mbps", alertDown(t, d.net.down));
  numRow(v, G_UP, lang.label_upload, d.net.up, 2, "Mbps", alertUp(t, d.net.up));
  numRow(v, G_SIGNAL, lang.label_ping, d.net.ping, 0, "ms", alertPing(t, d.net.ping));
  v.empty = isnan(d.net.down) && isnan(d.net.up) && isnan(d.net.ping);
}

static void buildPower(const DashboardData &d, const Settings &s, WidgetView &v)
{
  const Thresholds &t = s.th;
  const Power &p = d.power;
  v.title = lang.title_electricity;
  hero(v, p.total, 0, "W", alertPower(t, p.total));
  numRow(v, G_PRICE, lang.label_price, p.price, p.price < 10 ? 3 : 1, s.priceUnit, alertPrice(t, p.price));
  numRow(v, G_BOLT, lang.label_power, p.total, 0, "W", alertPower(t, p.total));
  numRow(v, G_VOLT, lang.label_voltage, p.voltage, 0, "V", alertVoltage(t, p.voltage));
  numRow(v, G_FREQ, lang.label_frequency, p.frequency, 2, "Hz", alertFrequency(t, p.frequency));
  float pf = normFactor(p.factor);
  numRow(v, G_PHI, lang.label_power_factor, pf, 2, "", alertFactor(t, p.factor));
  if (!isnan(p.today))
    numRow(v, G_METER, lang.label_today, p.today, 1, "kWh", false);
  numRow(v, G_METER, lang.label_month, p.month, 1, "kWh", alertMonth(t, p.month));
  v.empty = isnan(p.total) && isnan(p.price) && isnan(p.voltage);
}

static void buildPrices(const DashboardData &d, const Settings &s, WidgetView &v)
{
  v.title = lang.title_prices;
  v.kind = WK_CHART;
  v.series = &d.prices;
  const Series &se = d.prices;
  float mn = NAN, mx = NAN;
  for (int i = 0; i < se.count; i++)
  {
    if (isnan(se.v[i]))
      continue;
    if (isnan(mn) || se.v[i] < mn) mn = se.v[i];
    if (isnan(mx) || se.v[i] > mx) mx = se.v[i];
  }
  v.sMin = mn;
  v.sMax = mx;
  float now = se.count ? se.v[se.now] : d.power.price;
  int dec = (!isnan(now) && fabsf(now) < 10) ? 3 : 1;
  hero(v, now, dec, s.priceUnit, alertPrice(s.th, now));
  numRow(v, G_ARROW_R, lang.label_now, now, dec, s.priceUnit, alertPrice(s.th, now));
  numRow(v, G_TRI_DOWN, lang.label_min, mn, dec, s.priceUnit, false);
  numRow(v, G_TRI_UP, lang.label_max, mx, dec, s.priceUnit, alertPrice(s.th, mx));
  v.empty = se.count == 0 && isnan(d.power.price);
}

static void buildExtras(const DashboardData &d, const Settings &, WidgetView &v)
{
  v.title = lang.title_extras;
  for (int i = 0; i < d.extraCount; i++)
  {
    const Extra &e = d.extras[i];
    WRow &r = addRow(v, G_BULLET, e.label, false);
    snprintf(r.value, sizeof(r.value), "%s%s%s", e.value, e.unit[0] ? " " : "", e.unit);
  }
  v.empty = d.extraCount == 0;
}

static void buildAgenda(const DashboardData &d, const Settings &, WidgetView &v)
{
  v.title = lang.title_agenda;
  strlcpy(v.hero, d.time, sizeof(v.hero));
  snprintf(v.caption, sizeof(v.caption), "%s", weekdayName(d, 0, true));
  {
    WRow &r = addRow(v, G_ARROW_R, weekdayName(d, 0, true), false);
    strlcpy(r.value, d.date, sizeof(r.value));
  }
  if (d.sunrise[0])
  {
    WRow &r = addRow(v, G_SUN, lang.label_sunrise, false);
    strlcpy(r.value, d.sunrise, sizeof(r.value));
  }
  if (d.sunset[0])
  {
    WRow &r = addRow(v, G_EMPTY, lang.label_sunset, false);
    strlcpy(r.value, d.sunset, sizeof(r.value));
  }
  if (d.message[0])
  {
    // message spans the full row: put it in the label, wrap in themes
    WRow &r = addRow(v, G_DIAMOND, "", false);
    utf8Clip(r.label, sizeof(r.label), d.message, 27);
    if ((int)strlen(d.message) > (int)strlen(r.label))
      utf8Clip(r.value, sizeof(r.value), d.message + strlen(r.label), 23);
  }
  v.empty = !d.valid;
}

static void buildSystem(const DashboardData &d, const Settings &s, WidgetView &v)
{
  v.title = lang.title_system;
  const SystemStats &y = d.sys;
  {
    WRow &r = addRow(v, G_HOUSE, "IP", false);
    strlcpy(r.value, y.ip[0] ? y.ip : "--", sizeof(r.value));
  }
  {
    WRow &r = addRow(v, G_SIGNAL, lang.label_signal, alertRssi(s.th, y.rssi));
    snprintf(r.value, sizeof(r.value), "%d dBm", (int)y.rssi);
  }
  {
    WRow &r = addRow(v, G_METER, lang.label_heap, false);
    snprintf(r.value, sizeof(r.value), "%lu KB", (unsigned long)(y.heapFree / 1024));
  }
  {
    WRow &r = addRow(v, G_ARROW_R, lang.label_wakes, false);
    snprintf(r.value, sizeof(r.value), "#%lu", (unsigned long)y.wakeCount);
  }
  {
    WRow &r = addRow(v, G_BULLET, "FW", false);
    strlcpy(r.value, FW_VERSION, sizeof(r.value));
  }
  snprintf(v.hero, sizeof(v.hero), "%d", (int)y.rssi);
  strlcpy(v.heroUnit, "dBm", sizeof(v.heroUnit));
}

static void buildHourly(const DashboardData &d, const Settings &, WidgetView &v)
{
  v.title = lang.title_hourly;
  v.kind = WK_HOURLY;
  const Hourly &h = d.hourly;
  float mn = NAN, mx = NAN, rain = 0;
  for (int i = 0; i < h.count; i++)
  {
    if (!isnan(h.temp[i]))
    {
      if (isnan(mn) || h.temp[i] < mn) mn = h.temp[i];
      if (isnan(mx) || h.temp[i] > mx) mx = h.temp[i];
    }
    if (!isnan(h.precip[i]))
      rain += h.precip[i];
  }
  v.sMin = mn;
  v.sMax = mx;
  hero(v, h.count ? h.temp[0] : NAN, 0, "\xC2\xB0" "C", false);
  numRow(v, G_TRI_DOWN, lang.label_min, mn, 0, "\xC2\xB0" "C", false);
  numRow(v, G_TRI_UP, lang.label_max, mx, 0, "\xC2\xB0" "C", false);
  numRow(v, G_DROP, "", rain, 1, "mm", false);
  v.empty = h.count == 0;
}

static void buildHouse(const DashboardData &d, const Settings &, WidgetView &v)
{
  v.title = lang.title_house;
  const HouseStatus &h = d.house;
  char b[24];
  if (h.lightsOn >= 0)
  {
    WRow &r = addRow(v, G_SUN, lang.label_lights, false);
    snprintf(r.value, sizeof(r.value), "%d", h.lightsOn);
  }
  if (h.openCount >= 0)
  {
    WRow &r = addRow(v, G_SQUARE, lang.label_open, h.openCount > 0);
    if (h.openCount == 0)
      strlcpy(r.value, lang.label_all_closed, sizeof(r.value));
    else
      snprintf(r.value, sizeof(r.value), "%d", h.openCount);
    for (int i = 0; i < MAX_NAMES && h.open[i][0] && h.openCount > 0; i++)
    {
      WRow &n = addRow(v, "", "", true);
      utf8Clip(n.label, sizeof(n.label), h.open[i], 26);
    }
  }
  if (h.lowBattery >= 0)
  {
    WRow &r = addRow(v, G_BOLT, lang.label_battery, h.lowBattery > 0);
    snprintf(r.value, sizeof(r.value), "%d", h.lowBattery);
    for (int i = 0; i < 2 && h.battery[i][0] && h.lowBattery > 0; i++)
    {
      WRow &n = addRow(v, "", "", true);
      utf8Clip(n.label, sizeof(n.label), h.battery[i], 26);
    }
  }
  if (h.updates > 0)
  {
    WRow &r = addRow(v, G_UP, lang.label_updates, false);
    snprintf(r.value, sizeof(r.value), "%d", h.updates);
  }
  if (h.media[0])
  {
    WRow &r = addRow(v, G_ARROW_R, "", false);
    utf8Clip(r.label, sizeof(r.label), h.media, 26);
  }
  snprintf(b, sizeof(b), "%d", h.openCount < 0 ? 0 : h.openCount);
  strlcpy(v.hero, b, sizeof(v.hero));
  v.heroAlert = h.openCount > 0;
  v.empty = h.lightsOn < 0 && h.openCount < 0 && h.lowBattery < 0;
}

static void buildCalendar(const DashboardData &d, const Settings &, WidgetView &v)
{
  v.title = lang.title_calendar;
  for (int i = 0; i < d.eventCount; i++)
  {
    WRow &r = addRow(v, G_DIAMOND, "", false);
    strlcpy(r.value, d.events[i].when, sizeof(r.value));
    utf8Clip(r.label, sizeof(r.label), d.events[i].title, 26);
  }
  if (!d.eventCount && d.valid)
  {
    WRow &r = addRow(v, G_EMPTY, lang.label_no_events, false);
    (void)r;
  }
  v.empty = !d.valid;
}

static void buildHome(const DashboardData &d, const Settings &, WidgetView &v)
{
  v.title = lang.title_home;
  v.kind = WK_HOME;
  const Thermostat &t = d.thermostat;
  int home = 0;
  for (int i = 0; i < d.familyCount; i++)
    home += strcmp(d.family[i].state, "home") == 0;
  hero(v, t.target, 1, "\xC2\xB0" "C", false);
  strlcpy(v.caption, translateHvac(t.action, t.mode), sizeof(v.caption));
  if (d.familyCount)
  {
    WRow &r = addRow(v, G_HOUSE, lang.label_people, false);
    snprintf(r.value, sizeof(r.value), "%d/%d", home, d.familyCount);
  }
  if (!isnan(t.target))
  {
    WRow &r = addRow(v, G_THERMO, lang.label_thermostat, false);
    fmtTemp(r.value, sizeof(r.value), t.target, 1, true);
    WRow &c = addRow(v, G_THERMO, lang.label_now, false);
    fmtTemp(c.value, sizeof(c.value), t.current, 1, true);
    if (v.caption[0])
    {
      WRow &a = addRow(v, G_ARROW_R, v.caption, false);
      (void)a;
    }
  }
  // daily temperature range from the hourly forecast (mini chart data)
  float mn = NAN, mx = NAN;
  for (int i = 0; i < d.hourly.count; i++)
  {
    float x = d.hourly.temp[i];
    if (isnan(x))
      continue;
    if (isnan(mn) || x < mn) mn = x;
    if (isnan(mx) || x > mx) mx = x;
  }
  v.sMin = mn;
  v.sMax = mx;
  if (!isnan(mn))
  {
    WRow &r = addRow(v, G_TRI_UP, lang.title_hourly, false);
    snprintf(r.value, sizeof(r.value), "%.0f..%.0f\xC2\xB0" "C", mn, mx);
  }
  v.empty = !d.familyCount && isnan(t.target) && !d.hourly.count;
}

bool buildWidget(const char *id, const DashboardData &d, const Settings &s, WidgetView &v)
{
  memset(&v, 0, sizeof(v));
  v.id = id;
  v.title = "";
  v.sMin = v.sMax = NAN;
  if (strcmp(id, "outdoor") == 0) buildOutdoor(d, s, v);
  else if (strcmp(id, "indoor") == 0) buildIndoor(d, s, v);
  else if (strcmp(id, "forecast") == 0) buildForecast(d, s, v);
  else if (strcmp(id, "family") == 0) buildFamily(d, s, v);
  else if (strcmp(id, "network") == 0) buildNetwork(d, s, v);
  else if (strcmp(id, "power") == 0) buildPower(d, s, v);
  else if (strcmp(id, "prices") == 0) buildPrices(d, s, v);
  else if (strcmp(id, "extras") == 0) buildExtras(d, s, v);
  else if (strcmp(id, "agenda") == 0) buildAgenda(d, s, v);
  else if (strcmp(id, "system") == 0) buildSystem(d, s, v);
  else if (strcmp(id, "hourly") == 0) buildHourly(d, s, v);
  else if (strcmp(id, "house") == 0) buildHouse(d, s, v);
  else if (strcmp(id, "calendar") == 0) buildCalendar(d, s, v);
  else if (strcmp(id, "home") == 0) buildHome(d, s, v);
  else
    return false;
  return true;
}

StatusFlags computeStatus(const DashboardData &d, const Settings &s)
{
  StatusFlags f;
  memset(&f, 0, sizeof(f));
  const Thresholds &t = s.th;
  f.people = d.familyCount;
  for (int i = 0; i < d.familyCount; i++)
    f.home += strcmp(d.family[i].state, "home") == 0;
  f.frost = has(d.weather.temp) && d.weather.temp <= 0;
  f.heat = has(d.weather.temp) && d.weather.temp > t.outTempHigh;
  f.windy = alertWind(t, d.weather.wind) || alertGust(t, d.weather.gust);
  f.rain = strstr(d.weather.state, "rain") || strstr(d.weather.state, "pouring") ||
           strstr(d.weather.state, "lightning") ||
           (d.forecastCount && has(d.forecast[0].precipitation) && d.forecast[0].precipitation >= 1);
  bool roomAlert = false;
  for (int i = 0; i < d.roomCount; i++)
  {
    bool bath = strcmp(d.rooms[i].name, "@bath") == 0;
    if ((!bath && alertInTemp(t, d.rooms[i].temp)) || alertInHum(t, d.rooms[i].hum))
      roomAlert = true;
    if (has(d.rooms[i].hum) && d.rooms[i].hum > t.inHumHigh)
      f.humid = true;
  }
  f.netBad = alertDown(t, d.net.down) || alertUp(t, d.net.up) || alertPing(t, d.net.ping);
  f.powerHigh = alertPower(t, d.power.total) || alertVoltage(t, d.power.voltage) ||
                alertFrequency(t, d.power.frequency);
  f.priceHigh = alertPrice(t, d.power.price);
  f.open = d.house.openCount > 0;
  f.alerts = alertOutTemp(t, d.weather.temp) + f.windy + roomAlert + f.netBad + f.powerHigh + f.priceHigh;
  return f;
}
