// Theme-agnostic widget views. A view turns DashboardData + Settings into
// already formatted, translated strings with alert flags; themes only decide
// how to lay them out. Adding a widget = one builder here + it shows up in all
// themes and in the admin slot picker.
#ifndef WIDGETS_H
#define WIDGETS_H

#include "data.h"
#include "settings.h"

#define WIDGET_MAX_ROWS 10

enum WidgetKind : uint8_t
{
  WK_LIST,      // label/value rows
  WK_TABLE,     // label + two value columns (rooms)
  WK_FORECAST,  // weekday, icon, temperature range, precipitation
  WK_PEOPLE,    // name + presence
  WK_CHART,     // series chart + summary rows
  WK_HOURLY,    // temperature curve + precipitation bars (d.hourly)
  WK_HOME,      // overview: people, thermostat, daily temperature mini chart
};

struct WRow
{
  const char *glyph;       // leading glyph (G_* macro) or ""
  char label[28];
  char value[24];
  char value2[16];
  bool alert;
  bool on;                 // people: at home
  const uint8_t *icon;     // forecast weather icon
};

struct WidgetView
{
  const char *id;
  const char *title;
  WidgetKind kind;
  char hero[16];           // main value ("16.7"); empty when not applicable
  char heroUnit[12];       // "°C"
  bool heroAlert;
  const uint8_t *heroIcon; // 24x24 icon next to hero
  char caption[40];        // e.g. translated weather state
  uint8_t n;
  WRow rows[WIDGET_MAX_ROWS];
  const Series *series;
  float sMin, sMax;
  bool empty;              // no data for this widget
};

bool buildWidget(const char *id, const DashboardData &d, const Settings &s, WidgetView &v);

// Aggregated status used by headers / lamp rows.
struct StatusFlags
{
  int alerts;
  int home, people;
  bool frost, heat, windy, rain, humid, netBad, powerHigh, priceHigh, open;
};
StatusFlags computeStatus(const DashboardData &d, const Settings &s);

// Weekday name for "today + offset" (short or long), "" when unknown.
const char *weekdayName(const DashboardData &d, int offset, bool longName = false);

// Commonly used formatted strings
void fmtTemp(char *out, size_t n, float v, int decimals = 1, bool unit = true);

#endif
