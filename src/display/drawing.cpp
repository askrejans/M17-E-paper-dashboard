#include "display/drawing.h"
#include "display/epd_driver.h"
#include "font_8x16.h"
#include "data.h"
#include "config.h"
#include "utils/utils.h"
#include "icons.h"
#include "lang.h"
#include <time.h>
#include <string.h>

// ── Layout constants ───────────────────────────────────────────
static const int MARGIN      = 10;
static const int COL_GAP     = 8;
static const int COL_W       = (EPD_WIDTH - 2 * MARGIN - 2 * COL_GAP) / 3;
static const int COL1_X      = MARGIN;
static const int COL2_X      = COL1_X + COL_W + COL_GAP;
static const int COL3_X      = COL2_X + COL_W + COL_GAP;
static const int CONTENT_PAD = 10;
static const int HDR_H       = 40;
static const int TITLE_H     = 22;
static const int ROW1_Y      = HDR_H + 6;
static const int ROW1_H      = 190;
static const int ROW2_Y      = ROW1_Y + ROW1_H + 6;
static const int ROW2_H      = 190;


// Helper: Get weekday abbreviation for forecast days using lang translations and base date from JSON
const char* getForecastWeekdayAbbr(int offset)
{
  struct tm tm_base;
  memset(&tm_base, 0, sizeof(tm_base));
  int year = 0, month = 0, day = 0;
  if (strchr(data.date, '-')) {
    sscanf(data.date, "%d-%d-%d", &year, &month, &day);
  } else {
    sscanf(data.date, "%d.%d.%d", &day, &month, &year);
  }
  tm_base.tm_year = year - 1900;
  tm_base.tm_mon = month - 1;
  tm_base.tm_mday = day;

  time_t t = mktime(&tm_base) + offset * 86400;
  struct tm tm_result;
  localtime_r(&t, &tm_result);
  int wday = tm_result.tm_wday;

  if (lang.weekdays)
    return lang.weekdays[wday];

  static const char* fallback_days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  return fallback_days[wday];
}

// ── UTF-8 decoding ─────────────────────────────────────────────

struct Utf8Map
{
  uint16_t utf8;
  uint8_t glyph;
};

static const Utf8Map utf8_map[] = {
    {0xC480, 0x80}, // Ā
    {0xC481, 0x81}, // ā
    {0xC48C, 0x82}, // Č
    {0xC48D, 0x83}, // č
    {0xC492, 0x84}, // Ē
    {0xC493, 0x85}, // ē
    {0xC4A2, 0x86}, // Ģ
    {0xC4A3, 0x87}, // ģ
    {0xC4AA, 0x88}, // Ī
    {0xC4AB, 0x89}, // ī
    {0xC4B6, 0x95}, // Ķ
    {0xC4B7, 0x97}, // ķ
    {0xC4BB, 0x8A}, // Ļ
    {0xC4BC, 0x8B}, // ļ
    {0xC585, 0x8C}, // Ņ
    {0xC586, 0x8D}, // ņ
    {0xC5A0, 0x8E}, // Š
    {0xC5A1, 0x8F}, // š
    {0xC5AA, 0x90}, // Ū
    {0xC5AB, 0x91}, // ū
    {0xC5BD, 0x92}, // Ž
    {0xC5BE, 0x93}, // ž
    {0xC2B0, 0x94}, // °
};

