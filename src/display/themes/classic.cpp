// CLASSIC — the original M17 layout (default theme): gradient header, 3x2 grid
// of titled boxes, dithered system bar. Widgets are slot-mappable.
#include "display/theme.h"
#include "display/drawing.h"
#include "display/glyphs.h"
#include "display/theme_common.h"
#include "icons.h"
#include "lang.h"
#include "utils/utils.h"

static const int MARGIN = 10;
static const int COL_GAP = 8;
static const int COL_W = (EPD_WIDTH - 2 * MARGIN - 2 * COL_GAP) / 3;
static const int PAD = 10;
static const int HDR_H = 40;
static const int TITLE_H = 22;
static const int ROW1_Y = HDR_H + 6;
static const int ROW_H = 190;
static const int ROW2_Y = ROW1_Y + ROW_H + 6;

static int col(bool alert) { return alert ? RED : BLACK; }

static void drawHeader(const DashboardData &d, const Settings &s)
{
  fillRectGradientH(0, 0, EPD_WIDTH, 32, BLACK, YELLOW);
  hline(0, 32, EPD_WIDTH, YELLOW);
  fillRectDither(0, 33, EPD_WIDTH, 2, YELLOW, WHITE);

  icon16(12, 8, ICON_HOUSE_16, WHITE);
  text(32, 8, s.name, WHITE);
  if (d.stale)
  {
    int x = 32 + textWidth(s.name) + 12;
    fillRect(x - 4, 6, textWidth(lang.label_stale) + 8, 20, RED);
    text(x, 8, lang.label_stale, WHITE);
  }

  char b[48];
  const char *wd = weekdayName(d, 0);
  snprintf(b, sizeof(b), "%s%s%s", wd, wd[0] ? ", " : "", d.date[0] ? d.date : "--");
  textCenter(EPD_WIDTH / 2, 8, b, WHITE);

  snprintf(b, sizeof(b), "%s %s", lang.header_updated, d.time[0] ? d.time : "--:--");
  textRight(EPD_WIDTH - 16, 8, b, BLACK);
}

static void sep(int x, int y, int w) { hline(x + 8, y, w - 16, BLACK); }

static void drawOutdoor(const WidgetView &v, int x, int y, int w, int)
{
  int cy = y + TITLE_H + 10, lx = x + PAD;
  char b[64];
  int tw = utf8Len(v.caption) * 8, total = 24 + 8 + tw;
  int cx = x + (w - total) / 2;
  icon(cx, cy, v.heroIcon, BLACK);
  text(cx + 32, cy + 4, v.caption, BLACK);
  cy += 28;
  hline(x + 4, cy, w - 8, BLACK);
  cy += 6;

  snprintf(b, sizeof(b), G_THERMO " %s%s", v.hero, strcmp(v.hero, "--") ? "\xC2\xB0" "C" : "");
  text(lx, cy, b, col(v.heroAlert));
  snprintf(b, sizeof(b), G_DROP " %s", v.rows[0].value);
  text(lx + 130, cy, b, BLACK);
  cy += 18;
  // pressure, wind, gusts, clouds
  for (int i = 1; i < v.n; i++)
  {
    const WRow &r = v.rows[i];
    if (i == 2 || i == 3)
      snprintf(b, sizeof(b), "%s %s %s", r.glyph, r.label, r.value);
    else
      snprintf(b, sizeof(b), "%s %s", r.glyph, r.value);
    text(lx, cy, b, col(r.alert));
    cy += 18;
  }
}

static void drawTable(const WidgetView &v, int x, int y, int w, int h)
{
  int cy = y + TITLE_H + 10, lx = x + PAD;
  int tx = x + 140, hx = x + 200;
  char b[32];
  snprintf(b, sizeof(b), G_THERMO " %s", lang.label_temp_short);
  text(tx, cy, b, BLACK);
  snprintf(b, sizeof(b), G_DROP " %s", lang.label_hum_short);
  text(hx, cy, b, BLACK);
  cy += 18;
  int avail = y + h - cy - 4;
  int step = v.n ? avail / v.n : 20;
  if (step > 20) step = 20;
  if (step < 17) step = 17;
  for (int i = 0; i < v.n && cy + 16 <= y + h - 2; i++)
  {
    const WRow &r = v.rows[i];
    if (i % 2 == 1)
      fillRectShadeLight(x + 2, cy - 1, w - 4, 17, WHITE, YELLOW);
    char nm[20];
    utf8Clip(nm, sizeof(nm), r.label, 15);
    text(lx, cy, nm, col(r.alert));
    snprintf(b, sizeof(b), "%s", r.value);
    text(tx, cy, b, col(r.alert));
    text(hx, cy, r.value2, col(r.alert));
    cy += 17;
    if (i < v.n - 1 && step >= 20)
      sep(x, cy, w);
    cy += step - 17;
  }
}

