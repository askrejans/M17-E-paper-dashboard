// VFD — 80s hi-fi fluorescent display on black glass.
//
// Visual language: lit seven-segment numerals with faint "ghost" segments,
// segmented level meters, a spectrum-analyser bar display, boxed annunciators
// and white printed legends on the mask. Every element shows live data; there
// is no decoration that pretends to be a control.
#include "display/theme.h"
#include "display/theme_common.h"
#include "display/drawing.h"
#include "display/glyphs.h"
#include "utils/utils.h"
#include "lang.h"

static const int HDR_H = 76;
static const int PANEL_Y = 86;
static const int PANEL_H = 186;
static const int PANEL_GAP_Y = 10;
static const int PANEL_X = 10;
static const int PANEL_W = 252;
static const int PANEL_GAP_X = 12;

static const int LIT = YELLOW;    // phosphor
static const int MASK = WHITE;    // printed legends
static const int GHOST = YELLOW;  // drawn at 25 % density

// Values always glow in phosphor (red on black glass is hard to read);
// out-of-range values get a red marker dot instead.
static int lit(bool) { return LIT; }

static void alertDot(int cx, int cy) { fillCircle(cx, cy, 3, RED); }

// Right-aligned phosphor text; red dot in front of it when alert.
static void valueRight(int xr, int y, const char *s, bool alert)
{
  textRight(xr, y, s, LIT);
  if (alert)
    alertDot(xr - textWidth(s) - 7, y + 8);
}

// ── VFD primitives ─────────────────────────────────────────────

// Boxed annunciator: lit = filled phosphor with black legend.
static void annunciator(int x, int y, int w, const char *label, bool on, bool alert = false)
{
  const int h = 20;
  if (on)
  {
    fillRect(x, y, w, h, alert ? RED : LIT);
    textCenter(x + w / 2, y + 2, label, alert ? WHITE : BLACK);
  }
  else
  {
    hlineDotted(x, y, w, GHOST, 1, 1);
    hlineDotted(x, y + h - 1, w, GHOST, 1, 1);
    vlineDotted(x, y, h, GHOST, 1, 1);
    vlineDotted(x + w - 1, y, h, GHOST, 1, 1);
    textDim(x + w / 2 - textWidth(label) / 2, y + 2, label, GHOST);
  }
}

// Segmented horizontal meter. frac 0..1; peak (0..1 or <0) drawn as red tick;
// redFrom (0..1, >1 = never) turns segments above it red.
static void meter(int x, int y, int w, int h, float frac, float peak = -1, float redFrom = 2)
{
  const int segW = 5, gap = 2;
  int n = (w + gap) / (segW + gap);
  if (isnan(frac)) frac = 0;
  if (frac < 0) frac = 0;
  if (frac > 1) frac = 1;
  int on = (int)(frac * n + 0.5f);
  int pk = peak >= 0 ? (int)(fminf(peak, 1) * n + 0.5f) - 1 : -1;
  for (int i = 0; i < n; i++)
  {
    int sx = x + i * (segW + gap);
    if (i < on)
      fillRect(sx, y, segW, h, (float)i / n >= redFrom ? RED : LIT);
    else if (i == pk)
      fillRect(sx, y, segW, h, RED);
    else
      for (int yy = y; yy < y + h; yy += 2)
        px(sx + 2, yy, GHOST);
  }
}

// Each icon pixel becomes a 2x2 dot with a 1 px gap: dot-matrix VFD look.
static void iconDots(int x, int y, const uint8_t *data, int c)
{
  for (int row = 0; row < 24; row++)
    for (int col = 0; col < 24; col++)
      if (pgm_read_byte(&data[row * 3 + col / 8]) & (0x80 >> (col % 8)))
        fillRect(x + col * 3, y + row * 3, 2, 2, c);
}

