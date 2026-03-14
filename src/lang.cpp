#include "lang.h"
#include "secrets.h"

Translations lang;

// Latvian weekday abbreviations: Sv P O T C P Se
static const char* LV_weekdays[] = {"Sv", "P", "O", "T", "C", "P", "Se"};
// English weekday abbreviations: Sun Mon Tue Wed Thu Fri Sat
static const char* EN_weekdays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

// Latvian translations
const Translations LV = {
  // Box titles
  .title_outdoor = "Ārā",
  .title_indoor = "Telpās",
  .title_location = "Atrašanās vieta",
  .title_network = "Tīkls",
  .title_electricity = "Elektrība",
  .title_icon_test = "Ikonu pārbaude",
  .title_forecast = "Prognoze",
  
  // Weather labels
  .label_temperature = "Temperatūra",
  .label_humidity = "Mitrums",
  .label_pressure = "Gaisa spied.",
  .label_wind = "Vējš",
  .label_gusts = "Brāzmās",
  .label_clouds = "Mākoņi",
  
  // Indoor room names
  .room_living = "Viesistaba",
  .room_bedroom = "Guļamistaba",
  .room_kids = "Bērnu istaba",
  .room_bathroom = "Vannasistaba",
  .room_office = "Ofiss",
  .label_temp_hum_header = "              Temp    Mitr",
  
  // Network labels
  .label_download = "Down",
  .label_upload = "Up",
  .label_ping = "Ping",
  
  // Electricity labels
  .label_price = "Cena",
  .label_power = "Jauda",
  .label_voltage = "Sprieg.",
  .label_frequency = "Frekv.",
  .label_power_factor = "PF",
  .label_consumption = "Patēriņš",
  
  // Family states
  .state_home = "Mājās",
  .state_away = "Prom",
  
  // Weather states
  .weather_sunny = "Saulains",
  .weather_clear_night = "Skaidra nakts",
  .weather_cloudy = "Mākoņains",
  .weather_partlycloudy = "Daļēji mākoņains",
  .weather_rainy = "Lietus",
  .weather_pouring = "Stiprs lietus",
  .weather_snowy = "Sniegs",
  .weather_snowy_rainy = "Sniegs un lietus",
  .weather_fog = "Migla",
  .weather_hail = "Krusa",
  .weather_windy = "Vējains",
  .weather_windy_variant = "Vējš un mākoņi",
  .weather_lightning = "Zibens",
  .weather_lightning_rainy = "Pērkona negaiss",
  .weather_exceptional = "Īpašs",
  
  // Header
  .header_updated = "atjaunots @",
  .house_name = "M17",

  // Web dashboard labels
  .web_weather = "Laikapstākļi",
  .web_rooms = "Telpas",
  .web_network = "Tīkls",
  .web_electricity = "Elektrība",
  .web_system = "ESP32",
  .web_updated_ago = "atjaunots pirms",
  .web_state = "Stāvoklis",
  .web_upload = "Augšupielāde",
  .web_monthly = "Mēnesī",
};

// English translations
const Translations EN = {
  // Box titles
  .title_outdoor = "Outdoor",
  .title_indoor = "Indoor",
  .title_location = "Location",
  .title_network = "Network",
  .title_electricity = "Electricity",
  .title_icon_test = "Icon Test",
  .title_forecast = "Forecast",
  
  // Weather labels
  .label_temperature = "Temperature",
  .label_humidity = "Humidity",
  .label_pressure = "Pressure",
  .label_wind = "Wind",
  .label_gusts = "Gusts",
  .label_clouds = "Clouds",
  
  // Indoor room names
  .room_living = "Living Room",
  .room_bedroom = "Bedroom",
  .room_kids = "Kids Room",
  .room_bathroom = "Bathroom",
  .room_office = "Office",
  .label_temp_hum_header = "              Temp    Hum",
  
  // Network labels
  .label_download = "Down",
  .label_upload = "Up",
  .label_ping = "Ping",
  
  // Electricity labels
  .label_price = "Price",
  .label_power = "Power",
  .label_voltage = "Voltage",
  .label_frequency = "Freq.",
  .label_power_factor = "PF",
  .label_consumption = "Consump.",
  
  // Family states
  .state_home = "Home",
  .state_away = "Away",
  
  // Weather states
  .weather_sunny = "Sunny",
  .weather_clear_night = "Clear Night",
  .weather_cloudy = "Cloudy",
  .weather_partlycloudy = "Partly Cloudy",
  .weather_rainy = "Rainy",
  .weather_pouring = "Pouring",
  .weather_snowy = "Snowy",
  .weather_snowy_rainy = "Snow & Rain",
  .weather_fog = "Fog",
  .weather_hail = "Hail",
  .weather_windy = "Windy",
  .weather_windy_variant = "Wind & Clouds",
  .weather_lightning = "Lightning",
  .weather_lightning_rainy = "Thunderstorm",
  .weather_exceptional = "Exceptional",
  
  // Header
  .header_updated = "updated @",
  .house_name = "M17",

  // Web dashboard labels
  .web_weather = "Weather",
  .web_rooms = "Rooms",
  .web_network = "Network",
  .web_electricity = "Electricity",
  .web_system = "ESP32",
  .web_updated_ago = "updated",
  .web_state = "State",
  .web_upload = "Upload",
  .web_monthly = "Monthly",
};

void initLanguage()
{
  // Default to English, check secrets.h for LANGUAGE setting
  #ifdef LANGUAGE
    if (strcmp(LANGUAGE, "LV") == 0) {
      lang = LV;
      lang.weekdays = LV_weekdays;
    } else {
      lang = EN;
      lang.weekdays = EN_weekdays;
    }
  #else
    lang = EN; // Default
    lang.weekdays = EN_weekdays;
  #endif
}

const char* translateWeatherState(const char *state)
{
  if (strcmp(state, "sunny") == 0)
    return lang.weather_sunny;
  if (strcmp(state, "clear-night") == 0)
    return lang.weather_clear_night;
  if (strcmp(state, "cloudy") == 0)
    return lang.weather_cloudy;
  if (strcmp(state, "partlycloudy") == 0)
    return lang.weather_partlycloudy;
  if (strcmp(state, "rainy") == 0)
    return lang.weather_rainy;
  if (strcmp(state, "pouring") == 0)
    return lang.weather_pouring;
  if (strcmp(state, "snowy") == 0)
    return lang.weather_snowy;
  if (strcmp(state, "snowy-rainy") == 0)
    return lang.weather_snowy_rainy;
  if (strcmp(state, "fog") == 0)
    return lang.weather_fog;
  if (strcmp(state, "hail") == 0)
    return lang.weather_hail;
  if (strcmp(state, "windy") == 0)
    return lang.weather_windy;
  if (strcmp(state, "windy-variant") == 0)
    return lang.weather_windy_variant;
  if (strcmp(state, "lightning") == 0)
    return lang.weather_lightning;
  if (strcmp(state, "lightning-rainy") == 0)
    return lang.weather_lightning_rainy;
  if (strcmp(state, "exceptional") == 0)
    return lang.weather_exceptional;
  
  return state; // fallback
}

const char* translateFamilyState(const char *state)
{
  if (strcmp(state, "home") == 0)
    return lang.state_home;
  if (strcmp(state, "not_home") == 0)
    return lang.state_away;
  
  return state; // fallback
}