static void drawForecast(const WidgetView &v, int x, int y, int w, int h)
{
  int fy = y + TITLE_H + 10;
  char b[48];
  for (int i = 0; i < v.n && fy + 24 <= y + h; i++)
  {
    const WRow &r = v.rows[i];
    text(x + PAD, fy + 4, r.label, BLACK);
    icon(x + 40, fy, r.icon, BLACK);
    snprintf(b, sizeof(b), "%sC %s", r.value, r.value2);
    text(x + 70, fy + 4, b, BLACK);
    fy += 24;
    if (i < v.n - 1)
      sep(x, fy - 1, w);
  }
}

static void drawPeople(const WidgetView &v, int x, int y, int w, int h)
{
  int cy = y + TITLE_H + 12;
  char b[64];
  for (int i = 0; i < v.n && cy + 20 <= y + h; i++)
  {
    const WRow &r = v.rows[i];
    snprintf(b, sizeof(b), "%s %s: %s", r.glyph, r.label, r.value);
    char clip[64];
    utf8Clip(clip, sizeof(clip), b, (w - 2 * PAD) / 8);
    text(x + PAD, cy + 2, clip, r.on ? BLACK : RED);
    cy += 22;
    if (i < v.n - 1)
      sep(x, cy - 1, w);
  }
}

static void drawMiniPrices(const Series &se, int x, int y, int w, int h);

static void drawList(const WidgetView &v, int x, int y, int w, int h, const Series *mini = nullptr)
{
  bool net = strcmp(v.id, "network") == 0;
  // power: mini price forecast on the right of the first three rows
  bool chart = mini && mini->count && strcmp(v.id, "power") == 0;
  const int chartX = x + w / 2 + 4, chartW = w / 2 - PAD - 4;
  int cy = y + TITLE_H + 12;
  int step = net ? 24 : 20;
  int maxRows = (y + h - cy) / step;
  char b[64];
  for (int i = 0; i < v.n && i < maxRows; i++)
  {
    const WRow &r = v.rows[i];
    if (net)
      snprintf(b, sizeof(b), "%s %s %s", r.glyph, r.label, r.value);
    else if (strcmp(v.id, "power") == 0)
    {
      if (!strcmp(r.glyph, G_PHI))
        snprintf(b, sizeof(b), "%s PF %s", r.glyph, r.value);
      else if (!strcmp(r.glyph, G_METER))
        snprintf(b, sizeof(b), "%s %s  %s", r.glyph, r.value, r.label);
      else
        snprintf(b, sizeof(b), "%s %s", r.glyph, r.value);
    }
    else if (!r.label[0])
      snprintf(b, sizeof(b), "%s %s", r.glyph, r.value);
    else if (!r.value[0])
    {
      char t[40];
      snprintf(b, sizeof(b), "%s%s%s", r.glyph, r.glyph[0] ? " " : "  ", r.label);
      utf8Clip(t, sizeof(t), b, (w - 2 * PAD) / 8);
      strlcpy(b, t, sizeof(b));
    }
    else
    {
      // label left, value right-aligned
      snprintf(b, sizeof(b), "%s %s", r.glyph, r.label);
      char lab[40];
      utf8Clip(lab, sizeof(lab), b, (w - 2 * PAD) / 8 - utf8Len(r.value) - 1);
      text(x + PAD, cy, lab, col(r.alert));
      textRight(x + w - PAD, cy, r.value, col(r.alert));
      goto next;
    }
    text(x + PAD, cy, b, col(r.alert));
  next:
    if (chart && i == 0)
      drawMiniPrices(*mini, chartX, cy, chartW, 3 * step - 8);
    cy += step - 3;
    if (i < v.n - 1 && i < maxRows - 1)
    {
      if (chart && i < 2)
        hline(x + 8, cy, chartX - x - 14, BLACK);
      else
        sep(x, cy, w);
    }
    cy += 3;
  }
}

