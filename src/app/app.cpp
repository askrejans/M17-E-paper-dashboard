#include "app/app.h"
#include "app/log.h"
#include "app/net.h"
#include "app/storage.h"
#include "app/ha_client.h"
#include "app/web.h"
#include "display/epd_driver.h"
#include "display/epd_spi.h"
#include "display/drawing.h"
#include "display/theme.h"
#include "font_8x16.h"
#include "lang.h"
#include <WiFi.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>
#include <esp_heap_caps.h>
#include <esp_rom_crc.h>
#include <freertos/semphr.h>
#include "utils/utils.h"

AppState app;
unsigned char *fb = nullptr;
static AsyncWebServer server(80);
static SemaphoreHandle_t fbMutex, dataMutex;

// Survives deep sleep (not power-on reset)
RTC_DATA_ATTR static uint32_t rtcWakeCount = 0;
RTC_DATA_ATTR static uint32_t rtcFailCount = 0;
RTC_DATA_ATTR static uint32_t rtcFrameCrc = 0;
RTC_DATA_ATTR static uint32_t rtcPullFails = 0;
RTC_DATA_ATTR static bool rtcDataValid = false;
RTC_DATA_ATTR static DashboardData rtcData;
static_assert(sizeof(DashboardData) < 6000, "DashboardData must fit RTC slow memory");

bool appLockFb(uint32_t ms) { return xSemaphoreTake(fbMutex, pdMS_TO_TICKS(ms)) == pdTRUE; }
void appUnlockFb() { xSemaphoreGive(fbMutex); }
void appLockData() { xSemaphoreTake(dataMutex, portMAX_DELAY); }
void appUnlockData() { xSemaphoreGive(dataMutex); }

const char *appWakeReason()
{
  switch (esp_sleep_get_wakeup_cause())
  {
  case ESP_SLEEP_WAKEUP_TIMER: return "timer";
  case ESP_SLEEP_WAKEUP_EXT0: return "button";
  case ESP_SLEEP_WAKEUP_UNDEFINED: return "power-on/reset";
  default: return "other";
  }
}

void appFillSystemStats(DashboardData &d)
{
  d.sys.rssi = WiFi.isConnected() ? WiFi.RSSI() : 0;
  d.sys.channel = WiFi.channel();
  strlcpy(d.sys.ip, netIp().c_str(), sizeof(d.sys.ip));
  d.sys.heapFree = ESP.getFreeHeap();
  d.sys.uptime = millis() / 1000;
  d.sys.wakeCount = app.wakeCount;
  d.sys.chipTemp = temperatureRead();
}

void appSetData(const DashboardData &d, const char *source)
{
  appLockData();
  SystemStats keep = app.data.sys;
  app.data = d;
  app.data.sys = keep;
  app.data.stale = false;
  appUnlockData();
  app.lastUpdateMs = millis();
  strlcpy(app.lastSource, source, sizeof(app.lastSource));
  app.renderRequested = true;
}

bool appMaintenance()
{
  return app.maintenanceUntilMs && (int32_t)(app.maintenanceUntilMs - millis()) > 0;
}

void appSetMaintenance(bool on)
{
  app.maintenanceUntilMs = on ? millis() + MAINTENANCE_TIMEOUT_MIN * 60000UL : 0;
  if (on && !app.maintenanceUntilMs)
    app.maintenanceUntilMs = 1;
  logf("Maintenance %s", on ? "ON" : "OFF");
}

// ── Rendering ──────────────────────────────────────────────────

static uint32_t frameHash(const DashboardData &d)
{
  // Hash data (minus volatile system stats) + display-relevant settings, so
  // an identical picture never costs a 20 s panel refresh.
  SystemStats zero = {};
  uint32_t h = esp_rom_crc32_le(0, (const uint8_t *)&d, offsetof(DashboardData, sys));
  h = esp_rom_crc32_le(h, (const uint8_t *)&zero, sizeof(zero));
  h = esp_rom_crc32_le(h, (const uint8_t *)&d.stale, sizeof(DashboardData) - offsetof(DashboardData, stale));
  h = esp_rom_crc32_le(h, (const uint8_t *)settings.theme, sizeof(settings.theme));
  h = esp_rom_crc32_le(h, (const uint8_t *)settings.slots, sizeof(settings.slots));
  h = esp_rom_crc32_le(h, (const uint8_t *)&settings.th, sizeof(settings.th));
  h = esp_rom_crc32_le(h, (const uint8_t *)settings.language, sizeof(settings.language));
  h = esp_rom_crc32_le(h, (const uint8_t *)settings.name, sizeof(settings.name));
  h ^= settings.systemBar ? 0x5A5A5A5A : 0;
  return h;
}

