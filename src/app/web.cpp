#include "app/web.h"
#include "app/app.h"
#include "app/log.h"
#include "app/net.h"
#include "app/storage.h"
#include "app/ha_client.h"
#include "display/drawing.h"
#include "display/theme.h"
#include "ha_template.h"
#include "payload.h"
#include "lang.h"
#include "utils/utils.h"
#include "generated/web_assets.h"
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <esp_ota_ops.h>
#include <WiFiClient.h>

static volatile uint32_t lastAdminMs = 0;
static volatile bool otaRunning = false;
static const uint32_t ADMIN_KEEPALIVE_MS = 120000;

bool webBusy()
{
  return otaRunning || (lastAdminMs && millis() - lastAdminMs < ADMIN_KEEPALIVE_MS);
}

// ── auth ───────────────────────────────────────────────────────

// The admin panel is open until a password is set in it; afterwards HTTP
// Basic auth protects every admin page and API call.
static bool authed(AsyncWebServerRequest *r)
{
  lastAdminMs = millis();
  if (!settings.adminPass[0])
    return true;
  if (r->authenticate(settings.adminUser, settings.adminPass))
    return true;
  r->requestAuthentication(nullptr, false);
  return false;
}

static void sendJson(AsyncWebServerRequest *r, int code, JsonDocument &doc)
{
  AsyncResponseStream *res = r->beginResponseStream("application/json");
  res->setCode(code);
  res->addHeader("Cache-Control", "no-store");
  serializeJson(doc, *res);
  r->send(res);
}

static void sendOk(AsyncWebServerRequest *r, const char *msg = "ok")
{
  JsonDocument d;
  d["ok"] = true;
  d["message"] = msg;
  sendJson(r, 200, d);
}

static void sendErr(AsyncWebServerRequest *r, int code, const char *msg)
{
  JsonDocument d;
  d["ok"] = false;
  d["error"] = msg;
  sendJson(r, code, d);
}

// ── background jobs (slow work runs on the loop task) ──────────

enum JobType : uint8_t { JOB_NONE, JOB_HA_TEST, JOB_HA_ENTITIES, JOB_HA_RENDER, JOB_WIFI_SCAN, JOB_HA_FETCH };
struct Job
{
  volatile JobType type;
  volatile bool done;
  uint32_t id;
  int code;
  String arg1, arg2, arg3;
  String result;
};
static Job job = {JOB_NONE, true, 0, 0};

static uint32_t startJob(JobType t, const String &a1 = "", const String &a2 = "", const String &a3 = "")
{
  if (!job.done && job.type != JOB_NONE)
    return 0;
  job.id++;
  job.arg1 = a1;
  job.arg2 = a2;
  job.arg3 = a3;
  job.result = "";
  job.code = 0;
  job.done = false;
  job.type = t;
  return job.id;
}

