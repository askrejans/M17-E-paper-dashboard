#include "display/drawing.h"
#include "display/epd_driver.h"
#include "display/epd_spi.h"
#include "font_8x16.h"
#include "data.h"
#include "utils/utils.h"
#include "icons.h"

#include "lang.h"
#include <time.h>
#include <string.h>


// Helper: Get weekday abbreviation for forecast days using lang translations and base date from JSON
const char* getForecastWeekdayAbbr(int offset)
{
  // Parse base date from data.date (format: YYYY-MM-DD or DD.MM.YYYY)
  struct tm tm_base;
  memset(&tm_base, 0, sizeof(tm_base));
  int year = 0, month = 0, day = 0;
  if (strchr(data.date, '-')) {
    // Format: YYYY-MM-DD
    sscanf(data.date, "%d-%d-%d", &year, &month, &day);
  } else {
    // Format: DD.MM.YYYY
    sscanf(data.date, "%d.%d.%d", &day, &month, &year);
  }
  tm_base.tm_year = year - 1900;
  tm_base.tm_mon = month - 1;
  tm_base.tm_mday = day;

  // Add offset days
  time_t t = mktime(&tm_base) + offset * 86400;
  struct tm tm_result;
  localtime_r(&t, &tm_result);
  int wday = tm_result.tm_wday; // 0=Sunday, 1=Monday, ...

  // Use lang.weekdays translation array
  if (lang.weekdays) {
    // Latvian: 0=Sv, 1=P, ...; English: 0=Sun, 1=Mon, ...
    return lang.weekdays[wday];
  }
  // Fallback
  static const char* fallback_days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  return fallback_days[wday];
}

struct Utf8Map
{
  uint16_t utf8;
  uint8_t glyph;
};

static const Utf8Map utf8_map[] = {
    // Ā ā
    {0xC480, 0x80}, // Ā
    {0xC481, 0x81}, // ā

    // Č č
    {0xC48C, 0x82}, // Č
    {0xC48D, 0x83}, // č

    // Ē ē
    {0xC492, 0x84}, // Ē
    {0xC493, 0x85}, // ē

    // Ģ ģ
    {0xC4A2, 0x86}, // Ģ
    {0xC4A3, 0x87}, // ģ

    // Ī ī
    {0xC4AA, 0x88}, // Ī
    {0xC4AB, 0x89}, // ī

    // Ķ ķ
    {0xC4B6, 0x95}, // Ķ
    {0xC4B7, 0x97}, // ķ

    // Ļ ļ
    {0xC4BB, 0x8A}, // Ļ
    {0xC4BC, 0x8B}, // ļ

    // Ņ ņ
    {0xC585, 0x8C}, // Ņ
    {0xC586, 0x8D}, // ņ

    // Š š
    {0xC5A0, 0x8E}, // Š
    {0xC5A1, 0x8F}, // š

    // Ū ū
    {0xC5AA, 0x90}, // Ū
    {0xC5AB, 0x91}, // ū

    // Ž ž
    {0xC5BD, 0x92}, // Ž
    {0xC5BE, 0x93}, // ž

    {0xC2B0, 0x94}, // °

    // Arrows (2-byte UTF-8: C2xx range won't work for these, they're 3-byte)
    // These are accessed directly via glyph index in code
};