// Printed legend tab on the mask (white box, black text) + thin frame.
static void panelFrame(int x, int y, int w, int h, const char *title)
{
  char t[32];
  strlcpy(t, title, sizeof(t));
  utf8Upper(t);
  int tw = textWidth(t) + 12;
  // frame with open corners (mask print style)
  hline(x + 6, y + 9, w - 12, MASK);
  hline(x + 6, y + h - 1, w - 12, MASK);
  vline(x, y + 15, h - 22, MASK);
  vline(x + w - 1, y + 15, h - 22, MASK);
  line(x, y + 15, x + 6, y + 9, MASK);
  line(x + w - 7, y + 9, x + w - 1, y + 15, MASK);
  line(x, y + h - 7, x + 6, y + h - 1, MASK);
  line(x + w - 1, y + h - 7, x + w - 7, y + h - 1, MASK);
  fillRect(x + 14, y, tw, 19, MASK);
  text(x + 20, y + 2, t, BLACK);
}

static void label(int x, int y, const char *s) { text(x, y, s, MASK); }

// Seven-segment value with ghost "8"s behind it, right-aligned unit legend.
static int segValue(int x, int y, const char *v, int h, int c, const char *unit = nullptr)
{
  sevenSeg(x, y, v, h, c, GHOST);
  int w = sevenSegWidth(v, h);
  if (unit && unit[0])
    text(x + w + 4, y + h - 16, unit, MASK);
  return w;
}

static float frac(float v, float lo, float hi)
{
  if (isnan(v) || hi <= lo)
    return 0;
  return (v - lo) / (hi - lo);
}

static float logFrac(float v, float lo, float hi)
{
  if (isnan(v) || v <= 0)
    return 0;
  return (log10f(v) - log10f(lo)) / (log10f(hi) - log10f(lo));
}

// ── header ─────────────────────────────────────────────────────

static void header(const DashboardData &d, const Settings &s)
{
  // identity
  textScaled(16, 10, s.name, LIT, 2);
  label(16, 46, lang.header_info);

  // Annunciators: plain words, lit only when the condition is true.
  StatusFlags f = computeStatus(d, s);
  char home[24], open[24], word[24];
  snprintf(home, sizeof(home), "%s %d/%d", lang.ann_home, f.home, f.people);
  utf8Upper(home);
  snprintf(open, sizeof(open), "%s %d", lang.ann_open, d.house.openCount > 0 ? d.house.openCount : 0);
  utf8Upper(open);
  const int ax = 214, aw = 96, ag = 6;
  auto ann = [&](int col, int row, const char *txt, bool on, bool alert) {
    strlcpy(word, txt, sizeof(word));
    utf8Upper(word);
    annunciator(ax + col * (aw + ag), 10 + row * 26, aw, word, on, alert);
  };
  ann(0, 0, home, f.home > 0, false);
  ann(1, 0, lang.ann_rain, f.rain, false);
  ann(2, 0, lang.ann_frost, f.frost, false);
  ann(0, 1, lang.ann_wind, f.windy, true);
  ann(1, 1, open, f.open, true);
  ann(2, 1, lang.ann_old_data, d.stale, true);

  // clock
  const char *tm = d.time[0] ? d.time : "--:--";
  int cw = sevenSegWidth("88:88", 54);
  int cx = EPD_WIDTH - 14 - cw;
  sevenSeg(cx, 10, tm, 54, LIT, GHOST);
  char wd[32];
  strlcpy(wd, weekdayName(d, 0, true), sizeof(wd));
  utf8Upper(wd);
  textRight(cx - 14, 14, wd, MASK);
  textRight(cx - 14, 36, d.date[0] ? d.date : "--", LIT);

  hlineDotted(10, HDR_H, EPD_WIDTH - 20, MASK, 2, 3);
}

// ── widgets ────────────────────────────────────────────────────

static void outRow(int x, int y, int w, const char *name, float v, int dec, const char *unit, float barFrac,
                   bool alert, float hotFrom = 2)
{
  char lbl[16];
  utf8Clip(lbl, sizeof(lbl), name, 9);
  text(x + 14, y, lbl, MASK);
  char b[24];
  fmtNum(b, sizeof(b), v, dec, unit);
  valueRight(x + w - 12, y, b, alert);
  meter(x + 90, y + 4, w - 90 - 96, 8, barFrac, -1, hotFrom);
}