static inline uint8_t next_char(const char **s)
{
  uint8_t c0 = (uint8_t)(*s)[0];

  if (c0 < 0x80) {
    (*s)++;
    return c0;
  }

  if ((c0 & 0xE0) == 0xC0) {
    uint16_t u = (c0 << 8) | (uint8_t)(*s)[1];
    (*s) += 2;
    for (unsigned i = 0; i < sizeof(utf8_map) / sizeof(utf8_map[0]); i++)
      if (utf8_map[i].utf8 == u)
        return utf8_map[i].glyph;
    return '?';
  }

  if ((c0 & 0xF0) == 0xE0) {
    uint32_t u =
        ((uint32_t)c0 << 16) |
        ((uint32_t)(uint8_t)(*s)[1] << 8) |
        (uint32_t)(uint8_t)(*s)[2];
    (*s) += 3;

    switch (u) {
      case 0xE282AC: return 0x96; // €
      case 0xE296AA: return 0x98; // ▪
      case 0xE296B8: return 0x99; // ▸
      case 0xE28691: return 0x9C; // ↑
      case 0xE28693: return 0x9D; // ↓
      case 0xE28692: return 0x9E; // →
      case 0xE28690: return 0x9F; // ←
      case 0xE2978F: return 0xA0; // ●
      case 0xE2978B: return 0xA1; // ○
      case 0xE296A0: return 0xA2; // ■
      case 0xE296B2: return 0xA3; // ▲
      case 0xE296BC: return 0xA4; // ▼
      case 0xE29C93: return 0xA5; // ✓
      case 0xE29C97: return 0xA6; // ✗
      case 0xE29786: return 0xA7; // ◆
      case 0xE29AA1: return 0xA8; // ⚡
    }
    return '?';
  }

  (*s)++;
  return c0;
}

// ── Drawing primitives ─────────────────────────────────────────

void px(int x, int y, int c)
{
  if (x < 0 || y < 0 || x >= EPD_WIDTH || y >= EPD_HEIGHT)
    return;
  uint32_t i = y * EPD_WIDTH + x;
  uint32_t b = i >> 2;
  uint8_t s = (3 - (i & 3)) * 2;
  fb[b] = (fb[b] & ~(3 << s)) | (c << s);
}

void fill(int c)
{
  uint8_t v = (c == WHITE) ? 0x55 : 0x00;
  memset(fb, v, ALLSCREEN_BYTES);
}

void fillRect(int x, int y, int w, int h, int c)
{
  for (int yy = y; yy < y + h; yy++)
    for (int xx = x; xx < x + w; xx++)
      px(xx, yy, c);
}

void fillRectDither(int x, int y, int w, int h, int c1, int c2)
{
  for (int yy = y; yy < y + h; yy++)
    for (int xx = x; xx < x + w; xx++)
      px(xx, yy, ((xx + yy) & 1) ? c1 : c2);
}

void fillRectDitherDense(int x, int y, int w, int h, int c1, int c2)
{
  for (int yy = y; yy < y + h; yy++)
    for (int xx = x; xx < x + w; xx++) {
      int mx = xx & 1, my = yy & 1;
      px(xx, yy, (mx == 0 && my == 0) ? c2 : c1);
    }
}

void fillRectShadeLight(int x, int y, int w, int h, int c1, int c2)
{
  for (int yy = y; yy < y + h; yy++)
    for (int xx = x; xx < x + w; xx++)
      px(xx, yy, ((xx & 3) == 0 && (yy & 3) == 0) ? c2 : c1);
}

// 16x16 Bayer ordered dither matrix (values 0-255)
static const uint8_t bayer16[16][16] PROGMEM = {
  {  0,128, 32,160,  8,136, 40,168,  2,130, 34,162, 10,138, 42,170},
  {192, 64,224, 96,200, 72,232,104,194, 66,226, 98,202, 74,234,106},
  { 48,176, 16,144, 56,184, 24,152, 50,178, 18,146, 58,186, 26,154},
  {240,112,208, 80,248,120,216, 88,242,114,210, 82,250,122,218, 90},
  { 12,140, 44,172,  4,132, 36,164, 14,142, 46,174,  6,134, 38,166},
  {204, 76,236,108,196, 68,228,100,206, 78,238,110,198, 70,230,102},
  { 60,188, 28,156, 52,180, 20,148, 62,190, 30,158, 54,182, 22,150},
  {252,124,220, 92,244,116,212, 84,254,126,222, 94,246,118,214, 86},
  {  3,131, 35,163, 11,139, 43,171,  1,129, 33,161,  9,137, 41,169},
  {195, 67,227, 99,203, 75,235,107,193, 65,225, 97,201, 73,233,105},
  { 51,179, 19,147, 59,187, 27,155, 49,177, 17,145, 57,185, 25,153},
  {243,115,211, 83,251,123,219, 91,241,113,209, 81,249,121,217, 89},
  { 15,143, 47,175,  7,135, 39,167, 13,141, 45,173,  5,133, 37,165},
  {207, 79,239,111,199, 71,231,103,205, 77,237,109,197, 69,229,101},
  { 63,191, 31,159, 55,183, 23,151, 61,189, 29,157, 53,181, 21,149},
  {255,127,223, 95,247,119,215, 87,253,125,221, 93,245,117,213, 85}
};

