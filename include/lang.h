#ifndef LANG_H
#define LANG_H

#include <Arduino.h>

// Translation strings structure
struct Translations
{
  // Box titles
  const char* title_outdoor;
  const char* title_indoor;
  const char* title_location;
  const char* title_network;
  const char* title_electricity;
  const char* title_icon_test;
  const char* title_forecast;
  
  // Weather labels
  const char* label_temperature;
  const char* label_humidity;
  const char* label_pressure;
  const char* label_wind;
  const char* label_gusts;
  const char* label_clouds;
  
  // Indoor room names
  const char* room_living;
  const char* room_bedroom;
  const char* room_kids;
  const char* room_bathroom;
  const char* room_office;
  const char* label_temp_hum_header;
  
  // Network labels
  const char* label_download;
  const char* label_upload;
  const char* label_ping;
  
  // Electricity labels
  const char* label_price;
  const char* label_power;
  const char* label_voltage;
  const char* label_frequency;
  const char* label_power_factor;
  const char* label_consumption;
  
  // Family states
  const char* state_home;
  const char* state_away;
  
  // Weather states
  const char* weather_sunny;
  const char* weather_clear_night;
  const char* weather_cloudy;
  const char* weather_partlycloudy;
  const char* weather_rainy;
  const char* weather_pouring;
  const char* weather_snowy;
  const char* weather_snowy_rainy;
  const char* weather_fog;
  const char* weather_hail;
  const char* weather_windy;
  const char* weather_windy_variant;
  const char* weather_lightning;
  const char* weather_lightning_rainy;
  const char* weather_exceptional;
  
  // Header
  const char* header_updated;
  const char* house_name;
};

extern Translations lang;

void initLanguage();
const char* translateWeatherState(const char *state);
const char* translateFamilyState(const char *state);

#endif