static void wOutdoor(const DashboardData &d, const Settings &s, const WidgetView &v, int x, int y, int w, int)
{
  const Thresholds &t = s.th;
  int hw = segValue(x + 14, y + 28, v.hero, 58, LIT);
  text(x + 14 + hw + 4, y + 28, "\xC2\xB0" "C", MASK);
  if (v.heroAlert)
    alertDot(x + 14 + hw + 10, y + 78);
  iconDots(x + w - 84, y + 22, v.heroIcon, LIT);
  char cap[48];
  if (has(d.weather.clouds))
    snprintf(cap, sizeof(cap), "%s \xC2\xB7 " G_CLOUD "%.0f%%", v.caption, d.weather.clouds);
  else
    strlcpy(cap, v.caption, sizeof(cap));
  char clip[48];
  utf8Clip(clip, sizeof(clip), cap, (w - 28) / 8);
  text(x + 14, y + 92, clip, LIT);

  float wmax = fmaxf(t.gust * 2, 40);
  outRow(x, y + 114, w, lang.label_humidity, d.weather.hum, 0, "%", frac(d.weather.hum, 0, 100), false);
  outRow(x, y + 131, w, lang.label_wind, d.weather.wind, 0, s.windUnit, frac(d.weather.wind, 0, wmax),
         alertWind(t, d.weather.wind), frac(t.wind, 0, wmax));
  outRow(x, y + 148, w, lang.label_gusts, d.weather.gust, 0, s.windUnit, frac(d.weather.gust, 0, wmax),
         alertGust(t, d.weather.gust), frac(t.gust, 0, wmax));
  outRow(x, y + 165, w, lang.label_pressure, d.weather.press, 0, "hPa", frac(d.weather.press, 960, 1060), false);
}

static void wIndoor(const WidgetView &v, const Settings &s, int x, int y, int w, int h)
{
  int top = y + 26;
  int step = v.n ? (h - 32) / v.n : 30;
  if (step > 31) step = 31;
  int sh = step >= 28 ? 20 : 16;
  for (int i = 0; i < v.n; i++)
  {
    const WRow &r = v.rows[i];
    int ry = top + i * step;
    char nm[16];
    utf8Clip(nm, sizeof(nm), r.label, 10);
    text(x + 12, ry + (sh - 16) / 2 + 1, nm, MASK);
    if (r.alert)
      alertDot(x + 6, ry + sh / 2);
    char tv[12];
    strlcpy(tv, r.value, sizeof(tv));
    char *deg = strstr(tv, "\xC2\xB0");
    if (deg)
      *deg = 0;
    sevenSeg(x + 98, ry, tv, sh, lit(r.alert), GHOST);
    float hum = parseNumber(r.value2);
    meter(x + 150, ry + sh / 2 - 5, w - 150 - 44, 10, frac(hum, 0, 100), -1,
          frac(s.th.inHumHigh, 0, 100));
    textRight(x + w - 10, ry + (sh - 16) / 2 + 1, r.value2, LIT);
  }
}