static void drawMiniPrices(const Series &se, int x, int y, int w, int h)
{
  float hi = 0;
  for (int i = 0; i < se.count; i++)
    if (!isnan(se.v[i]) && se.v[i] > hi)
      hi = se.v[i];
  if (hi <= 0)
    hi = 1;
  int bw = w / se.count;
  if (bw < 2)
    bw = 2;
  for (int i = 0; i < se.count; i++)
  {
    if (isnan(se.v[i]))
      continue;
    int bh = (int)(se.v[i] / hi * (h - 1)) + 1;
    if (bh < 1)
      bh = 1;
    if (i == se.now)
      fillRect(x + i * bw, y + h - bh, bw - 1 > 0 ? bw - 1 : 1, bh, RED);
    else
      fillRectDither(x + i * bw, y + h - bh, bw - 1 > 0 ? bw - 1 : 1, bh, BLACK, WHITE);
  }
  hline(x, y + h, bw * se.count, BLACK);
}

static void drawChart(const WidgetView &v, int x, int y, int w, int h)
{
  int cy = y + TITLE_H + 8;
  char b[48];
  snprintf(b, sizeof(b), G_PRICE " %s %s", v.hero, v.heroUnit);
  text(x + PAD, cy, b, col(v.heroAlert));
  cy += 22;
  drawPriceBars(v, x + PAD, cy, w - 2 * PAD, h - (cy - y) - 30, BLACK, RED, BLACK);
  snprintf(b, sizeof(b), G_TRI_DOWN "%s  " G_TRI_UP "%s", v.rows[1].value, v.rows[2].value);
  char clip[48];
  utf8Clip(clip, sizeof(clip), b, (w - 2 * PAD) / 8);
  text(x + PAD, y + h - 22, clip, BLACK);
}

static void drawHourly(const DashboardData &d, const WidgetView &v, int x, int y, int w, int h)
{
  int cy = y + TITLE_H + 8;
  char b[48];
  snprintf(b, sizeof(b), G_TRI_DOWN "%s " G_TRI_UP "%s " G_DROP "%s", v.rows[0].value, v.rows[1].value,
           v.rows[2].value);
  char clip[48];
  utf8Clip(clip, sizeof(clip), b, (w - 2 * PAD) / 8);
  text(x + PAD, cy, clip, BLACK);
  drawHourlyCurve(d.hourly, v.sMin, v.sMax, x + PAD, cy + 24, w - 2 * PAD, h - (cy - y) - 50, BLACK, RED, BLACK,
                  BLACK);
}

static void drawHome(const DashboardData &d, const WidgetView &v, int x, int y, int w, int h)
{
  int cy = y + TITLE_H + 8;
  // condensed presence: ● home / ○ away, short names
  int cx = x + PAD;
  for (int i = 0; i < d.familyCount; i++)
  {
    bool home = strcmp(d.family[i].state, "home") == 0;
    char nm[12], loc[16], b[40];
    utf8Clip(nm, sizeof(nm), d.family[i].name, 7);
    utf8Clip(loc, sizeof(loc), translateFamilyState(d.family[i].state), 12);
    if (home)
      snprintf(b, sizeof(b), "%s%s", G_FILLED, nm);
    else
      snprintf(b, sizeof(b), "%s%s: %s", G_EMPTY, nm, loc); // where they are
    int bw = textWidth(b) + 8;
    if (cx + bw > x + w - PAD)
    {
      cx = x + PAD;
      cy += 18;
    }
    text(cx, cy, b, home ? BLACK : RED);
    cx += bw;
  }
  if (d.familyCount)
    cy += 20;
  const Thermostat &t = d.thermostat;
  if (!isnan(t.target))
  {
    hline(x + 4, cy - 3, w - 8, BLACK);
    char b[48], cur[12];
    fmtTemp(cur, sizeof(cur), t.current, 1, false);
    snprintf(b, sizeof(b), G_THERMO " %s %s" "\xC2\xB0" "C", lang.label_set, v.hero);
    textBold(x + PAD, cy, b, BLACK);
    snprintf(b, sizeof(b), "%s %s", lang.label_now, cur);
    textRight(x + w - PAD, cy, b, BLACK);
    cy += 18;
    if (v.caption[0])
    {
      bool heating = !strcmp(t.action, "heating") || !strcmp(t.action, "preheating");
      text(x + PAD, cy, v.caption, heating ? RED : BLACK);
      cy += 18;
    }
  }
  int ch = y + h - 22 - cy;
  if (d.hourly.count && ch >= 18)
    drawHourlyCurve(d.hourly, v.sMin, v.sMax, x + PAD, cy + 2, w - 2 * PAD, ch - 2, BLACK, RED, BLACK, BLACK);
}

