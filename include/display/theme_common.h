// Helpers shared by theme implementations.
#pragma once

#include "display/widgets.h"

// Hourly price bar chart; the current slot is drawn in `nowC`.
void drawPriceBars(const WidgetView &v, int x, int y, int w, int h, int barC, int nowC, int axisC);

// 24 h temperature curve (lo/hi in °C) with precipitation bars and hour ticks.
void drawHourlyCurve(const Hourly &h, float lo, float hi, int x, int y, int w, int ht, int lineC, int barC,
                     int axisC, int labelC);