static void wForecast(const DashboardData &d, const WidgetView &v, int x, int y, int w, int h)
{
  int n = v.n > 5 ? 5 : v.n;
  if (!n)
    return;
  int cw = (w - 12) / n;
  float pmax = 10;
  for (int i = 0; i < n; i++)
    if (has(d.forecast[i].precipitation) && d.forecast[i].precipitation > pmax)
      pmax = d.forecast[i].precipitation;
  for (int i = 0; i < n; i++)
  {
    const ForecastDay &f = d.forecast[i];
    int cx = x + 6 + i * cw + cw / 2;
    char wd[8];
    strlcpy(wd, v.rows[i].label, sizeof(wd));
    utf8Upper(wd);
    textCenter(cx, y + 26, wd, MASK);
    icon(cx - 12, y + 46, v.rows[i].icon, LIT);
    char b[12];
    fmtNum(b, sizeof(b), f.tempHigh, 0, "");
    int sw = sevenSegWidth(b, 22);
    sevenSeg(cx - sw / 2, y + 78, b, 22, LIT, GHOST);
    fmtNum(b, sizeof(b), f.tempLow, 0, "");
    strlcat(b, "\xC2\xB0", sizeof(b));
    textCenter(cx, y + 104, b, MASK);
    // precipitation: vertical segmented column
    int bh = 36, by = y + 124, segs = 6;
    float pf = has(f.precipitation) ? f.precipitation / pmax : 0;
    int on = (int)(pf * segs + (pf > 0 ? 0.99f : 0));
    for (int k = 0; k < segs; k++)
    {
      int sy = by + bh - (k + 1) * (bh / segs);
      if (k < on)
        fillRect(cx - 6, sy, 12, bh / segs - 2, LIT);
      else
        for (int xx = cx - 6; xx < cx + 6; xx += 2)
          px(xx, sy + 1, GHOST);
    }
    fmtNum(b, sizeof(b), f.precipitation, f.precipitation > 0 && f.precipitation < 10 ? 1 : 0, "");
    textCenter(cx, by + bh + 2, b, MASK);
  }
  (void)h;
}

static void wFamily(const WidgetView &v, int x, int y, int w, int)
{
  int bw = (w - 36) / 2;
  for (int i = 0; i < v.n && i < 6; i++)
  {
    const WRow &r = v.rows[i];
    int bx = x + 12 + (i % 2) * (bw + 12);
    int by = y + 28 + (i / 2) * 50;
    char nm[16];
    utf8Clip(nm, sizeof(nm), r.label, bw / 8 - 1);
    utf8Upper(nm);
    annunciator(bx, by, bw, nm, r.on);
    if (!r.on)
    {
      char st[24];
      utf8Clip(st, sizeof(st), r.value, bw / 8);
      textCenter(bx + bw / 2, by + 24, st, MASK);
    }
    else
      textCenter(bx + bw / 2, by + 24, r.value, MASK);
  }
}

// Three labelled readouts: name on the mask, value + unit in phosphor, and a
// bar underneath (speeds on a log scale up to 1 Gbps, ping up to 2x its limit).
static void netRow(int x, int y, int w, const char *name, const char *glyph, float v, const char *unit,
                   float barFrac, bool alert, int dec = 0)
{
  char lbl[32];
  snprintf(lbl, sizeof(lbl), "%s %s", glyph, name);
  utf8Upper(lbl);
  text(x + 14, y + 6, lbl, MASK);
  char b[16];
  fmtNum(b, sizeof(b), v, dec, "");
  int uw = textWidth(unit);
  int sw = sevenSegWidth(b, 24);
  sevenSeg(x + w - 14 - uw - 4 - sw, y, b, 24, LIT);
  if (alert)
    alertDot(x + w - 14 - uw - 4 - sw - 9, y + 12);
  text(x + w - 14 - uw, y + 8, unit, MASK);
  meter(x + 14, y + 30, w - 28, 7, barFrac, -1, alert ? 0 : 2);
}

static void wNetwork(const DashboardData &d, const Settings &s, const WidgetView &, int x, int y, int w, int)
{
  const Thresholds &t = s.th;
  netRow(x, y + 28, w, lang.net_download, G_DOWN, d.net.down, "Mbps", logFrac(d.net.down, 1, 1000),
         alertDown(t, d.net.down), d.net.down < 10 ? 1 : 0);
  netRow(x, y + 78, w, lang.net_upload, G_UP, d.net.up, "Mbps", logFrac(d.net.up, 1, 1000), alertUp(t, d.net.up),
         d.net.up < 10 ? 1 : 0);
  netRow(x, y + 128, w, lang.net_latency, G_SIGNAL, d.net.ping, "ms", frac(d.net.ping, 0, t.pingHigh * 2),
         alertPing(t, d.net.ping));
}