// Render `draw` band by band and stream every band to the panel.
static void displayBanded(bool fast, void (*draw)(void *), void *ctx)
{
  if (fast)
    EPD_init_Fast();
  else
    EPD_init();
  EPD_beginFrame();
  for (int y0 = 0; y0 < EPD_HEIGHT; y0 += FB_BAND_ROWS)
  {
    int rows = EPD_HEIGHT - y0 < FB_BAND_ROWS ? EPD_HEIGHT - y0 : FB_BAND_ROWS;
    fbSetBand(y0, rows);
    draw(ctx);
    EPD_writeRows(fb, rows);
  }
  EPD_endFrame();
  app.lastTransferMs = epdLastTransferMs;
}

struct DashCtx
{
  const DashboardData *d;
  const Settings *s;
};

static void drawDash(void *p)
{
  DashCtx *c = (DashCtx *)p;
  renderDashboard(*c->d, *c->s);
}

static void finishRefresh()
{
  uint32_t t0 = millis();
  EPD_refresh();
  EPD_sleep();
  app.lastRefreshMs = millis() - t0;
}

static void renderNow(bool force)
{
  static DashboardData snap;
  appLockData();
  appFillSystemStats(app.data);
  snap = app.data;
  appUnlockData();

  uint32_t hash = frameHash(snap);
  if (!force && settings.skipUnchanged && hash == rtcFrameCrc)
  {
    logf("Render: frame unchanged, panel refresh skipped");
    app.lastRenderSkipped = true;
    app.renderedThisWake = true;
    return;
  }
  if (!appLockFb(30000))
  {
    logf("Render: framebuffer busy");
    return;
  }
  uint32_t t0 = millis();
  setLanguage(settings.language);
  DashCtx ctx = {&snap, &settings};
  displayBanded(settings.fastRefresh, drawDash, &ctx);
  app.lastRenderMs = millis() - t0 - app.lastTransferMs; // CPU time for all bands
  appUnlockFb(); // panel has the frame; previews may use the buffer again
  finishRefresh();
  rtcFrameCrc = hash;
  app.lastRenderSkipped = false;
  app.renderedThisWake = true;
  logf("Render: %s theme, render %lu ms, SPI %lu ms, panel %lu ms (%s)", settings.theme,
       (unsigned long)app.lastRenderMs, (unsigned long)app.lastTransferMs, (unsigned long)app.lastRefreshMs,
       settings.fastRefresh ? "fast" : "quality");
}

static void showScreen(void (*draw)())
{
  if (!appLockFb(30000))
    return;
  displayBanded(false, [](void *f) { ((void (*)())f)(); }, (void *)draw);
  appUnlockFb();
  finishRefresh();
  rtcFrameCrc = 0;
}

// ── Pulling from Home Assistant ────────────────────────────────

bool appHaConfigured() { return settings.haUrl[0] && settings.haToken[0]; }

bool appPullNow(const char *source)
{
  if (!appHaConfigured())
  {
    strlcpy(app.lastError, "Home Assistant URL / token not set (Data source tab)", sizeof(app.lastError));
    return false;
  }
  logf("Pull: %s", source);
  static DashboardData fetched;
  appLockData();
  fetched = app.data;
  appUnlockData();
  if (haFetch(settings, fetched))
  {
    rtcPullFails = 0;
    app.lastError[0] = 0;
    appSetData(fetched, source);
    if (strcmp(source, "pull") != 0)
    {
      app.forceRefresh = true;   // a manual pull always redraws
      app.updateReceived = true; // push mode: counts as this cycle's update
    }
    return true;
  }
  strlcpy(app.lastError, haLast.error, sizeof(app.lastError));
  if (settings.sourceMode != SOURCE_PULL || strcmp(source, "pull") != 0)
    return false; // manual pulls do not count towards the stale logic
  rtcPullFails++;
  app.failCount = rtcPullFails;
  // After 3 failed scheduled pulls show the last data marked stale (once).
  if (rtcPullFails == 3 && app.data.valid)
  {
    appLockData();
    app.data.stale = true;
    appUnlockData();
    app.renderRequested = true;
  }
  return false;
}

