// UI strings. Every label has an English and a Latvian variant; the active
// language is selected at runtime from settings (device.language).
#ifndef LANG_H
#define LANG_H

#include "compat.h"

struct Translations
{
  const char *code;

  // Widget titles
  const char *title_outdoor;
  const char *title_indoor;
  const char *title_location;
  const char *title_network;
  const char *title_electricity;
  const char *title_forecast;
  const char *title_prices;
  const char *title_extras;
  const char *title_agenda;
  const char *title_system;
  const char *title_hourly;
  const char *title_house;
  const char *title_calendar;
  const char *label_lights;
  const char *label_open;
  const char *label_battery;
  const char *label_updates;
  const char *label_playing;
  const char *label_all_closed;
  const char *label_no_events;
  const char *title_home;
  const char *label_thermostat;
  const char *label_set;
  const char *label_people;
  const char *hvac_heating;
  const char *hvac_cooling;
  const char *hvac_idle;
  const char *hvac_off;
  const char *net_download;
  const char *net_upload;
  const char *net_latency;
  // Header annunciators (short, upper-cased on screen)
  const char *ann_home;
  const char *ann_rain;
  const char *ann_frost;
  const char *ann_wind;
  const char *ann_open;
  const char *ann_old_data;

  // Weather labels
  const char *label_temperature;
  const char *label_humidity;
  const char *label_pressure;
  const char *label_wind;
  const char *label_gusts;
  const char *label_clouds;

  // Legacy room keys (payload v1)
  const char *room_living;
  const char *room_bedroom;
  const char *room_kids;
  const char *room_bathroom;
  const char *room_office;
  const char *label_temp_short;
  const char *label_hum_short;

  // Network
  const char *label_download;
  const char *label_upload;
  const char *label_ping;

  // Electricity
  const char *label_price;
  const char *label_power;
  const char *label_voltage;
  const char *label_frequency;
  const char *label_power_factor;
  const char *label_consumption;
  const char *label_today;
  const char *label_month;
  const char *label_now;
  const char *label_min;
  const char *label_max;

  // Presence
  const char *state_home;
  const char *state_away;

  // Weather states (Home Assistant conditions)
  const char *weather_sunny;
  const char *weather_clear_night;
  const char *weather_cloudy;
  const char *weather_partlycloudy;
  const char *weather_rainy;
  const char *weather_pouring;
  const char *weather_snowy;
  const char *weather_snowy_rainy;
  const char *weather_fog;
  const char *weather_hail;
  const char *weather_windy;
  const char *weather_windy_variant;
  const char *weather_lightning;
  const char *weather_lightning_rainy;
  const char *weather_exceptional;

  // Header / misc
  const char *header_info;
  const char *header_updated;
  const char *label_sunrise;
  const char *label_sunset;
  const char *label_no_data;
  const char *label_stale;
  const char *label_uptime;
  const char *label_wakes;
  const char *label_heap;
  const char *label_signal;
  const char *label_all_home;
  const char *label_nobody_home;
  const char *label_alerts;
  const char *label_end_of_report;
  const char *const *weekdays;       // 7 short names, Sunday first
  const char *const *weekdays_long;  // 7 long names, Sunday first

  // Setup / status screens
  const char *setup_title;
  const char *setup_step1;
  const char *setup_step2;
  const char *setup_step3;
  const char *wifi_failed;
  const char *wifi_check;
  const char *maintenance_title;
  const char *maintenance_hint;
};

extern Translations lang;

void setLanguage(const char *code);   // "EN" (default) or "LV"
const char *translateWeatherState(const char *state);
const char *translateFamilyState(const char *state);
// Room/label names starting with '@' are translation keys (payload v1 rooms).
const char *translateName(const char *name);
// HA hvac_action / hvac mode -> short label ("heating" -> "Heating")
const char *translateHvac(const char *action, const char *mode);

#endif