static void smallSeg(int x, int y, const char *lbl, float v, int dec, bool alert)
{
  char b[16];
  fmtNum(b, sizeof(b), v, dec, "");
  label(x, y + 2, lbl);
  sevenSeg(x + textWidth(lbl) + 6, y, b, 18, LIT, GHOST);
  if (alert)
    alertDot(x + textWidth(lbl) + 6 + sevenSegWidth(b, 18) + 6, y + 9);
}

static void textRow(int x, int y, int w, const char *name, float v, int dec, const char *unit, bool alert)
{
  char lbl[24];
  utf8Clip(lbl, sizeof(lbl), name, 14);
  text(x + 14, y, lbl, MASK);
  char b[24];
  fmtNum(b, sizeof(b), v, dec, unit);
  valueRight(x + w - 12, y, b, alert);
}

static void miniPriceBars(const Series &se, int x, int y, int w, int h);

static void wPower(const DashboardData &d, const Settings &s, const WidgetView &v, int x, int y, int w, int)
{
  const Thresholds &t = s.th;
  const Power &p = d.power;
  int pw = segValue(x + 14, y + 26, v.hero, 44, LIT, "W");
  if (v.heroAlert)
    alertDot(x + 14 + pw + 6, y + 30);
  // price forecast for today next to the power reading
  int mx0 = x + 14 + pw + 4 + textWidth("W") + 14;
  if (d.prices.count && x + w - 14 - mx0 >= 48)
    miniPriceBars(d.prices, mx0, y + 30, x + w - 14 - mx0, 38);
  meter(x + 14, y + 76, w - 28, 8, frac(p.total, 0, t.powerHigh), -1, 0.8f);
  int ry = y + 92;
  textRow(x, ry, w, lang.label_price, p.price, fabsf(p.price) < 10 ? 3 : 1, s.priceUnit, alertPrice(t, p.price));
  textRow(x, ry += 17, w, lang.label_voltage, p.voltage, 0, "V", alertVoltage(t, p.voltage));
  textRow(x, ry += 17, w, lang.label_frequency, p.frequency, 2, "Hz", alertFrequency(t, p.frequency));
  if (has(p.today))
    textRow(x, ry += 17, w, lang.label_today, p.today, 1, "kWh", false);
  else
    textRow(x, ry += 17, w, lang.label_power_factor, normFactor(p.factor), 2, "", alertFactor(t, p.factor));
  textRow(x, ry += 17, w, lang.label_month, p.month, 0, "kWh", alertMonth(t, p.month));
}

// Spectrum-analyser style hourly price display.
static void wPrices(const DashboardData &d, const Settings &s, const WidgetView &v, int x, int y, int w, int h)
{
  int pw = segValue(x + 14, y + 28, v.hero, 30, LIT, s.priceUnit);
  if (v.heroAlert)
    alertDot(x + 14 + pw + 6, y + 30);
  char b[24];
  snprintf(b, sizeof(b), G_TRI_UP "%s", v.rows[2].value);
  char *sp = strchr(b, ' ');
  if (sp) *sp = 0;
  valueRight(x + w - 10, y + 26, b, v.rows[2].alert);
  snprintf(b, sizeof(b), G_TRI_DOWN "%s", v.rows[1].value);
  sp = strchr(b, ' ');
  if (sp) *sp = 0;
  textRight(x + w - 10, y + 44, b, LIT);

  const Series &se = d.prices;
  int n = se.count;
  if (!n)
    return;
  int ax = x + 12, aw = w - 24;
  int top = y + 68, bottom = y + h - 22;
  int cw = aw / n;
  if (cw < 3) cw = 3;
  int segH = 4, segGap = 2;
  int segs = (bottom - top) / (segH + segGap);
  float hi = isnan(v.sMax) || v.sMax <= 0 ? 1 : v.sMax;
  float lo = isnan(v.sMin) || v.sMin > 0 ? 0 : v.sMin;
  for (int i = 0; i < n; i++)
  {
    float f = isnan(se.v[i]) ? 0 : (se.v[i] - lo) / (hi - lo);
    int on = (int)(f * segs + 0.5f);
    if (on < 1 && !isnan(se.v[i])) on = 1;
    bool expensive = alertPrice(s.th, se.v[i]);
    int cx = ax + i * cw;
    for (int k = 0; k < segs; k++)
    {
      int sy = bottom - (k + 1) * (segH + segGap);
      if (k < on)
        fillRect(cx, sy, cw - 2, segH, i == se.now ? RED : (expensive ? RED : LIT));
      else if (i == se.now)
        hline(cx, sy + segH - 1, cw - 2, RED);
      else
        px(cx + (cw - 2) / 2, sy + 2, GHOST);
    }
    if (i == se.now)
      textCenter(cx + cw / 2, bottom + 4, G_TRI_UP, RED);
  }
  for (int hr = 0; hr < n; hr += 6)
  {
    char hb[4];
    snprintf(hb, sizeof(hb), "%d", hr * 24 / n);
    if (hr * cw + 8 < aw && (se.now < hr - 1 || se.now > hr + 2))
      text(ax + hr * cw, bottom + 4, hb, MASK);
  }
}