void webLoop()
{
  if (job.done || job.type == JOB_NONE)
    return;
  switch (job.type)
  {
  case JOB_HA_TEST:
  {
    String msg;
    bool ok = haTest(job.arg1.c_str(), job.arg2.c_str(), job.arg3 == "1", msg);
    job.code = ok ? 200 : 502;
    JsonDocument d;
    d["ok"] = ok;
    d["message"] = msg;
    serializeJson(d, job.result);
    break;
  }
  case JOB_HA_ENTITIES:
  {
    std::string tpl = buildEntitySearchTemplate(job.arg1.c_str(), job.arg2.c_str(), 40);
    job.code = haRenderTemplate(settings, String(tpl.c_str()), job.result);
    break;
  }
  case JOB_HA_RENDER:
  {
    std::string tpl = buildHaTemplate(settings.map);
    job.code = haRenderTemplate(settings, String(tpl.c_str()), job.result);
    break;
  }
  case JOB_HA_FETCH:
  {
    bool ok = appPullNow("manual");
    JsonDocument d;
    d["ok"] = ok;
    if (ok)
    {
      char msg[160];
      appLockData();
      snprintf(msg, sizeof(msg), "Fetched in %lu ms: %u rooms, %u people, %u forecast days, %u hourly, %u events. Redrawing the panel.",
               (unsigned long)haLast.ms, app.data.roomCount, app.data.familyCount, app.data.forecastCount,
               app.data.hourly.count, app.data.eventCount);
      appUnlockData();
      d["message"] = msg;
    }
    else
      d["message"] = app.lastError;
    serializeJson(d, job.result);
    job.code = ok ? 200 : 502;
    break;
  }
  case JOB_WIFI_SCAN:
  {
    int n = WiFi.scanNetworks(false, false);
    JsonDocument d;
    JsonArray a = d.to<JsonArray>();
    for (int i = 0; i < n && i < 30; i++)
    {
      JsonObject o = a.add<JsonObject>();
      o["ssid"] = WiFi.SSID(i);
      o["rssi"] = WiFi.RSSI(i);
      o["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
    }
    WiFi.scanDelete();
    serializeJson(d, job.result);
    job.code = 200;
    break;
  }
  default:
    break;
  }
  job.done = true;
}

static void sendJobStarted(AsyncWebServerRequest *r, uint32_t id)
{
  if (!id)
  {
    sendErr(r, 409, "another job is running");
    return;
  }
  JsonDocument d;
  d["ok"] = true;
  d["job"] = id;
  sendJson(r, 202, d);
}

// ── status ─────────────────────────────────────────────────────

static void statusJson(JsonObject s)
{
  s["fw"] = FW_VERSION;
  s["build"] = __DATE__ " " __TIME__;
  s["chip"] = ESP.getChipModel();
  s["mac"] = WiFi.macAddress();
  s["heap"] = ESP.getFreeHeap();
  s["heap_min"] = ESP.getMinFreeHeap();
  s["uptime"] = millis() / 1000;
  s["wake_count"] = app.wakeCount;
  s["wake_reason"] = appWakeReason();
  s["mode"] = app.mode == MODE_SETUP_AP ? "setup" : "normal";
  s["ap"] = app.mode == MODE_SETUP_AP ? netApName() : "";
  s["power_mode"] = settings.powerMode == POWER_ALWAYS_ON ? "always_on" : "sleep";
  s["source"] = settings.sourceMode == SOURCE_PULL ? "pull" : "push";
  s["last_update_age"] = app.lastUpdateMs ? (long)((millis() - app.lastUpdateMs) / 1000) : -1;
  s["last_source"] = app.lastSource;
  s["last_error"] = app.lastError;
  s["fail_count"] = app.failCount;
  s["ha_ok"] = haLast.ok;
  s["ha_configured"] = appHaConfigured();
  s["ha_ms"] = haLast.ms;
  s["ssid"] = WiFi.SSID();
  s["rssi"] = WiFi.RSSI();
  s["ip"] = netIp();
  s["hostname"] = settings.hostname;
  s["channel"] = WiFi.channel();
  s["maintenance"] = appMaintenance();
  s["maintenance_left"] = appMaintenance() ? (long)((app.maintenanceUntilMs - millis()) / 1000) : 0;
  long adminLeft = (int32_t)(app.adminUntilMs - millis()) > 0 ? (long)((app.adminUntilMs - millis()) / 1000) : 0;
  s["admin_window_left"] = adminLeft;
  s["next_wake_s"] = appSecondsToNextWake();
  s["render_ms"] = app.lastRenderMs;
  s["spi_ms"] = app.lastTransferMs;
  s["panel_ms"] = app.lastRefreshMs;
  s["render_skipped"] = app.lastRenderSkipped;
  s["auth"] = settings.adminPass[0] != 0;
  s["time"] = (long)time(nullptr);
  s["fs_used"] = LittleFS.usedBytes();
  s["fs_total"] = LittleFS.totalBytes();
  s["chip_temp"] = temperatureRead();
  const esp_partition_t *p = esp_ota_get_running_partition();
  s["partition"] = p ? p->label : "?";
}

// ── static assets ──────────────────────────────────────────────

static const WebAsset *findAsset(const char *path)
{
  for (size_t i = 0; i < WEB_ASSET_COUNT; i++)
    if (!strcmp(WEB_ASSETS[i].path, path))
      return &WEB_ASSETS[i];
  return nullptr;
}

// Assets change with every firmware build: revalidate with a build ETag so
// browsers get a cheap 304 normally and never mix files after an OTA update.
static const char *ASSET_ETAG = "\"" FW_VERSION "-" __DATE__ "-" __TIME__ "\"";

static void sendAsset(AsyncWebServerRequest *r, const WebAsset *a)
{
  if (!a)
  {
    r->send(404, "text/plain", "asset missing");
    return;
  }
  if (r->hasHeader("If-None-Match") && r->header("If-None-Match") == ASSET_ETAG)
  {
    AsyncWebServerResponse *nm = r->beginResponse(304);
    nm->addHeader("ETag", ASSET_ETAG);
    r->send(nm);
    return;
  }
  AsyncWebServerResponse *res = r->beginResponse(200, a->mime, a->data, a->len);
  res->addHeader("Content-Encoding", "gzip");
  res->addHeader("Cache-Control", "no-cache");
  res->addHeader("ETag", ASSET_ETAG);
  r->send(res);
}

// ── preview (renders into the framebuffer, streams a BMP) ──────

static void handlePreview(AsyncWebServerRequest *r)
{
  if (!authed(r))
    return;
  if (!appLockFb(3000))
  {
    sendErr(r, 503, "display busy, try again");
    return;
  }
  static Settings tmp;
  tmp = settings;
  if (r->hasParam("theme"))
  {
    String t = r->getParam("theme")->value();
    if (isThemeId(t.c_str()))
      strlcpy(tmp.theme, t.c_str(), sizeof(tmp.theme));
  }
  if (r->hasParam("slots"))
  {
    String sl = r->getParam("slots")->value();
    int i = 0, from = 0;
    while (i < DASH_SLOTS && from <= (int)sl.length())
    {
      int comma = sl.indexOf(',', from);
      if (comma < 0)
        comma = sl.length();
      String id = sl.substring(from, comma);
      strlcpy(tmp.slots[i++], isWidgetId(id.c_str()) ? id.c_str() : "none", sizeof(tmp.slots[0]));
      from = comma + 1;
    }
  }
  static char previewLang[4];
  strlcpy(previewLang, r->hasParam("lang") ? r->getParam("lang")->value().c_str() : settings.language,
          sizeof(previewLang));
  static DashboardData snap;
  appLockData();
  appFillSystemStats(app.data);
  snap = app.data;
  appUnlockData();
  fbSetBand(-1000, FB_BAND_ROWS); // nothing rendered yet

  // BMP rows are stored bottom-up; render the 60-row band holding the row
  // being streamed on demand, so the preview needs no full framebuffer.
  AsyncWebServerResponse *res = r->beginResponse("image/bmp", bmpSize(), [](uint8_t *buf, size_t maxLen, size_t index) -> size_t {
    int row = bmpRowAt(index);
    if (row >= 0 && (row < fbY0 || row >= fbY0 + fbRows))
    {
      setLanguage(previewLang);
      fbSetBand(row / FB_BAND_ROWS * FB_BAND_ROWS, FB_BAND_ROWS);
      renderDashboard(snap, tmp);
      setLanguage(settings.language);
    }
    size_t n = maxLen;
    size_t hdr = bmpSize() - EPD_HEIGHT * (EPD_WIDTH / 2);
    if (row < 0 && n > hdr - index)
      n = hdr - index; // header only; pixel rows start with a fresh band
    if (row >= 0)
    {
      // stop at the band boundary so the next call renders the next band
      size_t rowsLeft = row - fbY0 + 1;
      size_t bytesLeft = rowsLeft * (EPD_WIDTH / 2) - ((index - (bmpSize() - EPD_HEIGHT * (EPD_WIDTH / 2))) % (EPD_WIDTH / 2));
      if (n > bytesLeft)
        n = bytesLeft;
    }
    return bmpRead(buf, n, index);
  });
  res->addHeader("Cache-Control", "no-store");
  r->onDisconnect([]() { appUnlockFb(); });
  r->send(res);
}

// ── push endpoint ──────────────────────────────────────────────

static bool pushAuthorized(AsyncWebServerRequest *r)
{
  if (!settings.pushToken[0])
    return true;
  if (r->hasHeader("Authorization"))
  {
    String h = r->header("Authorization");
    if (h == String("Bearer ") + settings.pushToken)
      return true;
  }
  if (r->hasParam("token") && r->getParam("token")->value() == settings.pushToken)
    return true;
  return false;
}

static void handlePush(AsyncWebServerRequest *r, JsonVariant &json)
{
  if (!pushAuthorized(r))
  {
    sendErr(r, 401, "push token required");
    return;
  }
  static DashboardData incoming;
  if (!parsePayload(json.as<JsonVariantConst>(), incoming, settings.dateFormat))
  {
    sendErr(r, 400, "payload must be a JSON object");
    return;
  }
  int64_t epoch = json["epoch"] | (int64_t)0;
  if (epoch <= 1600000000)
  {
    // Older payloads carry only local date + time: derive the clock from them
    int dd, mm, yy, hh, mi;
    const char *t = json["time"] | "";
    if (parseDate(json["date"] | "", dd, mm, yy) && sscanf(t, "%d:%d", &hh, &mi) == 2)
    {
      struct tm tmv = {};
      tmv.tm_year = yy - 1900;
      tmv.tm_mon = mm - 1;
      tmv.tm_mday = dd;
      tmv.tm_hour = hh;
      tmv.tm_min = mi;
      tmv.tm_isdst = -1;
      epoch = mktime(&tmv);
    }
  }
  if (epoch > 1600000000)
  {
    struct timeval tv = {(time_t)epoch, 0};
    settimeofday(&tv, nullptr);
  }
  logf("Push: %s %s, %u rooms, %u people, %u forecast", incoming.date, incoming.time, incoming.roomCount,
       incoming.familyCount, incoming.forecastCount);
  appSetData(incoming, "push");
  app.updateReceived = true;
  r->send(200, "text/plain", "OK");
}

// ── config ─────────────────────────────────────────────────────

static void handleConfigPost(AsyncWebServerRequest *r, JsonVariant &json)
{
  if (!authed(r))
    return;
  static Settings tmp;
  tmp = settings;
  if (!settingsFromJson(tmp, json.as<JsonVariantConst>()))
  {
    sendErr(r, 400, "expected a JSON object");
    return;
  }
  bool reboot = strcmp(tmp.ssid, settings.ssid) || strcmp(tmp.wifiPass, settings.wifiPass) ||
                strcmp(tmp.hostname, settings.hostname);
  if (!saveSettings(tmp))
  {
    sendErr(r, 500, "could not write config");
    return;
  }
  settings = tmp;
  setLanguage(settings.language);
  app.renderRequested = true;
  JsonDocument d;
  d["ok"] = true;
  d["reboot_required"] = reboot;
  sendJson(r, 200, d);
}

static void handleAction(AsyncWebServerRequest *r, JsonVariant &json)
{
  if (!authed(r))
    return;
  const char *a = json["action"] | "";
  if (!strcmp(a, "render"))
  {
    app.forceRefresh = true;
    app.renderRequested = true;
  }
  else if (!strcmp(a, "fetch"))
  {
    // Pull from HA whenever it is configured, in push mode too; otherwise redraw.
    if (appHaConfigured())
    {
      app.manualPull = true;
      app.fetchRequested = true;
    }
    else
    {
      app.forceRefresh = true;
      app.renderRequested = true;
    }
  }
  else if (!strcmp(a, "reboot"))
    app.rebootRequested = true;
  else if (!strcmp(a, "sleep"))
  {
    lastAdminMs = 0;
    app.adminUntilMs = millis();
    appSetMaintenance(false);
    app.sleepRequested = true;
  }
  else if (!strcmp(a, "maintenance_on"))
    appSetMaintenance(true);
  else if (!strcmp(a, "maintenance_off"))
    appSetMaintenance(false);
  else if (!strcmp(a, "factory_reset"))
  {
    factoryReset();
    app.rebootRequested = true;
  }
  else
  {
    sendErr(r, 400, "unknown action");
    return;
  }
  sendOk(r, a);
}

// ── routes ─────────────────────────────────────────────────────

void webBegin(AsyncWebServer &server)
{
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *r) { sendAsset(r, findAsset("/index.html")); });
  server.on("/admin", HTTP_GET, [](AsyncWebServerRequest *r) {
    if (authed(r))
      sendAsset(r, findAsset("/admin.html"));
  });

  // live data (public, read-only — same information the panel shows)
  server.on("/api/state", HTTP_GET, [](AsyncWebServerRequest *r) {
    JsonDocument d;
    appLockData();
    appFillSystemStats(app.data);
    dataToJson(app.data, d["data"].to<JsonObject>());
    appUnlockData();
    JsonObject st = d["status"].to<JsonObject>();
    st["fw"] = FW_VERSION;
    st["name"] = settings.name;
    st["theme"] = settings.theme;
    st["language"] = settings.language;
    st["price_unit"] = settings.priceUnit;
    st["wind_unit"] = settings.windUnit;
    st["last_update_age"] = app.lastUpdateMs ? (long)((millis() - app.lastUpdateMs) / 1000) : -1;
    st["uptime"] = millis() / 1000;
    st["rssi"] = WiFi.RSSI();
    st["wake_count"] = app.wakeCount;
    JsonArray slots = st["slots"].to<JsonArray>();
    for (int i = 0; i < DASH_SLOTS; i++)
      slots.add(settings.slots[i]);
    JsonObject th = st["thresholds"].to<JsonObject>();
    JsonDocument tmp;
    settingsToJson(settings, tmp.to<JsonObject>(), true);
    th.set(tmp["thresholds"].as<JsonObjectConst>());
    sendJson(r, 200, d);
  });

  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *r) {
    if (!authed(r))
      return;
    JsonDocument d;
    statusJson(d.to<JsonObject>());
    sendJson(r, 200, d);
  });

  server.on("/api/meta", HTTP_GET, [](AsyncWebServerRequest *r) {
    if (!authed(r))
      return;
    JsonDocument d;
    JsonArray w = d["widgets"].to<JsonArray>();
    for (int i = 0; i < WIDGET_COUNT; i++)
      w.add(WIDGET_IDS[i]);
    JsonArray t = d["themes"].to<JsonArray>();
    for (int i = 0; i < THEME_COUNT; i++)
    {
      JsonObject o = t.add<JsonObject>();
      o["id"] = THEME_IDS[i];
      o["name"] = THEME_NAMES[i];
    }
    sendJson(r, 200, d);
  });

  server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest *r) {
    if (!authed(r))
      return;
    JsonDocument d;
    settingsToJson(settings, d.to<JsonObject>(), !r->hasParam("secrets"));
    if (r->hasParam("download"))
    {
      AsyncResponseStream *res = r->beginResponseStream("application/json");
      res->addHeader("Content-Disposition", "attachment; filename=m17-config.json");
      serializeJsonPretty(d, *res);
      r->send(res);
      return;
    }
    sendJson(r, 200, d);
  });

  auto *cfg = new AsyncCallbackJsonWebHandler("/api/config", handleConfigPost);
  cfg->setMethod(HTTP_POST);
  cfg->setMaxContentLength(16384);
  server.addHandler(cfg);

  auto *act = new AsyncCallbackJsonWebHandler("/api/action", handleAction);
  act->setMethod(HTTP_POST);
  server.addHandler(act);

  for (const char *path : {"/update", "/api/push"})
  {
    auto *push = new AsyncCallbackJsonWebHandler(path, handlePush);
    push->setMethod(HTTP_POST);
    push->setMaxContentLength(32768);
    server.addHandler(push);
  }

  server.on("/api/preview.bmp", HTTP_GET, handlePreview);

  server.on("/api/logs", HTTP_GET, [](AsyncWebServerRequest *r) {
    if (!authed(r))
      return;
    String s;
    logDump(s);
    r->send(200, "text/plain; charset=utf-8", s);
  });

  // ---- jobs ----
  server.on("/api/job", HTTP_GET, [](AsyncWebServerRequest *r) {
    if (!authed(r))
      return;
    uint32_t id = r->hasParam("id") ? r->getParam("id")->value().toInt() : 0;
    if (id != job.id)
    {
      sendErr(r, 404, "unknown job");
      return;
    }
    if (!job.done)
    {
      JsonDocument d;
      d["done"] = false;
      sendJson(r, 200, d);
      return;
    }
    // result is JSON from the job; wrap without re-parsing large bodies
    String out = "{\"done\":true,\"code\":" + String(job.code) + ",\"body\":";
    bool isJson = job.result.length() && (job.result[0] == '{' || job.result[0] == '[');
    if (isJson)
      out += job.result;
    else
    {
      JsonDocument s;
      s.set(job.result);
      String esc;
      serializeJson(s, esc);
      out += esc;
    }
    out += "}";
    r->send(200, "application/json", out);
  });

  server.on("/api/wifi/scan", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (authed(r))
      sendJobStarted(r, startJob(JOB_WIFI_SCAN));
  });

  auto *haTestH = new AsyncCallbackJsonWebHandler("/api/ha/test", [](AsyncWebServerRequest *r, JsonVariant &j) {
    if (!authed(r))
      return;
    String url = j["ha_url"] | settings.haUrl;
    String tok = j["ha_token"] | "";
    if (tok.isEmpty() || tok == SECRET_MASK)
      tok = settings.haToken;
    while (url.endsWith("/"))
      url.remove(url.length() - 1);
    bool insecure = j["insecure_tls"] | settings.haInsecureTls;
    sendJobStarted(r, startJob(JOB_HA_TEST, url, tok, insecure ? "1" : "0"));
  });
  haTestH->setMethod(HTTP_POST);
  server.addHandler(haTestH);

  server.on("/api/ha/entities", HTTP_GET, [](AsyncWebServerRequest *r) {
    if (!authed(r))
      return;
    String q = r->hasParam("q") ? r->getParam("q")->value() : "";
    String dom = r->hasParam("domain") ? r->getParam("domain")->value() : "";
    sendJobStarted(r, startJob(JOB_HA_ENTITIES, q, dom));
  });

  // Manual pull from Home Assistant (any source mode); result via /api/job
  server.on("/api/ha/fetch", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (!authed(r))
      return;
    if (!appHaConfigured())
    {
      sendErr(r, 400, "Set the Home Assistant URL and token first (Data source tab)");
      return;
    }
    sendJobStarted(r, startJob(JOB_HA_FETCH));
  });

  server.on("/api/ha/render", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (authed(r))
      sendJobStarted(r, startJob(JOB_HA_RENDER));
  });

  server.on("/api/ha/template", HTTP_GET, [](AsyncWebServerRequest *r) {
    if (!authed(r))
      return;
    std::string tpl = buildHaTemplate(settings.map);
    r->send(200, "text/plain; charset=utf-8", tpl.c_str());
  });

  // ---- OTA firmware update ----
  server.on(
      "/api/ota", HTTP_POST,
      [](AsyncWebServerRequest *r) {
        if (!authed(r))
          return;
        bool ok = !Update.hasError();
        JsonDocument d;
        d["ok"] = ok;
        d["message"] = ok ? "Update installed, rebooting" : Update.errorString();
        sendJson(r, ok ? 200 : 500, d);
        otaRunning = false;
        if (ok)
          app.rebootRequested = true;
      },
      [](AsyncWebServerRequest *r, const String &filename, size_t index, uint8_t *data, size_t len, bool final) {
        if (index == 0)
        {
          if (settings.adminPass[0] && !r->authenticate(settings.adminUser, settings.adminPass))
            return;
          otaRunning = true;
          logf("OTA: receiving %s", filename.c_str());
          if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH))
            logf("OTA: begin failed: %s", Update.errorString());
        }
        if (Update.isRunning() && len && Update.write(data, len) != len)
          logf("OTA: write failed: %s", Update.errorString());
        if (final && Update.isRunning())
        {
          if (Update.end(true))
            logf("OTA: %u bytes OK", (unsigned)(index + len));
          else
            logf("OTA: end failed: %s", Update.errorString());
        }
      });

  server.onNotFound([](AsyncWebServerRequest *r) {
    const WebAsset *a = findAsset(r->url().c_str());
    if (a)
    {
      sendAsset(r, a);
      return;
    }
    if (app.mode == MODE_SETUP_AP)
    {
      // captive portal: send every unknown host/path to the admin panel
      r->redirect(String("http://") + netIp() + "/admin");
      return;
    }
    r->send(404, "text/plain", "Not found");
  });

  DefaultHeaders::Instance().addHeader("X-Content-Type-Options", "nosniff");
  logf("HTTP: heap %u, largest block %u", (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
  server.begin();
  logf("HTTP: server started (state %d)", (int)server.state());
}

