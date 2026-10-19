// Dashboard data model. Platform independent: shared by the firmware, the
// host preview renderer and native unit tests.
//
// Every numeric value defaults to NAN, meaning "not provided"; renderers show
// "--" for it. That keeps partially-configured installations readable.
#ifndef DATA_H
#define DATA_H

#include "compat.h"
#include "config.h"

// 2-bit panel colours (GDEM075F52 native encoding)
enum
{
  BLACK = 0,
  WHITE = 1,
  YELLOW = 2,
  RED = 3
};

struct Weather
{
  char state[24];  // HA condition, e.g. "partlycloudy"
  float temp, hum, press, wind, gust, clouds;
};

struct Room
{
  char name[24];
  float temp, hum;
};

struct Person
{
  char name[20];
  char state[32];  // "home", "not_home" or a zone name
};

struct ForecastDay
{
  char condition[24];
  float tempLow, tempHigh, precipitation;
};

struct Internet
{
  float down, up, ping;
};

struct Power
{
  float price;       // current price, unit from settings.units.price
  float total;       // W
  float voltage;     // V
  float frequency;   // Hz
  float factor;      // 0..1 or percent (auto-detected)
  float month;       // kWh this month
  float today;       // kWh today
};

struct Series
{
  uint8_t count;
  uint8_t now;       // index of the current slot (e.g. current hour)
  float v[MAX_SERIES];
};

struct Extra
{
  char label[20];
  char value[20];
  char unit[8];
};

struct Hourly
{
  uint8_t count;
  int8_t startHour;     // local hour of the first entry, -1 unknown
  float temp[MAX_HOURLY];
  float precip[MAX_HOURLY];
};

struct Event
{
  char when[12];        // "18:00", "Tue 09:30" or "all day"
  char title[40];
};

// Auto-discovered house status (pull mode / HA package template)
struct HouseStatus
{
  int16_t lightsOn;     // -1 = unknown
  int16_t openCount;    // doors/windows/openings currently open
  char open[MAX_NAMES][20];
  int16_t lowBattery;
  char battery[MAX_NAMES][20];
  int16_t updates;      // pending HA update entities
  char media[40];       // "Artist - Title" of a playing media player
};

struct Thermostat
{
  char name[20];
  float target;     // set temperature
  float current;    // measured temperature
  char mode[12];    // HA hvac mode: heat, cool, auto, off ...
  char action[12];  // hvac_action: heating, idle, off ...
};

struct SystemStats
{
  int32_t rssi;
  uint8_t channel;
  char ip[16];
  uint32_t heapFree;
  uint32_t uptime;
  uint32_t wakeCount;
  float chipTemp;
};

struct DashboardData
{
  char date[16];        // formatted per settings.date_format
  char time[8];         // "HH:MM"
  int8_t weekday;       // 0=Sun..6=Sat, -1 when unknown
  int64_t epoch;        // optional Unix time from source, 0 when unknown

  Weather weather;

  uint8_t roomCount;
  Room rooms[MAX_ROOMS];

  uint8_t familyCount;
  Person family[MAX_FAMILY];

  uint8_t forecastCount;
  ForecastDay forecast[MAX_FORECAST];

  Internet net;
  Power power;
  Series prices;        // e.g. Nord Pool hourly prices for today

  uint8_t extraCount;
  Extra extras[MAX_EXTRAS];

  Hourly hourly;
  uint8_t eventCount;
  Event events[MAX_EVENTS];
  HouseStatus house;
  Thermostat thermostat;

  char sunrise[6];
  char sunset[6];
  char message[96];     // free text line (calendar event, notice ...)

  SystemStats sys;
  bool stale;           // rendering last-known data after failed fetches
  bool valid;           // at least one successful update since boot
};

void dataReset(DashboardData &d);
static inline bool has(float v) { return !isnan(v); }

#endif