static void wHourly(const DashboardData &d, const WidgetView &v, int x, int y, int w, int h)
{
  segValue(x + 14, y + 28, v.hero, 30, LIT, "\xC2\xB0" "C");
  char b[24];
  snprintf(b, sizeof(b), G_TRI_UP "%s", v.rows[1].value);
  textRight(x + w - 10, y + 26, b, LIT);
  snprintf(b, sizeof(b), G_TRI_DOWN "%s", v.rows[0].value);
  textRight(x + w - 10, y + 44, b, MASK);
  drawHourlyCurve(d.hourly, v.sMin, v.sMax, x + 12, y + 70, w - 24, h - 70 - 24, LIT, RED, MASK, MASK);
}

// Compact price forecast: one bar per hour, current hour red.
static void miniPriceBars(const Series &se, int x, int y, int w, int h)
{
  if (!se.count)
    return;
  float lo = 0, hi = 0;
  for (int i = 0; i < se.count; i++)
    if (!isnan(se.v[i]))
    {
      if (se.v[i] > hi) hi = se.v[i];
      if (se.v[i] < lo) lo = se.v[i];
    }
  if (hi <= lo)
    hi = lo + 1;
  int bw = w / se.count;
  if (bw < 2)
    bw = 2;
  for (int i = 0; i < se.count; i++)
  {
    if (isnan(se.v[i]))
      continue;
    int bh = (int)((se.v[i] - lo) / (hi - lo) * (h - 1)) + 1;
    int bx = x + i * bw;
    if (i == se.now)
      fillRect(bx, y + h - bh, bw - 1 > 0 ? bw - 1 : 1, bh, RED);
    else
      for (int yy = y + h - bh; yy < y + h; yy += 2) // segmented column
        hline(bx, yy, bw - 1 > 0 ? bw - 1 : 1, LIT);
  }
  hlineDotted(x, y + h + 1, bw * se.count, MASK, 1, 1);
}

