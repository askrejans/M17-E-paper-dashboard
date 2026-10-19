// Home Assistant REST client (pull mode, admin helpers).
#pragma once

#include <Arduino.h>
#include "data.h"
#include "settings.h"

struct HaStatus
{
  bool ok;
  int httpCode;
  uint32_t ms;
  char error[96];
};

extern HaStatus haLast;

// Render the mapping template + fetch daily/hourly forecasts into `d`.
// On success also sets the system clock from HA's time.
bool haFetch(const Settings &s, DashboardData &d);

// Low-level request. `out` receives the body. Returns HTTP status (<0 = transport error).
int haRequest(const Settings &s, const char *method, const char *path, const String &body, String &out);

// Check URL + token: GET /api/ -> {"message": "API running."}; fills `msg`.
bool haTest(const char *url, const char *token, bool insecureTls, String &msg);

// Render the Jinja template on HA and return the raw output (debug helper).
int haRenderTemplate(const Settings &s, const String &tpl, String &out);