void fillRectGradientH(int x, int y, int w, int h, int c_left, int c_right)
{
  for (int yy = y; yy < y + h; yy++) {
    for (int xx = x; xx < x + w; xx++) {
      int dx = xx - x;
      int ratio;
      int start = w * 60 / 100;
      if (dx <= start) {
        ratio = 0;
      } else {
        ratio = (dx - start) * 256 / (w - start);
        if (ratio > 256) ratio = 256;
      }
      int threshold = pgm_read_byte(&bayer16[yy & 15][xx & 15]);
      px(xx, yy, (ratio > threshold) ? c_right : c_left);
    }
  }
}

// ── Line & box primitives ──────────────────────────────────────

void hline(int x, int y, int w, int c)
{
  for (int xx = x; xx < x + w; xx++)
    px(xx, y, c);
}

void vline(int x, int y, int h, int c)
{
  for (int yy = y; yy < y + h; yy++)
    px(x, yy, c);
}

void rect(int x, int y, int w, int h)
{
  for (int i = x; i < x + w; i++) {
    px(i, y, BLACK);
    px(i, y + h - 1, BLACK);
  }
  for (int i = y; i < y + h; i++) {
    px(x, i, BLACK);
    px(x + w - 1, i, BLACK);
  }
}

void rectColored(int x, int y, int w, int h, int c)
{
  for (int i = x; i < x + w; i++) {
    px(i, y, c);
    px(i, y + h - 1, c);
  }
  for (int i = y; i < y + h; i++) {
    px(x, i, c);
    px(x + w - 1, i, c);
  }
}

void rectWithTitle(int x, int y, int w, int h, const char *title)
{
  rect(x, y, w, h);

  // Flat solid title bar
  fillRect(x + 1, y + 1, w - 2, TITLE_H, BLACK);

  // Yellow accent line under title
  hline(x + 1, y + TITLE_H + 1, w - 2, YELLOW);

  // Title text (white on black)
  text(x + 8, y + 3, title, WHITE);
}

// ── Text & icon rendering ──────────────────────────────────────

void text(int x, int y, const char *s, int c)
{
  while (*s) {
    uint8_t ch = next_char(&s);
    const uint8_t *g = font8x16[ch];
    for (int r = 0; r < 16; r++) {
      uint8_t b = g[r];
      for (int i = 0; i < 8; i++)
        if (b & (0x80 >> i))
          px(x + i, y + r, c);
    }
    x += 8;
  }
}

void icon(int x, int y, const uint8_t *iconData, int c)
{
  for (int row = 0; row < 24; row++)
    for (int col = 0; col < 24; col++) {
      int byteIdx = row * 3 + (col / 8);
      int bitIdx = 7 - (col % 8);
      if (pgm_read_byte(&iconData[byteIdx]) & (1 << bitIdx))
        px(x + col, y + row, c);
    }
}

