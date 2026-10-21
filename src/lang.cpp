#include "lang.h"

Translations lang;

static const char *const LV_weekdays[] = {"Sv", "P", "O", "T", "C", "Pk", "S"};
static const char *const LV_weekdays_long[] = {"Svētdiena", "Pirmdiena", "Otrdiena", "Trešdiena",
                                               "Ceturtdiena", "Piektdiena", "Sestdiena"};
static const char *const EN_weekdays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
static const char *const EN_weekdays_long[] = {"Sunday", "Monday", "Tuesday", "Wednesday",
                                               "Thursday", "Friday", "Saturday"};

static const Translations LV = {
    .code = "LV",
    .title_outdoor = "Ārā",
    .title_indoor = "Telpās",
    .title_location = "Atrašanās vieta",
    .title_network = "Tīkls",
    .title_electricity = "Elektrība",
    .title_forecast = "Prognoze",
    .title_prices = "Elektrības cena",
    .title_extras = "Sensori",
    .title_agenda = "Šodien",
    .title_system = "Sistēma",
    .title_hourly = "Tuvākās 24 h",
    .title_house = "Māja",
    .title_calendar = "Kalendārs",
    .label_lights = "Gaismas",
    .label_open = "Atvērts",
    .label_battery = "Baterijas",
    .label_updates = "Atjauninājumi",
    .label_playing = "Atskaņo",
    .label_all_closed = "Viss aizvērts",
    .label_no_events = "Nav notikumu",
    .title_home = "Mājas",
    .label_thermostat = "Termostats",
    .label_set = "Iestatīts",
    .label_people = "Cilvēki",
    .hvac_heating = "Silda",
    .hvac_cooling = "Dzesē",
    .hvac_idle = "Gaida",
    .hvac_off = "Izslēgts",
    .net_download = "Lejupielāde",
    .net_upload = "Augšupielāde",
    .net_latency = "Ping",
    .ann_home = "Mājās",
    .ann_rain = "Lietus",
    .ann_frost = "Salna",
    .ann_wind = "Vējš",
    .ann_open = "Atvērts",
    .ann_old_data = "Veci dati",

    .label_temperature = "Temperatūra",
    .label_humidity = "Mitrums",
    .label_pressure = "Spiediens",
    .label_wind = "Vējš",
    .label_gusts = "Brāzmas",
    .label_clouds = "Mākoņi",

    .room_living = "Viesistaba",
    .room_bedroom = "Guļamistaba",
    .room_kids = "Bērnu istaba",
    .room_bathroom = "Vannasistaba",
    .room_office = "Ofiss",
    .label_temp_short = "Temp",
    .label_hum_short = "Mitr",

    .label_download = "Lejup",
    .label_upload = "Augšup",
    .label_ping = "Ping",

    .label_price = "Cena",
    .label_power = "Jauda",
    .label_voltage = "Spriegums",
    .label_frequency = "Frekvence",
    .label_power_factor = "Jaudas koef.",
    .label_consumption = "Patēriņš",
    .label_today = "Šodien",
    .label_month = "Mēnesī",
    .label_now = "Tagad",
    .label_min = "Min",
    .label_max = "Max",

    .state_home = "Mājās",
    .state_away = "Prom",

    .weather_sunny = "Saulains",
    .weather_clear_night = "Skaidra nakts",
    .weather_cloudy = "Mākoņains",
    .weather_partlycloudy = "Daļēji mākoņains",
    .weather_rainy = "Lietus",
    .weather_pouring = "Stiprs lietus",
    .weather_snowy = "Sniegs",
    .weather_snowy_rainy = "Slapjš sniegs",
    .weather_fog = "Migla",
    .weather_hail = "Krusa",
    .weather_windy = "Vējains",
    .weather_windy_variant = "Vējš un mākoņi",
    .weather_lightning = "Zibens",
    .weather_lightning_rainy = "Pērkona negaiss",
    .weather_exceptional = "Īpaši apstākļi",

    .header_info = "MĀJAS INFO",
    .header_updated = "atjaunots @",
    .label_sunrise = "Saullēkts",
    .label_sunset = "Saulriets",
    .label_no_data = "Nav datu",
    .label_stale = "NOVECOJIS",
    .label_uptime = "Darbībā",
    .label_wakes = "Pamošanās",
    .label_heap = "Atmiņa",
    .label_signal = "Signāls",
    .label_all_home = "Visi mājās",
    .label_nobody_home = "Neviena nav mājās",
    .label_alerts = "Brīdinājumi",
    .label_end_of_report = "ATSKAITES BEIGAS",
    .weekdays = LV_weekdays,
    .weekdays_long = LV_weekdays_long,

    .setup_title = "Iestatīšana",
    .setup_step1 = "1. Pievienojies Wi-Fi tīklam:",
    .setup_step2 = "2. Atver pārlūkā:",
    .setup_step3 = "3. Ievadi Wi-Fi un Home Assistant datus.",
    .wifi_failed = "Wi-Fi: nav savienojuma",
    .wifi_check = "Pārbaudi Wi-Fi iestatījumus. Atkārtošu vēlāk.",
    .maintenance_title = "Apkopes režīms",
    .maintenance_hint = "Iekārta ir nomodā. Administrēšana:",
};