// ── BOOT button (GPIO0) ────────────────────────────────────────
// A press pulls from Home Assistant (when configured) or redraws the panel.
// In deep-sleep mode the button also wakes the device (ext0).

static const gpio_num_t BUTTON_PIN = GPIO_NUM_0;

static void buttonLoop()
{
  static bool last = true;
  static uint32_t changedMs = 0;
  bool level = digitalRead(BUTTON_PIN);
  if (level != last && millis() - changedMs > 40)
  {
    changedMs = millis();
    last = level;
    if (!level) // pressed (active low)
    {
      logf("Button: manual update");
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
  }
}

// ── Sleep scheduling ───────────────────────────────────────────

uint32_t appSecondsToNextWake()
{
  uint32_t interval = settings.intervalMin * 60;
  time_t now = time(nullptr);
  bool clock = now > 1600000000;
  uint32_t sec;
  if (!clock)
    sec = settings.sourceMode == SOURCE_PULL
              ? interval
              : (app.updateReceived ? interval - settings.wakeBeforeSec : interval - settings.listenWindowSec);
  else if (settings.sourceMode == SOURCE_PULL)
    sec = secondsToSlot(now, interval, -2, 30); // just after the slot
  else
    sec = secondsToSlot(now, interval, settings.wakeBeforeSec, 30);

  // Quiet hours: sleep through the window, wake at its end
  if (clock && settings.quietStart >= 0)
  {
    time_t wake = now + sec;
    struct tm lt;
    localtime_r(&wake, &lt);
    uint32_t q = secondsUntilQuietEnd(settings.quietStart, settings.quietEnd, lt.tm_hour, lt.tm_min, lt.tm_sec);
    if (q)
      sec += q;
  }
  if (sec < 30)
    sec = 30;
  return sec;
}

static void deepSleep(uint32_t sec)
{
  logf("Sleep: %lu s (wake #%lu done)", (unsigned long)sec, (unsigned long)app.wakeCount);
  appLockData();
  rtcData = app.data;
  rtcDataValid = app.data.valid;
  appUnlockData();
  delay(50);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  esp_sleep_enable_timer_wakeup((uint64_t)sec * US_PER_SEC);
  // BOOT button wakes the device for a manual update
  rtc_gpio_pullup_en(BUTTON_PIN);
  esp_sleep_enable_ext0_wakeup(BUTTON_PIN, 0);
  esp_deep_sleep_start();
}

static bool canSleep()
{
  if (settings.powerMode == POWER_ALWAYS_ON || app.mode == MODE_SETUP_AP || appMaintenance())
    return false;
  if ((int32_t)(app.adminUntilMs - millis()) > 0)
    return false;
  if (webBusy())
    return false;
  if (settings.sourceMode == SOURCE_PULL)
    return app.renderedThisWake || app.failCount > 0;
  return (app.updateReceived && app.renderedThisWake) || (int32_t)(millis() - app.listenUntilMs) > 0;
}

// ── Screens ────────────────────────────────────────────────────

static void drawSetup()
{
  char url[40];
  snprintf(url, sizeof(url), "http://%s", netIp().c_str());
  renderSetupScreen(netApName(), url);
}

static void drawWifiError()
{
  renderMessageScreen(lang.wifi_failed, lang.wifi_check, settings.ssid, "");
}

// ── Setup / loop ───────────────────────────────────────────────

// 60-row band buffer (12 KB) instead of a 96 KB full framebuffer
static unsigned char bandBuffer[EPD_WIDTH * FB_BAND_ROWS / 4];

static void allocFramebuffer()
{
  fb = bandBuffer;
  fbSetBand(0, FB_BAND_ROWS);
}

void appSetup()
{
  logBegin();
  delay(50);
  logTitle(FW_NAME " " FW_VERSION);
  fbMutex = xSemaphoreCreateMutex();
  dataMutex = xSemaphoreCreateMutex();
  memset(&app, 0, sizeof(app));
  dataReset(app.data);
  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  bool timerWake = cause == ESP_SLEEP_WAKEUP_TIMER || cause == ESP_SLEEP_WAKEUP_EXT0;
  app.wakeCount = ++rtcWakeCount;
  app.failCount = rtcFailCount;
  logf("Boot: %s, wake #%lu, chip %s rev %d, heap %u", appWakeReason(), (unsigned long)app.wakeCount,
       ESP.getChipModel(), ESP.getChipRevision(), (unsigned)ESP.getFreeHeap());

  allocFramebuffer();
  font8x16_init();
  EPD_spiBegin();

  storageBegin();
  loadSettings(settings);
  setLanguage(settings.language);
  setenv("TZ", settings.timezone, 1);
  tzset();

  if (rtcDataValid)
  {
    app.data = rtcData;
    strlcpy(app.lastSource, "rtc", sizeof(app.lastSource));
  }
  // Reset button / power-on: stay reachable for a while for the admin panel
  if (!timerWake)
    app.adminUntilMs = millis() + ADMIN_WINDOW_SEC * 1000UL;

  bool connected = settings.ssid[0] && netConnect(settings, WIFI_CONNECT_TIMEOUT_MS);
  if (!connected)
  {
    rtcFailCount++;
    app.failCount = rtcFailCount;
    if (!settings.ssid[0] || rtcFailCount >= WIFI_FAILS_BEFORE_AP || !timerWake)
    {
      netStartSetupAp(settings);
      app.mode = MODE_SETUP_AP;
      app.adminUntilMs = millis() + 10 * 60 * 1000UL;
      webBegin(server);
      showScreen(drawSetup);
      return;
    }
    if (rtcFailCount == 1)
      showScreen(drawWifiError);
    deepSleep(60 * rtcFailCount);
    return;
  }
  rtcFailCount = 0;
  app.failCount = 0;
  netStartMdns(settings);
  webBegin(server);

  app.listenUntilMs = millis() + settings.listenWindowSec * 1000UL;
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  bool buttonWake = esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0;
  if (buttonWake && appHaConfigured())
    app.manualPull = true;
  if (settings.sourceMode == SOURCE_PULL || app.manualPull)
    app.fetchRequested = true;
  else if (buttonWake)
  {
    app.forceRefresh = true;
    app.renderRequested = true;
  }
}

void appLoop()
{
  netLoop();
  webLoop();
  buttonLoop();

  if (app.fetchRequested)
  {
    app.fetchRequested = false;
    appPullNow(app.manualPull ? "manual" : "pull");
    app.manualPull = false;
  }


  if (app.renderRequested)
  {
    bool force = app.forceRefresh;
    app.renderRequested = false;
    app.forceRefresh = false;
    renderNow(force);
  }

  // Always-on + pull: refresh on the interval
  static uint32_t lastPull = 0;
  if (settings.powerMode == POWER_ALWAYS_ON && settings.sourceMode == SOURCE_PULL && app.mode == MODE_NORMAL &&
      millis() - lastPull > settings.intervalMin * 60000UL)
  {
    lastPull = millis();
    app.fetchRequested = true;
  }

  if (app.rebootRequested)
  {
    delay(300);
    ESP.restart();
  }

  if (app.sleepRequested || canSleep())
  {
    uint32_t sec = appSecondsToNextWake();
    if (settings.sourceMode == SOURCE_PULL && rtcPullFails > 0 && 120 * rtcPullFails < sec)
      sec = 120 * rtcPullFails; // retry sooner after failures (2, 4, 6 ... min)
    deepSleep(sec);
  }

  // Setup AP that nobody used: sleep and retry Wi-Fi later
  if (app.mode == MODE_SETUP_AP && settings.ssid[0] && (int32_t)(millis() - app.adminUntilMs) > 0 &&
      !appMaintenance() && !webBusy())
    deepSleep(settings.intervalMin * 60);

  delay(5);
}