const uint8_t* getWeatherIcon(const char *state)
{
  if (strcmp(state, "sunny") == 0)       return ICON_SUNNY;
  if (strcmp(state, "clear-night") == 0) return ICON_CLEAR_NIGHT;
  if (strcmp(state, "cloudy") == 0)      return ICON_CLOUDY;
  if (strcmp(state, "partlycloudy") == 0) return ICON_PARTLYCLOUDY;
  if (strcmp(state, "rainy") == 0)       return ICON_RAINY;
  if (strcmp(state, "pouring") == 0)     return ICON_RAINY;
  if (strcmp(state, "snowy") == 0)       return ICON_SNOWY;
  if (strcmp(state, "snowy-rainy") == 0) return ICON_SNOWY;
  if (strcmp(state, "fog") == 0)         return ICON_FOG;
  if (strcmp(state, "hail") == 0)        return ICON_HAIL;
  if (strcmp(state, "windy") == 0)       return ICON_WINDY;
  if (strcmp(state, "windy-variant") == 0) return ICON_WINDY_VARIANT;
  if (strcmp(state, "lightning") == 0)    return ICON_THUNDERSTORM;
  if (strcmp(state, "lightning-rainy") == 0) return ICON_THUNDERSTORM;
  if (strcmp(state, "exceptional") == 0) return ICON_WINDY;
  return ICON_CLOUDY;
}

// ── Glyph shorthand macros ─────────────────────────────────────

#define G_BULLET   "\x98"
#define G_ARROW_R  "\x99"
#define G_UP       "\x9C"
#define G_DOWN     "\x9D"
#define G_RIGHT    "\x9E"
#define G_FILLED   "\xA0"
#define G_EMPTY    "\xA1"
#define G_CHECK    "\xA5"
#define G_CROSS    "\xA6"
#define G_DIAMOND  "\xA7"
#define G_BOLT     "\xA8"
#define G_THERMO   "\xAB"
#define G_HOUSE    "\xAC"
#define G_SIGNAL   "\xAD"
#define G_DROP     "\xAA"
#define G_WIND     "\xAE"
#define G_CLOUD    "\xAF"
#define G_PRESS    "\xB0"
#define G_FREQ     "\xB1"
#define G_VOLT     "\xB2"
#define G_METER    "\xB3"
#define G_PHI      "\xB4"
#define G_PRICE    "\xB5"

// 16x16 house icon for header
static const uint8_t ICON_HOUSE_16[32] = {
  0x01, 0x80,
  0x03, 0xC0,
  0x07, 0xE0,
  0x0F, 0xF0,
  0x1F, 0xF8,
  0x3F, 0xFC,
  0x7F, 0xFE,
  0xFF, 0xFF,
  0x3F, 0xFC,
  0x3F, 0xFC,
  0x33, 0xCC,
  0x33, 0xCC,
  0x33, 0xCC,
  0x30, 0x0C,
  0x30, 0x0C,
  0x3F, 0xFC,
};

static void icon16(int x, int y, const uint8_t *data16, int c)
{
  for (int r = 0; r < 16; r++) {
    uint8_t hi = data16[r * 2];
    uint8_t lo = data16[r * 2 + 1];
    for (int i = 0; i < 8; i++)
      if (hi & (0x80 >> i)) px(x + i, y + r, c);
    for (int i = 0; i < 8; i++)
      if (lo & (0x80 >> i)) px(x + 8 + i, y + r, c);
  }
}

// ── Alert color helper ─────────────────────────────────────────

static int alertColor(bool alert)
{
  return alert ? RED : BLACK;
}

// ── Dashboard section drawers ──────────────────────────────────

static void drawHeader()
{
  // Gradient header bar
  fillRectGradientH(0, 0, EPD_WIDTH, 32, BLACK, YELLOW);

  // Yellow accent + dithered fade below
  hline(0, 32, EPD_WIDTH, YELLOW);
  fillRectDither(0, 33, EPD_WIDTH, 2, YELLOW, WHITE);

  // Left: House icon + name
  icon16(12, 8, ICON_HOUSE_16, WHITE);
  text(32, 8, lang.house_name, WHITE);

  // Center: Weekday + Date
  const char* today = getForecastWeekdayAbbr(0);
  char date_buf[40];
  snprintf(date_buf, sizeof(date_buf), "%s, %s", today, data.date);
  int date_width = strlen(date_buf) * 8;
  text((EPD_WIDTH - date_width) / 2, 8, date_buf, WHITE);

  // Right: Refresh time (in yellow zone)
  char refresh_text[32];
  snprintf(refresh_text, sizeof(refresh_text), "%s %s", lang.header_updated, data.time);
  int refresh_width = strlen(refresh_text) * 8;
  text(EPD_WIDTH - refresh_width - 16, 8, refresh_text, BLACK);
}

