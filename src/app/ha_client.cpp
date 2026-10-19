#include "app/ha_client.h"
#include "app/log.h"
#include "ha_template.h"
#include "payload.h"
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <sys/time.h>

HaStatus haLast = {false, 0, 0, ""};

static int request(const char *base, const char *token, bool insecure, const char *method, const char *path,
                   const String &body, String &out)
{
  String url = String(base) + path;
  bool tls = url.startsWith("https://");
  WiFiClient plain;
  WiFiClientSecure secure;
  if (tls && insecure)
    secure.setInsecure();
  HTTPClient http;
  http.setTimeout(12000);
  http.setReuse(false);
  if (!http.begin(tls ? (WiFiClient &)secure : plain, url))
    return -100;
  http.addHeader("Authorization", String("Bearer ") + token);
  http.addHeader("Content-Type", "application/json");
  int code = http.sendRequest(method, body);
  if (code > 0)
    out = http.getString();
  else
    out = http.errorToString(code);
  http.end();
  return code;
}

int haRequest(const Settings &s, const char *method, const char *path, const String &body, String &out)
{
  if (!s.haUrl[0] || !s.haToken[0])
  {
    out = "Home Assistant URL or token not configured";
    return -1;
  }
  return request(s.haUrl, s.haToken, s.haInsecureTls, method, path, body, out);
}

bool haTest(const char *url, const char *token, bool insecureTls, String &msg)
{
  String out;
  int code = request(url, token, insecureTls, "GET", "/api/", "", out);
  if (code == 200)
  {
    JsonDocument doc;
    deserializeJson(doc, out);
    msg = doc["message"] | "OK";
    // also report the HA version via /api/config
    String cfg;
    if (request(url, token, insecureTls, "GET", "/api/config", "", cfg) == 200)
    {
      JsonDocument c;
      JsonDocument filter;
      filter["version"] = true;
      filter["location_name"] = true;
      if (!deserializeJson(c, cfg, DeserializationOption::Filter(filter)))
        msg += String(" — ") + (c["location_name"] | "") + " (HA " + (c["version"] | "?") + ")";
    }
    return true;
  }
  if (code == 401)
    msg = "401 Unauthorized: check the long-lived access token";
  else if (code < 0)
    msg = "Connection failed: " + out;
  else
    msg = "HTTP " + String(code) + ": " + out.substring(0, 120);
  return false;
}

int haRenderTemplate(const Settings &s, const String &tpl, String &out)
{
  JsonDocument b;
  b["template"] = tpl;
  String body;
  serializeJson(b, body);
  return haRequest(s, "POST", "/api/template", body, out);
}

static void fail(const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(haLast.error, sizeof(haLast.error), fmt, ap);
  va_end(ap);
  haLast.ok = false;
  logf("HA: %s", haLast.error);
}

// weather.get_forecasts with ?return_response (HA 2024.8+)
static bool fetchForecast(const Settings &s, const char *entity, const char *type, JsonDocument &out)
{
  JsonDocument req;
  req["entity_id"] = entity;
  req["type"] = type;
  String body, resp;
  serializeJson(req, body);
  int code = haRequest(s, "POST", "/api/services/weather/get_forecasts?return_response", body, resp);
  if (code != 200)
  {
    logf("HA: %s forecast HTTP %d", type, code);
    return false;
  }
  JsonDocument filter;
  JsonObject f = filter["service_response"][entity]["forecast"][0].to<JsonObject>();
  for (const char *k : {"datetime", "condition", "temperature", "templow", "precipitation"})
    f[k] = true;
  return !deserializeJson(out, resp, DeserializationOption::Filter(filter));
}

bool haFetch(const Settings &s, DashboardData &d)
{
  uint32_t t0 = millis();
  haLast.error[0] = 0;
  std::string tpl = buildHaTemplate(s.map);
  String out;
  int code = haRenderTemplate(s, String(tpl.c_str()), out);
  haLast.httpCode = code;
  if (code != 200)
  {
    fail("template HTTP %d: %.60s", code, out.c_str());
    return false;
  }
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, out);
  if (err)
  {
    fail("template output is not JSON (%s): %.50s", err.c_str(), out.c_str());
    return false;
  }
  static DashboardData next; // static: too large for the loop task stack
  if (!parsePayload(doc.as<JsonVariantConst>(), next, s.dateFormat))
  {
    fail("template output is not an object");
    return false;
  }

  const char *weather = doc["weather_entity"] | "";
  if (weather[0])
  {
    char todayIso[11];
    strlcpy(todayIso, doc["date"] | "", sizeof(todayIso));
    JsonDocument fc;
    if (fetchForecast(s, weather, "daily", fc))
      parseHaForecast(fc["service_response"][weather]["forecast"].as<JsonArrayConst>(), next, todayIso);
    JsonDocument hr;
    if (fetchForecast(s, weather, "hourly", hr))
      parseHaHourly(hr["service_response"][weather]["forecast"].as<JsonArrayConst>(), next);
  }

  // HA is the clock: keeps deep-sleep slot alignment right without NTP
  int64_t epoch = doc["epoch"] | (int64_t)0;
  if (epoch > 1600000000)
  {
    struct timeval tv = {(time_t)epoch, 0};
    settimeofday(&tv, nullptr);
  }

  next.sys = d.sys;
  d = next;
  haLast.ok = true;
  haLast.ms = millis() - t0;
  logf("HA: fetched in %lu ms (%u rooms, %u people, %u forecast, %u hourly, %u events)", (unsigned long)haLast.ms,
       d.roomCount, d.familyCount, d.forecastCount, d.hourly.count, d.eventCount);
  return true;
}