static inline uint8_t next_char(const char **s)
{
  uint8_t c0 = (uint8_t)(*s)[0];

  // ASCII
  if (c0 < 0x80)
  {
    (*s)++;
    return c0;
  }

  // 2-byte UTF-8
  if ((c0 & 0xE0) == 0xC0)
  {
    uint16_t u = (c0 << 8) | (uint8_t)(*s)[1];
    (*s) += 2;

    for (unsigned i = 0; i < sizeof(utf8_map) / sizeof(utf8_map[0]); i++)
      if (utf8_map[i].utf8 == u)
        return utf8_map[i].glyph;

    return '?';
  }

  // 3-byte UTF-8
  if ((c0 & 0xF0) == 0xE0)
  {
    uint32_t u =
        ((uint32_t)c0 << 16) |
        ((uint32_t)(uint8_t)(*s)[1] << 8) |
        (uint32_t)(uint8_t)(*s)[2];

    (*s) += 3;

    switch (u) {
      case 0xE282AC: return 0x96; // €
      case 0xE296AA: return 0x98; // ▪ U+25AA
      case 0xE296B8: return 0x99; // ▸ U+25B8
      case 0xE28691: return 0x9C; // ↑ U+2191
      case 0xE28693: return 0x9D; // ↓ U+2193
      case 0xE28692: return 0x9E; // → U+2192
      case 0xE28690: return 0x9F; // ← U+2190
      case 0xE2978F: return 0xA0; // ● U+25CF
      case 0xE2978B: return 0xA1; // ○ U+25CB
      case 0xE296A0: return 0xA2; // ■ U+25A0
      case 0xE296B2: return 0xA3; // ▲ U+25B2
      case 0xE296BC: return 0xA4; // ▼ U+25BC
      case 0xE29C93: return 0xA5; // ✓ U+2713
      case 0xE29C97: return 0xA6; // ✗ U+2717
      case 0xE29786: return 0xA7; // ◆ U+25C6
      case 0xE29AA1: return 0xA8; // ⚡ U+26A1
    }

    return '?';
  }

  // Direct glyph index (raw byte, not a valid UTF-8 start)
  // Passes through to font table lookup - renders blank if slot is empty
  (*s)++;
  return c0;
}

void px(int x, int y, int c)
{
  if (x < 0 || y < 0 || x >= 800 || y >= 480)
    return;
  uint32_t i = y * 800 + x;
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
  // 75% c1 / 25% c2 using 2x2 ordered pattern
  for (int yy = y; yy < y + h; yy++)
    for (int xx = x; xx < x + w; xx++) {
      int mx = xx & 1, my = yy & 1;
      px(xx, yy, (mx == 0 && my == 0) ? c2 : c1);
    }
}

// Very light shade: ~6% c2 density (1 pixel per 4x4 block)
// Suitable for alternating row backgrounds where text must remain readable
void fillRectShadeLight(int x, int y, int w, int h, int c1, int c2)
{
  for (int yy = y; yy < y + h; yy++)
    for (int xx = x; xx < x + w; xx++) {
      // Single pixel at position (0,0) in each 4x4 block = 1/16 = 6.25%
      px(xx, yy, ((xx & 3) == 0 && (yy & 3) == 0) ? c2 : c1);
    }
}

// 16x16 Bayer ordered dither matrix (values 0-255)
// 257 gradation levels — maximum smoothness for 2-color gradients.
// Generated by recursive subdivision: bayer16[y][x] = 4*bayer8[y%8][x%8] + bayer2[y/8][x/8]
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
  // Maximum-smoothness horizontal gradient using 16x16 Bayer ordered dithering.
  // Solid c_left for first 60%, then 256-level fade to c_right.
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
  for (int i = x; i < x + w; i++)
  {
    px(i, y, BLACK);
    px(i, y + h - 1, BLACK);
  }
  for (int i = y; i < y + h; i++)
  {
    px(x, i, BLACK);
    px(x + w - 1, i, BLACK);
  }
}

void rectColored(int x, int y, int w, int h, int c)
{
  for (int i = x; i < x + w; i++)
  {
    px(i, y, c);
    px(i, y + h - 1, c);
  }
  for (int i = y; i < y + h; i++)
  {
    px(x, i, c);
    px(x + w - 1, i, c);
  }
}

void rectWithTitle(int x, int y, int w, int h, const char *title)
{
  const int title_h = 24;

  // outer border
  rect(x, y, w, h);

  // title bar: horizontal gradient BLACK -> YELLOW (left to right, F1 chequered fade)
  fillRectGradientH(x + 1, y + 1, w - 2, title_h - 2, BLACK, YELLOW);

  // thin bottom accent
  hline(x + 1, y + title_h - 1, w - 2, YELLOW);

  // title text (left-aligned in the black zone)
  text(x + 8, y + 4, title, YELLOW);
}

