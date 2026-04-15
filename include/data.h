#ifndef DATA_H
#define DATA_H

#include <Arduino.h>
#include "config.h"

enum
{
  BLACK = 0,
  WHITE = 1,
  YELLOW = 2,
  RED = 3
};

struct Person
{
  char name[24];
  char state[32];
};

struct ForecastDay
{
  char condition[24];
  float tempLow;
  float tempHigh;
  float precipitation;
};

struct DashboardData
{
  char date[16];
  char time[8];
  char weatherState[24];

  float outTemp, outHum, outPress, outWind, outGust, clouds;
  float lrT, lrH, brT, brH, krT, krH, baT, baH, ofT, ofH;
  float down, up, ping;
  float powerPrice, powerTotal, powerVoltage, powerFrequency, powerFactor, powerMonth;

  Person family[MAX_FAMILY];
  ForecastDay forecast[MAX_FORECAST];

  // System stats (populated at draw time)
  int32_t wifiRssi;
  uint8_t wifiChannel;
  char ip[16];
  uint32_t heapFree;
  uint32_t psramFree;
  uint32_t uptime;
  uint32_t wakeCount;
};

extern DashboardData data;
extern volatile bool redraw_requested;

#endif