static void wHome(const DashboardData &d, const WidgetView &v, int x, int y, int w, int h)
{
  // people: lit chip = at home; away people show their location under the chip
  int n = d.familyCount > 6 ? 6 : d.familyCount;
  int perRow = n <= 2 ? (n ? n : 1) : (n <= 4 ? 2 : 3); // 2 per row leaves room for zone names
  int rowsUsed = n ? (n + perRow - 1) / perRow : 0;
  int cw = (w - 28 - (perRow - 1) * 6) / perRow;
  const int rowH = 38;
  for (int i = 0; i < n; i++)
  {
    const Person &p = d.family[i];
    bool home = strcmp(p.state, "home") == 0;
    int bx = x + 14 + (i % perRow) * (cw + 6), by = y + 26 + (i / perRow) * rowH;
    char nm[16];
    utf8Clip(nm, sizeof(nm), p.name, cw / 8 - 1);
    utf8Upper(nm);
    annunciator(bx, by, cw, nm, home);
    if (!home)
    {
      char loc[16];
      utf8Clip(loc, sizeof(loc), translateFamilyState(p.state), cw / 8);
      textCenter(bx + cw / 2, by + 22, loc, LIT);
    }
  }
  int ty = y + 26 + rowsUsed * rowH + (rowsUsed ? 2 : 0);

  // thermostat: set temperature (large), current + heating state on the right
  const Thermostat &t = d.thermostat;
  if (!isnan(t.target))
  {
    text(x + 14, ty + 10, G_THERMO, MASK);
    int sw = segValue(x + 26, ty, v.hero, 36, LIT);
    text(x + 26 + sw + 3, ty, "\xC2\xB0" "C", MASK);
    int rx = x + w / 2 + 16;
    char b[24];
    fmtTemp(b, sizeof(b), t.current, 1, true);
    text(rx, ty, lang.label_now, MASK);
    textRight(x + w - 12, ty, b, LIT);
    if (v.caption[0])
    {
      bool heating = !strcmp(t.action, "heating") || !strcmp(t.action, "preheating");
      char cap[16];
      utf8Clip(cap, sizeof(cap), v.caption, (x + w - 12 - rx) / 8);
      utf8Upper(cap);
      annunciator(rx, ty + 18, x + w - 12 - rx, cap, heating);
    }
    ty += 44;
  }

  // daily temperature mini chart (next 24 h)
  int ch = y + h - 20 - ty;
  if (d.hourly.count && ch >= 18)
    drawHourlyCurve(d.hourly, v.sMin, v.sMax, x + 12, ty, w - 24, ch, LIT, RED, MASK, MASK);
}

static void wRows(const WidgetView &v, int x, int y, int w, int h)
{
  bool cal = !strcmp(v.id, "calendar");
  int cy = y + 28;
  int step = v.n ? (h - 34) / v.n : 20;
  if (step > 24) step = 24;
  if (step < 17) step = 17;
  for (int i = 0; i < v.n && cy + 16 <= y + h - 2; i++)
  {
    const WRow &r = v.rows[i];
    if (cal)
    {
      // time in phosphor, title on the mask
      int tw = r.value[0] ? textWidth(r.value) + 8 : 0;
      text(x + 14, cy, r.value, LIT);
      char t[40];
      utf8Clip(t, sizeof(t), r.label, (w - 28 - tw) / 8);
      text(x + 14 + tw, cy, t, MASK);
    }
    else if (!r.value[0])
    {
      char t[40];
      utf8Clip(t, sizeof(t), r.label, (w - 28) / 8);
      text(x + 14, cy, t, LIT);
      if (r.alert)
        alertDot(x + 7, cy + 8);
    }
    else if (!r.label[0])
      text(x + 14, cy, r.value, LIT);
    else
    {
      char lab[32];
      utf8Clip(lab, sizeof(lab), r.label, (w - 28) / 8 - utf8Len(r.value) - 1);
      text(x + 14, cy, lab, MASK);
      valueRight(x + w - 12, cy, r.value, r.alert);
    }
    cy += step;
  }
}

static void wAgenda(const DashboardData &d, int x, int y, int w, int)
{
  char wd[32];
  strlcpy(wd, weekdayName(d, 0, true), sizeof(wd));
  utf8Upper(wd);
  text(x + 14, y + 28, wd, LIT);
  textRight(x + w - 12, y + 28, d.date, MASK);
  int sy = y + 54;
  label(x + 14, sy + 6, G_SUN G_UP);
  sevenSeg(x + 40, sy, d.sunrise[0] ? d.sunrise : "--:--", 26, LIT, GHOST);
  label(x + w / 2 + 6, sy + 6, G_SUN G_DOWN);
  sevenSeg(x + w / 2 + 32, sy, d.sunset[0] ? d.sunset : "--:--", 26, LIT, GHOST);
  if (d.message[0])
  {
    // word-wrap message over up to 3 lines
    int maxc = (w - 28) / 8;
    const char *p = d.message;
    int ly = y + 100;
    for (int line = 0; line < 3 && *p; line++)
    {
      char buf[40];
      utf8Clip(buf, sizeof(buf), p, maxc);
      int len = strlen(buf);
      if ((int)strlen(p) > len)
      {
        char *sp = strrchr(buf, ' ');
        if (sp && sp > buf + 4)
        {
          *sp = 0;
          len = sp - buf + 1;
        }
      }
      text(x + 14, ly, buf, line == 0 ? LIT : MASK);
      p += len;
      while (*p == ' ')
        p++;
      ly += 20;
    }
  }
}

