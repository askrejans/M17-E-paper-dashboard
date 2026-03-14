#ifndef CONFIG_H
#define CONFIG_H

#define UPDATE_INTERVAL_MIN 15
#define WAKE_BEFORE_SEC 90    // wake 90s early
#define LISTEN_WINDOW_SEC 180 // stay awake max 3 min

#define US_PER_SEC 1000000ULL
#define US_PER_MIN (60ULL * US_PER_SEC)

#define EPD_WIDTH 800
#define EPD_HEIGHT 480

#if defined(__has_include)
#  if __has_include("secrets.h")
#    include "secrets.h"
#  endif
#endif

/* Ensure macros exist to avoid compile errors in environments without secrets.h */
#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif
#ifndef DATE_FORMAT
#define DATE_FORMAT "DD.MM.YYYY"
#endif

#endif