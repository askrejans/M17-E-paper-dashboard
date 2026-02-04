#include "display/drawing.h"
#include "display/epd_display.h"
#include "display/epd_spi.h"
#include "font_8x16.h"
#include "data.h"
#include "utils/utils.h"
#include "icons.h"
#include "lang.h"

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

  // 3-byte UTF-8 (EURO)
  if ((c0 & 0xF0) == 0xE0)
  {
    uint32_t u =
        ((uint32_t)c0 << 16) |
        ((uint32_t)(*s)[1] << 8) |
        (uint32_t)(*s)[2];

    (*s) += 3;

    // € = E2 82 AC
    if (u == 0xE282AC)
      return 0x96; // € glyph

    return '?';
  }

  // unsupported
  (*s)++;
  return '?';
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

void rectWithTitle(int x, int y, int w, int h, const char *title)
{
  const int title_h = 24;

  // outer border
  rect(x, y, w, h);

  // title bar
  fillRect(x + 1, y + 1, w - 2, title_h, BLACK);

  // title text (vertically centered in bar)
  text(x + 8, y + 5, title, YELLOW);
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

void draw_dashboard()
{
  fill(WHITE);

  fillRect(0, 0, 800, 40, BLACK);

  // Left: House name
  text(20, 12, lang.house_name, WHITE);
  
  // Center: Date
  int date_width = strlen(data.date) * 8;
  int date_x = (800 - date_width) / 2;
  text(date_x, 12, data.date, YELLOW);
  
  // Right: Refresh time
  char refresh_text[32];
  sprintf(refresh_text, "%s %s", lang.header_updated, data.time);
  int refresh_width = strlen(refresh_text) * 8;
  int refresh_x = 800 - refresh_width - 20;
  text(refresh_x, 12, refresh_text, YELLOW);

  char b[96];

  rectWithTitle(10, 50, 260, 200, lang.title_outdoor);

  int y = 80;

  /* Weather icon and state - centered at top */
  const uint8_t* weatherIcon = getWeatherIcon(data.weatherState);
  const char* weatherLV = translateWeatherState(data.weatherState);
  
  // Center the icon and text
  int weatherTextWidth = strlen(weatherLV) * 8;
  int totalWidth = 24 + 8 + weatherTextWidth;
  int centerX = 10 + (260 - totalWidth) / 2;
  
  icon(centerX, y, weatherIcon, BLACK);
  text(centerX + 24 + 8, y + 4, weatherLV, BLACK);
  
  y += 30;

  /* Temperatūra */
  sprintf(b, "%s  %.1f°C", lang.label_temperature, data.outTemp);
  text(20, y, b, (data.outTemp < -10 || data.outTemp > 30) ? RED : BLACK);
  y += 18;

  /* Mitrums */
  sprintf(b, "%s       %.0f%%", lang.label_humidity, data.outHum);
  text(20, y, b, BLACK);
  y += 18;

  /* Gaisa spiediens */
  sprintf(b, "%s  %.0f hPa", lang.label_pressure, data.outPress);
  text(20, y, b, BLACK);
  y += 18;

  /* Vēja stiprums */
  sprintf(b, "%s          %.1f km/h", lang.label_wind, data.outWind);
  text(20, y, b, (data.outWind > 20) ? RED : BLACK);
  y += 18;

  /* Brāzmās */
  sprintf(b, "%s       %.1f km/h", lang.label_gusts, data.outGust);
  text(20, y, b, (data.outGust > 25) ? RED : BLACK);
  y += 18;

  /* Mākoņu daudzums */
  sprintf(b, "%s        %.0f %%", lang.label_clouds, data.clouds);
  text(20, y, b, BLACK);

  /* 5-Day Forecast box in upper right */
  rectWithTitle(550, 50, 230, 200, lang.title_forecast);
  
  int fy = 80;
  for (int i = 0; i < 5; i++)
  {
    sprintf(b, "D+%d", i + 1);
    text(560, fy + 4, b, BLACK);
    
    const uint8_t* fIcon = getWeatherIcon(data.forecast[i].condition);
    icon(590, fy, fIcon, BLACK);
    
    sprintf(b, "%.0f-%.0f°C %.0fmm", 
            data.forecast[i].tempLow, 
            data.forecast[i].tempHigh,
            data.forecast[i].precipitation);
    text(620, fy + 4, b, BLACK);
    
    fy += 24;
  }

  rectWithTitle(280, 50, 260, 200, lang.title_indoor);

  y = 80;

  /* Header */
  text(290, y, lang.label_temp_hum_header, BLACK);
  y += 18;
  /* Dzīvojamā istaba */
  sprintf(b, "%s    %.1f°C  %.0f%%", lang.room_living, data.lrT, data.lrH);
  text(290, y, b, (badTemp(data.lrT) || badHum(data.lrH)) ? RED : BLACK);
  y += 18;

  /* Guļamistaba */
  sprintf(b, "%s   %.1f°C  %.0f%%", lang.room_bedroom, data.brT, data.brH);
  text(290, y, b, (badTemp(data.brT) || badHum(data.brH)) ? RED : BLACK);
  y += 18;

  /* Bērnu istaba */
  sprintf(b, "%s  %.1f°C  %.0f%%", lang.room_kids, data.krT, data.krH);
  text(290, y, b, (badTemp(data.krT) || badHum(data.krH)) ? RED : BLACK);
  y += 18;

  /* Vannas istaba */
  sprintf(b, "%s  %.1f°C  %.0f%%", lang.room_bathroom, data.baT, data.baH);
  text(290, y, b, badHum(data.baH) ? RED : BLACK);
  y += 18;

  /* Ofiss */
  sprintf(b, "%s         %.1f°C  %.0f%%", lang.room_office, data.ofT, data.ofH);
  text(290, y, b, (badTemp(data.ofT) || badHum(data.ofH)) ? RED : BLACK);

  rectWithTitle(10, 270, 260, 180, lang.title_location);

  for (int i = 0; i < 4; i++)
  {
    const char *st = translateFamilyState(data.family[i].state);

    sprintf(b, "%s: %s", data.family[i].name, st);
    text(20, 300 + i * 20, b,
         strcmp(data.family[i].state, "home") == 0 ? BLACK : RED);
  }

  rectWithTitle(280, 270, 260, 180, lang.title_network);

  sprintf(b, "%s %.2f Mbps", lang.label_download, data.down);
  text(290, 300, b, (data.down < 10) ? RED : BLACK);
  sprintf(b, "%s   %.2f Mbps", lang.label_upload, data.up);
  text(290, 320, b, (data.up < 5) ? RED : BLACK);
  sprintf(b, "%s %.0f ms", lang.label_ping, data.ping);
  text(290, 340, b, (data.ping > 50) ? RED : BLACK);

  rectWithTitle(550, 270, 230, 180, lang.title_electricity);

  int py = 300;

  sprintf(b, "%s     %.2f €/MWh", lang.label_price, data.powerPrice);
  text(570, py, b, (data.powerPrice > 20) ? RED : BLACK);
  py += 18;

  sprintf(b, "%s    %.0f W", lang.label_power, data.powerTotal);
  text(570, py, b, (data.powerTotal > 5000) ? RED : BLACK);
  py += 18;

  sprintf(b, "%s  %.0f V", lang.label_voltage, data.powerVoltage);
  text(570, py, b, (data.powerVoltage < 220 || data.powerVoltage > 240) ? RED : BLACK);
  py += 18;

  sprintf(b, "%s   %.2f Hz", lang.label_frequency, data.powerFrequency);
  text(570, py, b, (data.powerFrequency < 48 || data.powerFrequency > 52) ? RED : BLACK);
  py += 18;

  sprintf(b, "%s       %.2f", lang.label_power_factor, data.powerFactor);
  text(570, py, b, (data.powerFactor < 70) ? RED : BLACK);
  py += 18;

  sprintf(b, "%s %.1f kWh", lang.label_consumption, data.powerMonth);
  text(570, py, b, (data.powerMonth > 320) ? RED : BLACK);

  EPD_init();
  PIC_display(fb);
  EPD_sleep();
}