void text(int x, int y, const char *s, int c)
{
  while (*s)
  {
    uint8_t ch = next_char(&s);
    const uint8_t *g = font8x16[ch];

    for (int r = 0; r < 16; r++)
    {
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
  // Draw 24x24 icon from PROGMEM
  for (int row = 0; row < 24; row++)
  {
    for (int col = 0; col < 24; col++)
    {
      int byteIdx = row * 3 + (col / 8);
      int bitIdx = 7 - (col % 8);
      uint8_t byte = pgm_read_byte(&iconData[byteIdx]);
      
      if (byte & (1 << bitIdx))
        px(x + col, y + row, c);
    }
  }
}

const uint8_t* getWeatherIcon(const char *state)
{
  // Map weather state strings to icons (exact matches from Home Assistant)
  if (strcmp(state, "sunny") == 0)
    return ICON_SUNNY;
  if (strcmp(state, "clear-night") == 0)
    return ICON_CLEAR_NIGHT;
  if (strcmp(state, "cloudy") == 0)
    return ICON_CLOUDY;
  if (strcmp(state, "partlycloudy") == 0)
    return ICON_PARTLYCLOUDY;
  if (strcmp(state, "rainy") == 0)
    return ICON_RAINY;
  if (strcmp(state, "pouring") == 0)
    return ICON_RAINY;
  if (strcmp(state, "snowy") == 0)
    return ICON_SNOWY;
  if (strcmp(state, "snowy-rainy") == 0)
    return ICON_SNOWY;
  if (strcmp(state, "fog") == 0)
    return ICON_FOG;
  if (strcmp(state, "hail") == 0)
    return ICON_HAIL;
  if (strcmp(state, "windy") == 0)
    return ICON_WINDY;
  if (strcmp(state, "windy-variant") == 0)
    return ICON_WINDY_VARIANT;
  if (strcmp(state, "lightning") == 0)
    return ICON_THUNDERSTORM;
  if (strcmp(state, "lightning-rainy") == 0)
    return ICON_THUNDERSTORM;
  if (strcmp(state, "exceptional") == 0)
    return ICON_WINDY;
  
  // Default to cloudy if unknown
  return ICON_CLOUDY;
}



// Glyph shorthand for inline use (these map to ext_glyphs font slots)
#define G_BULLET   "\x98"   // ▪
#define G_ARROW_R  "\x99"   // ▸
#define G_UP       "\x9C"   // ↑
#define G_DOWN     "\x9D"   // ↓
#define G_RIGHT    "\x9E"   // →
#define G_FILLED   "\xA0"   // ●
#define G_EMPTY    "\xA1"   // ○
#define G_CHECK    "\xA5"   // ✓
#define G_CROSS    "\xA6"   // ✗
#define G_DIAMOND  "\xA7"   // ◆
#define G_BOLT     "\xA8"   // ⚡
#define G_THERMO   "\xAB"   // 🌡
#define G_HOUSE    "\xAC"   // 🏠
#define G_SIGNAL   "\xAD"   // 📶
#define G_DROP     "\xAA"   // 💧
#define G_WIND     "\xAE"   // ≋
#define G_CLOUD    "\xAF"   // ☁
#define G_PRESS    "\xB0"   // ◎
#define G_FREQ     "\xB1"   // ∿
#define G_VOLT     "\xB2"   // ⏚
#define G_METER    "\xB3"   // ☰
#define G_PHI      "\xB4"   // cosφ
#define G_PRICE    "\xB5"   // ₿

// 16x16 house icon for header (2 bytes per row = 16 pixels wide)
static const uint8_t ICON_HOUSE_16[32] = {
  0x01, 0x80,  // row 0:        ##
  0x03, 0xC0,  // row 1:       ####
  0x07, 0xE0,  // row 2:      ######
  0x0F, 0xF0,  // row 3:     ########
  0x1F, 0xF8,  // row 4:    ##########
  0x3F, 0xFC,  // row 5:   ############
  0x7F, 0xFE,  // row 6:  ##############
  0xFF, 0xFF,  // row 7: ################
  0x3F, 0xFC,  // row 8:   ############
  0x3F, 0xFC,  // row 9:   ############
  0x33, 0xCC,  // row 10:  ##  ####  ##
  0x33, 0xCC,  // row 11:  ##  ####  ##
  0x33, 0xCC,  // row 12:  ##  ####  ##
  0x30, 0x0C,  // row 13:  ##        ##
  0x30, 0x0C,  // row 14:  ##        ##
  0x3F, 0xFC,  // row 15:  ############
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

void draw_dashboard()
{
  fill(WHITE);

  // --- Column layout: 3 equal-width columns ---
  const int COL_GAP = 8;
  const int MARGIN = 10;
  const int COL_W = (800 - 2 * MARGIN - 2 * COL_GAP) / 3; // 254
  const int COL1_X = MARGIN;                                 // 10
  const int COL2_X = COL1_X + COL_W + COL_GAP;              // 272
  const int COL3_X = COL2_X + COL_W + COL_GAP;              // 534
  const int CONTENT_PAD = 10; // padding inside boxes

  // ============================================================
  // HEADER - horizontal gradient BLACK -> YELLOW (F1 chequered fade)
  // ============================================================
  const int HDR_H = 40;

  // Main header bar: horizontal gradient
  fillRectGradientH(0, 0, 800, 32, BLACK, YELLOW);
  // Bottom accent: thin yellow line fading to white
  hline(0, 32, 800, YELLOW);
  fillRectDither(0, 33, 800, 2, YELLOW, WHITE);

  // Left: House name with 16x16 house icon (in BLACK zone - white text)
  icon16(12, 8, ICON_HOUSE_16, WHITE);
  text(32, 8, lang.house_name, WHITE);

  // Center: Weekday + Date (in mid zone)
  const char* today = getForecastWeekdayAbbr(0);
  char date_buf[40];
  sprintf(date_buf, "%s, %s", today, data.date);
  int date_width = strlen(date_buf) * 8;
  int date_x = (800 - date_width) / 2;
  text(date_x, 8, date_buf, WHITE);

  // Right: Refresh time (in YELLOW zone - black text for contrast)
  char refresh_text[32];
  sprintf(refresh_text, "%s %s", lang.header_updated, data.time);
  int refresh_width = strlen(refresh_text) * 8;
  int refresh_x = 800 - refresh_width - 16;
  text(refresh_x, 8, refresh_text, BLACK);

  // ============================================================
  // BOX LAYOUT
  // ============================================================
  const int ROW1_Y = HDR_H + 6;  // 54
  const int ROW1_H = 200;
  const int ROW2_Y = ROW1_Y + ROW1_H + 8; // 262
  const int ROW2_H = 200;

  char b[96];
  int y, lx;

  // ============================================================
  // BOX 1: OUTDOOR (upper left)
  // ============================================================
  rectWithTitle(COL1_X, ROW1_Y, COL_W, ROW1_H, lang.title_outdoor);

  y = ROW1_Y + 34;

  /* Weather icon and state - centered at top */
  const uint8_t* weatherIcon = getWeatherIcon(data.weatherState);
  const char* weatherLV = translateWeatherState(data.weatherState);

  int weatherTextWidth = strlen(weatherLV) * 8;
  int totalWidth = 24 + 8 + weatherTextWidth;
  int centerX = COL1_X + (COL_W - totalWidth) / 2;

  icon(centerX, y, weatherIcon, BLACK);
  text(centerX + 24 + 8, y + 4, weatherLV, BLACK);

  y += 28;
  lx = COL1_X + CONTENT_PAD;

  // Thin separator after weather state
  hline(COL1_X + 4, y, COL_W - 8, YELLOW);
  y += 6;

  sprintf(b, G_THERMO " %.1f\xC2\xB0""C", data.outTemp);
  text(lx, y, b, (data.outTemp < -10 || data.outTemp > 30) ? RED : BLACK);
  // humidity on right side of same row
  sprintf(b, G_DROP " %.0f%%", data.outHum);
  text(lx + 130, y, b, BLACK);
  y += 18;

  sprintf(b, G_PRESS " %.0f hPa", data.outPress);
  text(lx, y, b, BLACK);
  y += 18;

  sprintf(b, G_WIND " %s %.1f km/h", lang.label_wind, data.outWind);
  text(lx, y, b, (data.outWind > 20) ? RED : BLACK);
  y += 18;

  sprintf(b, G_WIND " %s %.1f km/h", lang.label_gusts, data.outGust);
  text(lx, y, b, (data.outGust > 25) ? RED : BLACK);
  y += 18;

  sprintf(b, G_CLOUD " %.0f%%", data.clouds);
  text(lx, y, b, BLACK);

  // ============================================================
  // BOX 2: INDOOR (upper center)
  // ============================================================
  rectWithTitle(COL2_X, ROW1_Y, COL_W, ROW1_H, lang.title_indoor);

  y = ROW1_Y + 34;
  lx = COL2_X + CONTENT_PAD;

  // Fixed-column layout: room name at lx, temp at temp_x, humidity at hum_x
  int temp_x = COL2_X + 140;  // fixed column for temperature
  int hum_x  = COL2_X + 200;  // fixed column for humidity

  // Column headers: icon next to column label
  text(lx, y, "Room", BLACK);
  text(temp_x, y, G_THERMO " Temp", BLACK);
  text(hum_x, y, G_DROP " Hum", BLACK);
  y += 18;

  // Helper macro: render room row with fixed columns, icons next to values
  #define ROOM_ROW(room_name, t, h_val, alert) do { \
    text(lx, y, room_name, alert ? RED : BLACK); \
    sprintf(b, G_THERMO "%.1f\xC2\xB0", t); \
    text(temp_x, y, b, alert ? RED : BLACK); \
    sprintf(b, G_DROP "%.0f%%", h_val); \
    text(hum_x, y, b, alert ? RED : BLACK); \
    y += 17; hline(COL2_X + 8, y, COL_W - 16, YELLOW); y += 3; \
  } while(0)

  ROOM_ROW(lang.room_living, data.lrT, data.lrH, badTemp(data.lrT) || badHum(data.lrH));
  ROOM_ROW(lang.room_bedroom, data.brT, data.brH, badTemp(data.brT) || badHum(data.brH));
  ROOM_ROW(lang.room_kids, data.krT, data.krH, badTemp(data.krT) || badHum(data.krH));
  ROOM_ROW(lang.room_bathroom, data.baT, data.baH, badHum(data.baH));

  // Last row without separator
  text(lx, y, lang.room_office, (badTemp(data.ofT) || badHum(data.ofH)) ? RED : BLACK);
  sprintf(b, G_THERMO "%.1f\xC2\xB0", data.ofT);
  text(temp_x, y, b, (badTemp(data.ofT) || badHum(data.ofH)) ? RED : BLACK);
  sprintf(b, G_DROP "%.0f%%", data.ofH);
  text(hum_x, y, b, (badTemp(data.ofT) || badHum(data.ofH)) ? RED : BLACK);

  #undef ROOM_ROW

  // ============================================================
  // BOX 3: FORECAST (upper right)
  // ============================================================
  rectWithTitle(COL3_X, ROW1_Y, COL_W, ROW1_H, lang.title_forecast);

  int fy = ROW1_Y + 34;
  for (int i = 0; i < 5; i++)
  {
    const char* weekday = getForecastWeekdayAbbr(i + 1);
    text(COL3_X + CONTENT_PAD, fy + 4, weekday, BLACK);

    const uint8_t* fIcon = getWeatherIcon(data.forecast[i].condition);
    icon(COL3_X + 40, fy, fIcon, BLACK);

    sprintf(b, "%.0f-%.0f\xC2\xB0""C %.0fmm",
            data.forecast[i].tempLow,
            data.forecast[i].tempHigh,
            data.forecast[i].precipitation);
    text(COL3_X + 70, fy + 4, b, BLACK);

    fy += 24;
    if (i < 4)
      hline(COL3_X + 8, fy - 1, COL_W - 16, YELLOW);
  }

  // ============================================================
  // BOX 4: LOCATION (lower left)
  // ============================================================
  rectWithTitle(COL1_X, ROW2_Y, COL_W, ROW2_H, lang.title_location);

  lx = COL1_X + CONTENT_PAD;
  y = ROW2_Y + 36;

  for (int i = 0; i < 4; i++)
  {
    const char *st = translateFamilyState(data.family[i].state);
    bool home = strcmp(data.family[i].state, "home") == 0;

    sprintf(b, "%s %s: %s", home ? G_FILLED : G_EMPTY,
            data.family[i].name, st);
    text(lx, y + 2, b, home ? BLACK : RED);
    y += 22;
    if (i < 3)
      hline(COL1_X + 8, y - 1, COL_W - 16, YELLOW);
  }

  // ============================================================
  // BOX 5: NETWORK (lower center)
  // ============================================================
  rectWithTitle(COL2_X, ROW2_Y, COL_W, ROW2_H, lang.title_network);

  lx = COL2_X + CONTENT_PAD;
  y = ROW2_Y + 36;

  sprintf(b, G_DOWN " %s %.2f Mbps", lang.label_download, data.down);
  text(lx, y, b, (data.down < 10) ? RED : BLACK);
  y += 20; hline(COL2_X + 8, y, COL_W - 16, YELLOW); y += 4;

  sprintf(b, G_UP " %s   %.2f Mbps", lang.label_upload, data.up);
  text(lx, y, b, (data.up < 5) ? RED : BLACK);
  y += 20; hline(COL2_X + 8, y, COL_W - 16, YELLOW); y += 4;

  sprintf(b, G_SIGNAL " %s %.0f ms", lang.label_ping, data.ping);
  text(lx, y, b, (data.ping > 50) ? RED : BLACK);

  // ============================================================
  // BOX 6: ELECTRICITY (lower right)
  // ============================================================
  rectWithTitle(COL3_X, ROW2_Y, COL_W, ROW2_H, lang.title_electricity);

  lx = COL3_X + CONTENT_PAD;
  int py = ROW2_Y + 36;

  sprintf(b, G_PRICE " %.2f \xE2\x82\xAC/MWh", data.powerPrice);
  text(lx, py, b, (data.powerPrice > 20) ? RED : BLACK);
  py += 17; hline(COL3_X + 8, py, COL_W - 16, YELLOW); py += 3;

  sprintf(b, G_BOLT " %.0f W", data.powerTotal);
  text(lx, py, b, (data.powerTotal > 5000) ? RED : BLACK);
  py += 17; hline(COL3_X + 8, py, COL_W - 16, YELLOW); py += 3;

  sprintf(b, G_VOLT " %.0f V", data.powerVoltage);
  text(lx, py, b, (data.powerVoltage < 220 || data.powerVoltage > 240) ? RED : BLACK);
  py += 17; hline(COL3_X + 8, py, COL_W - 16, YELLOW); py += 3;

  sprintf(b, G_FREQ " %.2f Hz", data.powerFrequency);
  text(lx, py, b, (data.powerFrequency < 48 || data.powerFrequency > 52) ? RED : BLACK);
  py += 17; hline(COL3_X + 8, py, COL_W - 16, YELLOW); py += 3;

  sprintf(b, G_PHI " PF %.2f", data.powerFactor);
  text(lx, py, b, (data.powerFactor < 70) ? RED : BLACK);
  py += 17; hline(COL3_X + 8, py, COL_W - 16, YELLOW); py += 3;

  sprintf(b, G_METER " %.1f kWh", data.powerMonth);
  text(lx, py, b, (data.powerMonth > 320) ? RED : BLACK);

  // ============================================================
  // FOOTER - gradient accent matching header style
  // ============================================================
  fillRectGradientH(0, 466, 800, 4, YELLOW, BLACK);
  fillRectDitherDense(0, 470, 800, 2, BLACK, WHITE);

  EPD_init();
  PIC_display(fb);
  EPD_sleep();
}