static void wSystem(const DashboardData &d, const Settings &s, int x, int y, int w, int)
{
  char b[24];
  snprintf(b, sizeof(b), "%d", (int)d.sys.rssi);
  int rw = segValue(x + 14, y + 30, b, 40, LIT, "dBm");
  if (alertRssi(s.th, d.sys.rssi))
    alertDot(x + 14 + rw + 6, y + 34);
  meter(x + 14, y + 78, w - 28, 12, frac((float)d.sys.rssi, -100, -30));
  int ry = y + 100;
  auto row = [&](const char *l, const char *v) {
    label(x + 14, ry, l);
    textRight(x + w - 12, ry, v, LIT);
    ry += 20;
  };
  row("IP", d.sys.ip[0] ? d.sys.ip : "--");
  snprintf(b, sizeof(b), "%lu KB", (unsigned long)(d.sys.heapFree / 1024));
  row(lang.label_heap, b);
  snprintf(b, sizeof(b), "#%lu  v%s", (unsigned long)d.sys.wakeCount, FW_VERSION);
  row(lang.label_wakes, b);
}

static void panel(const char *id, const DashboardData &d, const Settings &s, int x, int y, int w, int h)
{
  WidgetView v;
  if (!buildWidget(id, d, s, v))
    return;
  panelFrame(x, y, w, h, v.title);
  if (v.empty && strcmp(id, "system") != 0)
  {
    char nd[24];
    strlcpy(nd, lang.label_no_data, sizeof(nd));
    utf8Upper(nd);
    sevenSeg(x + w / 2 - sevenSegWidth("----", 30) / 2, y + h / 2 - 24, "----", 30, LIT, GHOST);
    textCenter(x + w / 2, y + h / 2 + 14, nd, MASK);
    return;
  }
  if (!strcmp(id, "outdoor")) wOutdoor(d, s, v, x, y, w, h);
  else if (!strcmp(id, "indoor")) wIndoor(v, s, x, y, w, h);
  else if (!strcmp(id, "forecast")) wForecast(d, v, x, y, w, h);
  else if (!strcmp(id, "family")) wFamily(v, x, y, w, h);
  else if (!strcmp(id, "network")) wNetwork(d, s, v, x, y, w, h);
  else if (!strcmp(id, "power")) wPower(d, s, v, x, y, w, h);
  else if (!strcmp(id, "prices")) wPrices(d, s, v, x, y, w, h);
  else if (!strcmp(id, "agenda")) wAgenda(d, x, y, w, h);
  else if (!strcmp(id, "system")) wSystem(d, s, x, y, w, h);
  else if (!strcmp(id, "hourly")) wHourly(d, v, x, y, w, h);
  else if (!strcmp(id, "home")) wHome(d, v, x, y, w, h);
  else wRows(v, x, y, w, h);
}

void themeVfd(const DashboardData &d, const Settings &s)
{
  fill(BLACK);
  header(d, s);
  for (int i = 0; i < DASH_SLOTS; i++)
  {
    int x = PANEL_X + (i % 3) * (PANEL_W + PANEL_GAP_X);
    int y = PANEL_Y + (i / 3) * (PANEL_H + PANEL_GAP_Y);
    panel(s.slots[i], d, s, x, y, PANEL_W, PANEL_H);
  }
}
