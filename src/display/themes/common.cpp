// Drawing helpers shared by several themes.
#include "display/theme_common.h"
#include "display/drawing.h"
#include "data.h"

// Hourly price bar chart; the current hour is drawn red, the rest dithered.
void drawPriceBars(const WidgetView &v, int x, int y, int w, int h, int barC, int nowC, int axisC)
{
  const Series *s = v.series;
  if (!s || !s->count)
    return;
  float lo = v.sMin < 0 ? v.sMin : 0, hi = v.sMax;
  if (isnan(hi) || hi <= lo)
    hi = lo + 1;
  int bw = w / s->count;
  if (bw < 2)
    bw = 2;
  int base = y + h;
  for (int i = 0; i < s->count; i++)
  {
    if (isnan(s->v[i]))
      continue;
    int bh = (int)((s->v[i] - lo) / (hi - lo) * (h - 2));
    if (bh < 1)
      bh = 1;
    int bx = x + i * bw;
    if (i == s->now)
      fillRect(bx, base - bh, bw - 1, bh, nowC);
    else
      fillRectDither(bx, base - bh, bw - 1, bh, barC, WHITE);
    vline(bx, base - bh, bh, barC);
  }
  hline(x, base, bw * s->count, axisC);
  for (int t = 0; t <= s->count; t += 6)
    vline(x + t * bw, base, 3, axisC);
}


void drawHourlyCurve(const Hourly &h, float lo, float hi, int x, int y, int w, int ht, int lineC, int barC,
                     int axisC, int labelC)
{
  if (!h.count)
    return;
  if (isnan(lo) || isnan(hi))
    return;
  if (hi - lo < 4)
  {
    float mid = (hi + lo) / 2;
    lo = mid - 2;
    hi = mid + 2;
  }
  int n = h.count;
  float step = (float)(w - 1) / (n > 1 ? n - 1 : 1);
  int base = y + ht;
  // precipitation bars (mm/h, full height = 4 mm)
  for (int i = 0; i < n; i++)
  {
    float p = h.precip[i];
    if (isnan(p) || p <= 0)
      continue;
    int bh = (int)(fminf(p / 4.0f, 1.0f) * (ht / 2));
    if (bh < 2)
      bh = 2;
    int bx = x + (int)(i * step) - 2;
    for (int k = 0; k < 5; k += 2) // hatched bar keeps the curve readable on top
      vline(bx + k, base - bh, bh, barC);
    hline(bx, base - bh, 5, barC);
  }
  // temperature polyline, 2 px thick
  int px0 = -1, py0 = -1;
  for (int i = 0; i < n; i++)
  {
    if (isnan(h.temp[i]))
      continue;
    int xx = x + (int)(i * step);
    int yy = base - 2 - (int)((h.temp[i] - lo) / (hi - lo) * (ht - 6));
    if (px0 >= 0)
    {
      line(px0, py0, xx, yy, lineC);
      line(px0, py0 + 1, xx, yy + 1, lineC);
    }
    px0 = xx;
    py0 = yy;
  }
  hline(x, base, w, axisC);
  // hour ticks every 6 h
  for (int i = 0; i < n; i++)
  {
    int hour = h.startHour >= 0 ? (h.startHour + i) % 24 : -1;
    bool tick = hour >= 0 ? hour % 6 == 0 : i % 6 == 0;
    if (!tick)
      continue;
    int xx = x + (int)(i * step);
    vline(xx, base, 3, axisC);
    if (hour >= 0 && labelC >= 0)
    {
      char b[4];
      snprintf(b, sizeof(b), "%d", hour);
      if (xx + textWidth(b) < x + w + 4)
        text(xx - (i ? textWidth(b) / 2 : 0), base + 4, b, labelC);
    }
  }
}