static void drawOutdoorBox()
{
  rectWithTitle(COL1_X, ROW1_Y, COL_W, ROW1_H, lang.title_outdoor);

  int y = ROW1_Y + TITLE_H + 10;
  int lx = COL1_X + CONTENT_PAD;
  char b[96];

  // Weather icon + state centered
  const uint8_t* weatherIcon = getWeatherIcon(data.weatherState);
  const char* weatherLabel = translateWeatherState(data.weatherState);
  int weatherTextW = strlen(weatherLabel) * 8;
  int totalW = 24 + 8 + weatherTextW;
  int cx = COL1_X + (COL_W - totalW) / 2;
  icon(cx, y, weatherIcon, BLACK);
  text(cx + 32, y + 4, weatherLabel, BLACK);

  y += 28;
  hline(COL1_X + 4, y, COL_W - 8, BLACK);
  y += 6;

  // Temperature + humidity on same row
  bool tempAlert = data.outTemp < ALERT_OUTDOOR_TEMP_LOW || data.outTemp > ALERT_OUTDOOR_TEMP_HIGH;
  snprintf(b, sizeof(b), G_THERMO " %.1f\xC2\xB0""C", data.outTemp);
  text(lx, y, b, alertColor(tempAlert));
  snprintf(b, sizeof(b), G_DROP " %.0f%%", data.outHum);
  text(lx + 130, y, b, BLACK);
  y += 18;

  snprintf(b, sizeof(b), G_PRESS " %.0f hPa", data.outPress);
  text(lx, y, b, BLACK);
  y += 18;

  snprintf(b, sizeof(b), G_WIND " %s %.1f km/h", lang.label_wind, data.outWind);
  text(lx, y, b, alertColor(data.outWind > ALERT_WIND_HIGH));
  y += 18;

  snprintf(b, sizeof(b), G_WIND " %s %.1f km/h", lang.label_gusts, data.outGust);
  text(lx, y, b, alertColor(data.outGust > ALERT_GUST_HIGH));
  y += 18;

  snprintf(b, sizeof(b), G_CLOUD " %.0f%%", data.clouds);
  text(lx, y, b, BLACK);
}

static void drawIndoorBox()
{
  rectWithTitle(COL2_X, ROW1_Y, COL_W, ROW1_H, lang.title_indoor);

  int y = ROW1_Y + TITLE_H + 10;
  int lx = COL2_X + CONTENT_PAD;
  int temp_x = COL2_X + 140;
  int hum_x  = COL2_X + 200;
  char b[96];

  // Column headers
  text(temp_x, y, G_THERMO " Temp", BLACK);
  text(hum_x, y, G_DROP " Hum", BLACK);
  y += 18;

  // Room data with alternating shading
  struct { const char* name; float t; float h; bool skipTempAlert; } rooms[] = {
    { lang.room_living,   data.lrT, data.lrH, false },
    { lang.room_bedroom,  data.brT, data.brH, false },
    { lang.room_kids,     data.krT, data.krH, false },
    { lang.room_bathroom, data.baT, data.baH, true  },
    { lang.room_office,   data.ofT, data.ofH, false },
  };

  for (int i = 0; i < 5; i++) {
    // Alternating light shade for readability
    if (i % 2 == 1)
      fillRectShadeLight(COL2_X + 2, y - 1, COL_W - 4, 17, WHITE, YELLOW);

    bool alert = (rooms[i].skipTempAlert ? false : badTemp(rooms[i].t)) || badHum(rooms[i].h);
    int c = alertColor(alert);

    text(lx, y, rooms[i].name, c);
    snprintf(b, sizeof(b), "%.1f\xC2\xB0", rooms[i].t);
    text(temp_x, y, b, c);
    snprintf(b, sizeof(b), "%.0f%%", rooms[i].h);
    text(hum_x, y, b, c);

    y += 17;
    if (i < 4)
      hline(COL2_X + 8, y, COL_W - 16, BLACK);
    y += 3;
  }
}

