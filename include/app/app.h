// Application state machine: wake -> connect -> (pull | listen for push) ->
// render -> deep sleep, with admin/maintenance/setup modes that keep the
// device awake. Web handlers only set flags; the loop task does the work.
#pragma once

#include <Arduino.h>
#include "data.h"
#include "settings.h"

enum AppMode : uint8_t
{
  MODE_NORMAL,
  MODE_SETUP_AP,     // no/invalid Wi-Fi: access point + captive admin
};

struct AppState
{
  AppMode mode;
  DashboardData data;          // latest data (guarded by appLockData)
  volatile bool renderRequested;
  volatile bool forceRefresh;  // ignore "unchanged frame" skip
  volatile bool fetchRequested;
  volatile bool manualPull;    // fetch was requested by a person (button/admin)
  volatile bool updateReceived;
  volatile bool rebootRequested;
  volatile bool sleepRequested;
  uint32_t lastUpdateMs;       // millis() of last data update (0 = none this boot)
  char lastSource[8];          // "push" | "pull" | "rtc"
  uint32_t adminUntilMs;       // awake window for admin after reset/power-on
  uint32_t maintenanceUntilMs; // 0 = off
  uint32_t listenUntilMs;      // push mode deadline
  bool renderedThisWake;
  bool lastRenderSkipped;
  uint32_t lastRenderMs;
  uint32_t lastTransferMs;
  uint32_t lastRefreshMs;
  uint32_t wakeCount;
  uint32_t failCount;
  char lastError[96];
};

extern AppState app;
extern unsigned char *fb;

void appSetup();
void appLoop();

// Locks: framebuffer (render/transfer/preview) and data (push handler/render)
bool appLockFb(uint32_t timeoutMs);
void appUnlockFb();
void appLockData();
void appUnlockData();

// Replace current data (from push / pull) and schedule a render.
void appSetData(const DashboardData &d, const char *source);
void appFillSystemStats(DashboardData &d);
bool appMaintenance();
// Fetch from Home Assistant now (works in push and pull mode when HA URL and
// token are set). `source` is recorded as the data origin ("pull", "manual").
bool appPullNow(const char *source);
bool appHaConfigured();
void appSetMaintenance(bool on);
uint32_t appSecondsToNextWake();
const char *appWakeReason();
