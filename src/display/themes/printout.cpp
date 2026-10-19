// LINE PRINTER — a mainframe job report on fanfold paper: tractor-feed holes,
// pale "greenbar" bands (yellow on this panel), black/red ribbon, upper-case
// type and dot leaders. Red ink marks out-of-range values, as on two-colour
// ribbons of the era.
#include "display/theme.h"
#include "display/theme_common.h"
#include "display/drawing.h"
#include "display/glyphs.h"
#include "utils/utils.h"
#include "lang.h"

static const int FEED = 30;                 // tractor strip width
static const int PX0 = FEED + 10;           // printable area
static const int PX1 = EPD_WIDTH - FEED - 10;
static const int LINE = 16;
static const int BODY_Y = 96;               // first report line (on the 16 px grid)
static const int COL_W = (PX1 - PX0 - 16) / 2;
static const int COL_CH = COL_W / 8;

static void paper()
{
  fill(WHITE);
  // pale bars: two lines tall, every other pair
  for (int y = BODY_Y - 2; y < EPD_HEIGHT - 24; y += 4 * LINE)
    fillRectLevel(FEED + 2, y, EPD_WIDTH - 2 * FEED - 4, 2 * LINE, WHITE, YELLOW, 70);
  // tractor feed strips
  for (int side = 0; side < 2; side++)
  {
    int x0 = side ? EPD_WIDTH - FEED : 0;
    int perf = side ? EPD_WIDTH - FEED : FEED;
    vlineDotted(perf, 0, EPD_HEIGHT, BLACK, 2, 3);
    for (int y = 12; y < EPD_HEIGHT; y += 40)
    {
      fillCircle(x0 + FEED / 2, y, 6, BLACK);
      fillCircle(x0 + FEED / 2, y, 4, WHITE);
    }
  }
}

struct Col
{
  int x, line;   // line index from BODY_Y
  int maxLine;
};

static int ly(const Col &c) { return BODY_Y + c.line * LINE; }

static void upper(char *dst, size_t n, const char *src, int maxGlyphs)
{
  utf8Clip(dst, n, src, maxGlyphs);
  utf8Upper(dst);
}

static void row(Col &c, const char *label, const char *value, bool alert)
{
  if (c.line >= c.maxLine)
    return;
  char lab[40], val[32];
  upper(val, sizeof(val), value, 16);
  int vl = utf8Len(val);
  upper(lab, sizeof(lab), label, COL_CH - vl - 4 - (alert ? 2 : 0));
  int y = ly(c);
  text(c.x + 16, y, lab, BLACK);
  int from = 2 + utf8Len(lab) + 1, to = COL_CH - vl - 1 - (alert ? 2 : 0);
  for (int k = from; k < to; k++)
    text(c.x + k * 8, y, ".", BLACK);
  int vx = c.x + (COL_CH - vl - (alert ? 2 : 0)) * 8;
  text(vx, y, val, alert ? RED : BLACK);
  if (alert)
    text(c.x + (COL_CH - 2) * 8, y, "<<", RED);
  c.line++;
}

static void line(Col &c, const char *s, int color, int indent = 2)
{
  if (c.line >= c.maxLine)
    return;
  char b[64];
  upper(b, sizeof(b), s, COL_CH - indent);
  text(c.x + indent * 8, ly(c), b, color);
  c.line++;
}

static void section(Col &c, int no, const char *id, const DashboardData &d, const Settings &s)
{
  WidgetView v;
  if (!buildWidget(id, d, s, v))
    return;
  if (c.line + 2 > c.maxLine)
    return;
  char h[48], t[32];
  upper(t, sizeof(t), v.title, 24);
  snprintf(h, sizeof(h), "%d. %s", no, t);
  textBold(c.x, ly(c), h, BLACK);
  c.line++;
  if (v.empty && strcmp(id, "system") != 0)
  {
    line(c, lang.label_no_data, BLACK);
    c.line++;
    return;
  }
  if (!strcmp(id, "outdoor"))
  {
    char b[48];
    snprintf(b, sizeof(b), "%s %s%s", v.caption, v.hero, strcmp(v.hero, "--") ? "\xC2\xB0" "C" : "");
    line(c, b, v.heroAlert ? RED : BLACK);
  }
  for (int i = 0; i < v.n && c.line < c.maxLine; i++)
  {
    const WRow &r = v.rows[i];
    char val[40];
    if (v.kind == WK_TABLE)
      snprintf(val, sizeof(val), "%s %s", r.value, r.value2);
    else if (v.kind == WK_FORECAST)
      snprintf(val, sizeof(val), "%s %s", r.value, r.value2);
    else
      strlcpy(val, r.value, sizeof(val));
    if (!r.label[0])
      line(c, val, r.alert ? RED : BLACK);
    else if (!val[0])
      line(c, r.label, r.alert ? RED : BLACK);
    else
      row(c, r.label, val, r.alert);
  }
  if ((v.kind == WK_CHART || v.kind == WK_HOURLY) && c.line + 4 <= c.maxLine)
  {
    int y = ly(c) + 2;
    if (v.kind == WK_CHART)
      drawPriceBars(v, c.x + 16, y, COL_W - 24, 3 * LINE - 6, BLACK, RED, BLACK);
    else
      drawHourlyCurve(d.hourly, v.sMin, v.sMax, c.x + 16, y, COL_W - 24, 3 * LINE - 6, BLACK, RED, BLACK, -1);
    c.line += 4;
  }
  c.line++; // blank line between sections
}

void themePrintout(const DashboardData &d, const Settings &s)
{
  paper();

  // banner
  char name[24];
  upper(name, sizeof(name), s.name, 8);
  textScaled(PX0, 12, name, BLACK, 3);
  int nx = PX0 + textWidth(name, 3) + 16;
  text(nx, 16, "HOME STATUS REPORT", BLACK);
  char b[64], wd[24];
  upper(wd, sizeof(wd), weekdayName(d, 0, true), 16);
  snprintf(b, sizeof(b), "RUN %s %s  %s", wd, d.date, d.time);
  text(nx, 36, b, BLACK);
  StatusFlags f = computeStatus(d, s);
  snprintf(b, sizeof(b), "JOB %04lu  PAGE 0001", (unsigned long)(d.sys.wakeCount % 10000));
  textRight(PX1, 16, b, BLACK);
  if (f.alerts)
  {
    snprintf(b, sizeof(b), "** %d %s **", f.alerts, lang.label_alerts);
    char u[48];
    upper(u, sizeof(u), b, 30);
    textRight(PX1, 36, u, RED);
  }
  else if (d.stale)
    textRight(PX1, 36, lang.label_stale, RED);
  for (int x = PX0; x < PX1; x += 8)
    text(x, 62, "=", BLACK);

  Col left = {PX0, 0, (EPD_HEIGHT - 32 - BODY_Y) / LINE};
  Col right = {PX0 + COL_W + 16, 0, left.maxLine};
  for (int i = 0; i < DASH_SLOTS; i++)
    section(i < 3 ? left : right, i + 1, s.slots[i], d, s);

  char end[48];
  snprintf(end, sizeof(end), "*** %s ***", lang.label_end_of_report);
  upper(b, sizeof(b), end, 40);
  textCenter(EPD_WIDTH / 2, EPD_HEIGHT - 22, b, BLACK);
}