static void drawForecastBox()
{
  rectWithTitle(COL3_X, ROW1_Y, COL_W, ROW1_H, lang.title_forecast);

  int fy = ROW1_Y + TITLE_H + 10;
  char b[96];

  for (int i = 0; i < MAX_FORECAST; i++) {
    const char* weekday = getForecastWeekdayAbbr(i + 1);
    text(COL3_X + CONTENT_PAD, fy + 4, weekday, BLACK);

    const uint8_t* fIcon = getWeatherIcon(data.forecast[i].condition);
    icon(COL3_X + 40, fy, fIcon, BLACK);

    snprintf(b, sizeof(b), "%.0f-%.0f\xC2\xB0""C %.0fmm",
             data.forecast[i].tempLow,
             data.forecast[i].tempHigh,
             data.forecast[i].precipitation);
    text(COL3_X + 70, fy + 4, b, BLACK);

    fy += 24;
    if (i < MAX_FORECAST - 1)
      hline(COL3_X + 8, fy - 1, COL_W - 16, BLACK);
  }
}

static void drawLocationBox()
{
  rectWithTitle(COL1_X, ROW2_Y, COL_W, ROW2_H, lang.title_location);

  int lx = COL1_X + CONTENT_PAD;
  int y = ROW2_Y + TITLE_H + 12;
  char b[96];

  for (int i = 0; i < MAX_FAMILY; i++) {
    const char *st = translateFamilyState(data.family[i].state);
    bool home = strcmp(data.family[i].state, "home") == 0;

    snprintf(b, sizeof(b), "%s %s: %s", home ? G_FILLED : G_EMPTY,
             data.family[i].name, st);
    text(lx, y + 2, b, home ? BLACK : RED);
    y += 22;
    if (i < MAX_FAMILY - 1)
      hline(COL1_X + 8, y - 1, COL_W - 16, BLACK);
  }
}

static void drawNetworkBox()
{
  rectWithTitle(COL2_X, ROW2_Y, COL_W, ROW2_H, lang.title_network);

  int lx = COL2_X + CONTENT_PAD;
  int y = ROW2_Y + TITLE_H + 12;
  char b[96];

  snprintf(b, sizeof(b), G_DOWN " %s %.2f Mbps", lang.label_download, data.down);
  text(lx, y, b, alertColor(data.down < ALERT_DOWN_LOW));
  y += 20;
  hline(COL2_X + 8, y, COL_W - 16, BLACK);
  y += 4;

  snprintf(b, sizeof(b), G_UP " %s   %.2f Mbps", lang.label_upload, data.up);
  text(lx, y, b, alertColor(data.up < ALERT_UP_LOW));
  y += 20;
  hline(COL2_X + 8, y, COL_W - 16, BLACK);
  y += 4;

  snprintf(b, sizeof(b), G_SIGNAL " %s %.0f ms", lang.label_ping, data.ping);
  text(lx, y, b, alertColor(data.ping > ALERT_PING_HIGH));
}

