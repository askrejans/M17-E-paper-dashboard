#ifndef CONFIG_H
#define CONFIG_H

#define UPDATE_INTERVAL_MIN 15
#define WAKE_BEFORE_SEC 90    // wake 90s early
#define LISTEN_WINDOW_SEC 180 // stay awake max 3 min

#define US_PER_SEC 1000000ULL
#define US_PER_MIN (60ULL * US_PER_SEC)

#define EPD_WIDTH 800
#define EPD_HEIGHT 480

// Data limits
#define MAX_FAMILY    4
#define MAX_FORECAST  5

// Alert thresholds - Outdoor
#define ALERT_OUTDOOR_TEMP_LOW   (-10.0f)
#define ALERT_OUTDOOR_TEMP_HIGH    30.0f
#define ALERT_WIND_HIGH            20.0f
#define ALERT_GUST_HIGH            25.0f

// Alert thresholds - Indoor
#define ALERT_INDOOR_TEMP_LOW      19.0f
#define ALERT_INDOOR_TEMP_HIGH     25.0f
#define ALERT_INDOOR_HUM_LOW       35.0f
#define ALERT_INDOOR_HUM_HIGH      60.0f

// Alert thresholds - Network
#define ALERT_DOWN_LOW             10.0f
#define ALERT_UP_LOW                5.0f
#define ALERT_PING_HIGH            50.0f
#define ALERT_RSSI_LOW            -75

// Alert thresholds - Power
#define ALERT_PRICE_HIGH           20.0f
#define ALERT_POWER_HIGH         5000.0f
#define ALERT_VOLTAGE_LOW         220.0f
#define ALERT_VOLTAGE_HIGH        240.0f
#define ALERT_FREQ_LOW             48.0f
#define ALERT_FREQ_HIGH            52.0f
#define ALERT_PF_LOW               70.0f
#define ALERT_MONTHLY_HIGH        320.0f

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