static const Translations EN = {
    .code = "EN",
    .title_outdoor = "Outdoor",
    .title_indoor = "Indoor",
    .title_location = "Location",
    .title_network = "Network",
    .title_electricity = "Electricity",
    .title_forecast = "Forecast",
    .title_prices = "Energy Price",
    .title_extras = "Sensors",
    .title_agenda = "Today",
    .title_system = "System",
    .title_hourly = "Next 24 h",
    .title_house = "House",
    .title_calendar = "Calendar",
    .label_lights = "Lights on",
    .label_open = "Open",
    .label_battery = "Low battery",
    .label_updates = "Updates",
    .label_playing = "Playing",
    .label_all_closed = "All closed",
    .label_no_events = "No events",
    .title_home = "Home",
    .label_thermostat = "Thermostat",
    .label_set = "Set",
    .label_people = "People",
    .hvac_heating = "Heating",
    .hvac_cooling = "Cooling",
    .hvac_idle = "Idle",
    .hvac_off = "Off",
    .net_download = "Download",
    .net_upload = "Upload",
    .net_latency = "Ping",
    .ann_home = "Home",
    .ann_rain = "Rain",
    .ann_frost = "Frost",
    .ann_wind = "Wind",
    .ann_open = "Open",
    .ann_old_data = "Data old",

    .label_temperature = "Temperature",
    .label_humidity = "Humidity",
    .label_pressure = "Pressure",
    .label_wind = "Wind",
    .label_gusts = "Gusts",
    .label_clouds = "Clouds",

    .room_living = "Living Room",
    .room_bedroom = "Bedroom",
    .room_kids = "Kids Room",
    .room_bathroom = "Bathroom",
    .room_office = "Office",
    .label_temp_short = "Temp",
    .label_hum_short = "Hum",

    .label_download = "Down",
    .label_upload = "Up",
    .label_ping = "Ping",

    .label_price = "Price",
    .label_power = "Power",
    .label_voltage = "Voltage",
    .label_frequency = "Frequency",
    .label_power_factor = "Power factor",
    .label_consumption = "Consumption",
    .label_today = "Today",
    .label_month = "Month",
    .label_now = "Now",
    .label_min = "Min",
    .label_max = "Max",

    .state_home = "Home",
    .state_away = "Away",

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

    .header_info = "HOME INFORMATION",
    .header_updated = "updated @",
    .label_sunrise = "Sunrise",
    .label_sunset = "Sunset",
    .label_no_data = "No data",
    .label_stale = "STALE",
    .label_uptime = "Uptime",
    .label_wakes = "Wakes",
    .label_heap = "Heap",
    .label_signal = "Signal",
    .label_all_home = "Everyone home",
    .label_nobody_home = "Nobody home",
    .label_alerts = "Alerts",
    .label_end_of_report = "END OF REPORT",
    .weekdays = EN_weekdays,
    .weekdays_long = EN_weekdays_long,

    .setup_title = "Setup",
    .setup_step1 = "1. Join the Wi-Fi network:",
    .setup_step2 = "2. Open in a browser:",
    .setup_step3 = "3. Enter Wi-Fi and Home Assistant details.",
    .wifi_failed = "Wi-Fi: not connected",
    .wifi_check = "Check Wi-Fi settings. Will retry later.",
    .maintenance_title = "Maintenance mode",
    .maintenance_hint = "Device stays awake. Admin panel:",
};

void setLanguage(const char *code)
{
  lang = (code && strcmp(code, "LV") == 0) ? LV : EN;
}

struct StateName
{
  const char *key;
  const char *Translations::*field;
};

static const StateName WEATHER_STATES[] = {
    {"sunny", &Translations::weather_sunny},
    {"clear-night", &Translations::weather_clear_night},
    {"cloudy", &Translations::weather_cloudy},
    {"partlycloudy", &Translations::weather_partlycloudy},
    {"rainy", &Translations::weather_rainy},
    {"pouring", &Translations::weather_pouring},
    {"snowy", &Translations::weather_snowy},
    {"snowy-rainy", &Translations::weather_snowy_rainy},
    {"fog", &Translations::weather_fog},
    {"hail", &Translations::weather_hail},
    {"windy", &Translations::weather_windy},
    {"windy-variant", &Translations::weather_windy_variant},
    {"lightning", &Translations::weather_lightning},
    {"lightning-rainy", &Translations::weather_lightning_rainy},
    {"exceptional", &Translations::weather_exceptional},
};

const char *translateWeatherState(const char *state)
{
  for (const StateName &s : WEATHER_STATES)
    if (strcmp(state, s.key) == 0)
      return lang.*(s.field);
  return state;
}

const char *translateFamilyState(const char *state)
{
  if (strcmp(state, "home") == 0)
    return lang.state_home;
  if (strcmp(state, "not_home") == 0 || strcmp(state, "away") == 0)
    return lang.state_away;
  return state;
}

const char *translateName(const char *name)
{
  if (name[0] != '@')
    return name;
  if (strcmp(name, "@living") == 0) return lang.room_living;
  if (strcmp(name, "@bedroom") == 0) return lang.room_bedroom;
  if (strcmp(name, "@kids") == 0) return lang.room_kids;
  if (strcmp(name, "@bath") == 0) return lang.room_bathroom;
  if (strcmp(name, "@office") == 0) return lang.room_office;
  return name + 1;
}

const char *translateHvac(const char *action, const char *mode)
{
  const char *a = action && action[0] ? action : mode;
  if (!a || !a[0])
    return "";
  if (!strcmp(a, "heating") || !strcmp(a, "preheating") || !strcmp(a, "heat"))
    return lang.hvac_heating;
  if (!strcmp(a, "cooling") || !strcmp(a, "cool"))
    return lang.hvac_cooling;
  if (!strcmp(a, "idle") || !strcmp(a, "auto") || !strcmp(a, "heat_cool"))
    return lang.hvac_idle;
  if (!strcmp(a, "off"))
    return lang.hvac_off;
  return a;
}