static void drawElectricityBox()
{
  rectWithTitle(COL3_X, ROW2_Y, COL_W, ROW2_H, lang.title_electricity);

  int lx = COL3_X + CONTENT_PAD;
  int py = ROW2_Y + TITLE_H + 12;
  char b[96];

  snprintf(b, sizeof(b), G_PRICE " %.2f \xE2\x82\xAC/MWh", data.powerPrice);
  text(lx, py, b, alertColor(data.powerPrice > ALERT_PRICE_HIGH));
  py += 17;
  hline(COL3_X + 8, py, COL_W - 16, BLACK);
  py += 3;

  snprintf(b, sizeof(b), G_BOLT " %.0f W", data.powerTotal);
  text(lx, py, b, alertColor(data.powerTotal > ALERT_POWER_HIGH));
  py += 17;
  hline(COL3_X + 8, py, COL_W - 16, BLACK);
  py += 3;

  snprintf(b, sizeof(b), G_VOLT " %.0f V", data.powerVoltage);
  text(lx, py, b, alertColor(data.powerVoltage < ALERT_VOLTAGE_LOW || data.powerVoltage > ALERT_VOLTAGE_HIGH));
  py += 17;
  hline(COL3_X + 8, py, COL_W - 16, BLACK);
  py += 3;

  snprintf(b, sizeof(b), G_FREQ " %.2f Hz", data.powerFrequency);
  text(lx, py, b, alertColor(data.powerFrequency < ALERT_FREQ_LOW || data.powerFrequency > ALERT_FREQ_HIGH));
  py += 17;
  hline(COL3_X + 8, py, COL_W - 16, BLACK);
  py += 3;

  snprintf(b, sizeof(b), G_PHI " PF %.2f", data.powerFactor);
  text(lx, py, b, alertColor(data.powerFactor < ALERT_PF_LOW));
  py += 17;
  hline(COL3_X + 8, py, COL_W - 16, BLACK);
  py += 3;

  snprintf(b, sizeof(b), G_METER " %.1f kWh", data.powerMonth);
  text(lx, py, b, alertColor(data.powerMonth > ALERT_MONTHLY_HIGH));
}

static void drawSystemBar()
{
  int sy = ROW2_Y + ROW2_H + 4;
  char b[96];

  // Subtle dithered background strip
  fillRectDitherDense(0, sy, EPD_WIDTH, 18, WHITE, YELLOW);

  // Compact system info: IP · RSSI · Ch · Heap · PSRAM · Wake · Up
  int x = MARGIN + 2;
  int ty = sy + 1;

  snprintf(b, sizeof(b), "%s", data.ip);
  text(x, ty, b, BLACK);
  x += strlen(b) * 8 + 8;

  text(x, ty, "|", BLACK); x += 12;

  snprintf(b, sizeof(b), "%ddBm", data.wifiRssi);
  text(x, ty, b, alertColor(data.wifiRssi < ALERT_RSSI_LOW));
  x += strlen(b) * 8 + 8;

  text(x, ty, "|", BLACK); x += 12;

  snprintf(b, sizeof(b), "Ch%u", data.wifiChannel);
  text(x, ty, b, BLACK);
  x += strlen(b) * 8 + 8;

  text(x, ty, "|", BLACK); x += 12;

  snprintf(b, sizeof(b), "Heap %luK", (unsigned long)(data.heapFree / 1024));
  text(x, ty, b, BLACK);
  x += strlen(b) * 8 + 8;

  text(x, ty, "|", BLACK); x += 12;

  snprintf(b, sizeof(b), "PSRAM %luK", (unsigned long)(data.psramFree / 1024));
  text(x, ty, b, BLACK);
  x += strlen(b) * 8 + 8;

  text(x, ty, "|", BLACK); x += 12;

  snprintf(b, sizeof(b), "#%lu", (unsigned long)data.wakeCount);
  text(x, ty, b, BLACK);
  x += strlen(b) * 8 + 8;

  text(x, ty, "|", BLACK); x += 12;

  snprintf(b, sizeof(b), "%lus", (unsigned long)data.uptime);
  text(x, ty, b, BLACK);

  // Footer accents
  hline(0, sy + 19, EPD_WIDTH, YELLOW);
  hline(0, sy + 21, EPD_WIDTH, BLACK);
}

// ── Main dashboard entry point ─────────────────────────────────

void draw_dashboard()
{
  fill(WHITE);

  drawHeader();
  drawOutdoorBox();
  drawIndoorBox();
  drawForecastBox();
  drawLocationBox();
  drawNetworkBox();
  drawElectricityBox();
  drawSystemBar();

  EPD_init();
  PIC_display(fb);
  EPD_sleep();
}