static void drawSlot(const char *id, const DashboardData &d, const Settings &s, int x, int y, int w, int h)
{
  WidgetView v;
  if (!buildWidget(id, d, s, v))
    return;
  rectWithTitle(x, y, w, h, v.title);
  if (v.empty && strcmp(id, "system") != 0)
  {
    textCenter(x + w / 2, y + h / 2, lang.label_no_data, BLACK);
    return;
  }
  if (strcmp(id, "outdoor") == 0)
    drawOutdoor(v, x, y, w, h);
  else if (v.kind == WK_TABLE)
    drawTable(v, x, y, w, h);
  else if (v.kind == WK_FORECAST)
    drawForecast(v, x, y, w, h);
  else if (v.kind == WK_PEOPLE)
    drawPeople(v, x, y, w, h);
  else if (v.kind == WK_CHART)
    drawChart(v, x, y, w, h);
  else if (v.kind == WK_HOURLY)
    drawHourly(d, v, x, y, w, h);
  else if (v.kind == WK_HOME)
    drawHome(d, v, x, y, w, h);
  else
    drawList(v, x, y, w, h, &d.prices);
}

static void drawSystemBar(const DashboardData &d, const Settings &s)
{
  int sy = ROW2_Y + ROW_H + 4;
  char b[96];
  fillRectDitherDense(0, sy, EPD_WIDTH, 18, WHITE, YELLOW);
  int x = MARGIN + 2, ty = sy + 1;
  auto item = [&](const char *str, int c, bool sepAfter) {
    text(x, ty, str, c);
    x += textWidth(str) + 8;
    if (sepAfter)
    {
      text(x, ty, "|", BLACK);
      x += 12;
    }
  };
  item(d.sys.ip[0] ? d.sys.ip : "--", BLACK, true);
  snprintf(b, sizeof(b), "%ddBm", (int)d.sys.rssi);
  item(b, col(alertRssi(s.th, d.sys.rssi)), true);
  snprintf(b, sizeof(b), "Ch%u", d.sys.channel);
  item(b, BLACK, true);
  snprintf(b, sizeof(b), "Heap %luK", (unsigned long)(d.sys.heapFree / 1024));
  item(b, BLACK, true);
  snprintf(b, sizeof(b), "#%lu", (unsigned long)d.sys.wakeCount);
  item(b, BLACK, true);
  snprintf(b, sizeof(b), "%lus", (unsigned long)d.sys.uptime);
  item(b, BLACK, false);
  textRight(EPD_WIDTH - MARGIN - 2, ty, "v" FW_VERSION, BLACK);

  hline(0, sy + 19, EPD_WIDTH, YELLOW);
  hline(0, sy + 21, EPD_WIDTH, BLACK);
}

void themeClassic(const DashboardData &d, const Settings &s)
{
  fill(WHITE);
  drawHeader(d, s);
  for (int i = 0; i < DASH_SLOTS; i++)
  {
    int x = MARGIN + (i % 3) * (COL_W + COL_GAP);
    int y = i < 3 ? ROW1_Y : ROW2_Y;
    drawSlot(s.slots[i], d, s, x, y, COL_W, ROW_H);
  }
  if (s.systemBar)
    drawSystemBar(d, s);
}
