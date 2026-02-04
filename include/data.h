#ifndef DATA_H
#define DATA_H

#include <Arduino.h>

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

  Person family[4];
  ForecastDay forecast[5];  // 5-day forecast
};

extern DashboardData data;
extern volatile bool redraw_requested;

#endif