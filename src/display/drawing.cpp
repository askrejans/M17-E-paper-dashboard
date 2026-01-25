#include "display/drawing.h"
#include "display/epd_display.h"
#include "display/epd_spi.h"
#include "font_8x16.h"
#include "data.h"
#include "utils/utils.h"

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

void draw_dashboard()
{
  fill(WHITE);

  fillRect(0, 0, 800, 40, BLACK);

  char header[48];
  sprintf(header, "     %s               atjaunots @ %s", data.date, data.time);
  int header_x = (800 - strlen(header) * 8) / 2;

  text(20, 12, "M17", WHITE);
  text(header_x, 12, header, YELLOW);

  char b[64];

  rectWithTitle(10, 50, 260, 200, "Ārā");

  int y = 80;

  /* Temperatūra */
  sprintf(b, "Temperatūra  %.1f°C", data.outTemp);
  text(20, y, b, (data.outTemp < -10 || data.outTemp > 30) ? RED : BLACK);
  y += 18;

  /* Mitrums */
  sprintf(b, "Mitrums       %.0f%%", data.outHum);
  text(20, y, b, BLACK);
  y += 18;

  /* Gaisa spiediens */
  sprintf(b, "Gaisa spied.  %.0f hPa", data.outPress);
  text(20, y, b, BLACK);
  y += 18;

  /* Vēja stiprums */
  sprintf(b, "Vējš          %.1f km/h", data.outWind);
  text(20, y, b, (data.outWind > 20) ? RED : BLACK);
  y += 18;

  /* Brāzmās */
  sprintf(b, "Brāzmās       %.1f km/h", data.outGust);
  text(20, y, b, (data.outGust > 25) ? RED : BLACK);
  y += 18;

  /* Mākoņu daudzums */
  sprintf(b, "Mākoņi        %.0f %%", data.clouds);
  text(20, y, b, BLACK);
  y += 18;

  /* Stāvoklis */
  sprintf(b, "Apstākļi      %s", data.weatherState);
  text(20, y, b, BLACK);

  rectWithTitle(280, 50, 260, 200, "Telpās");

  y = 80;

  /* Header */
  text(290, y, "              Temp    Mitr", BLACK);
  y += 18;
  /* Dzīvojamā istaba */
  sprintf(b, "Viesistaba    %.1f°C  %.0f%%", data.lrT, data.lrH);
  text(290, y, b, (badTemp(data.lrT) || badHum(data.lrH)) ? RED : BLACK);
  y += 18;

  /* Guļamistaba */
  sprintf(b, "Guļamistaba   %.1f°C  %.0f%%", data.brT, data.brH);
  text(290, y, b, (badTemp(data.brT) || badHum(data.brH)) ? RED : BLACK);
  y += 18;

  /* Bērnu istaba */
  sprintf(b, "Bērnu istaba  %.1f°C  %.0f%%", data.krT, data.krH);
  text(290, y, b, (badTemp(data.krT) || badHum(data.krH)) ? RED : BLACK);
  y += 18;

  /* Vannas istaba */
  sprintf(b, "Vannasistaba  %.1f°C  %.0f%%", data.baT, data.baH);
  text(290, y, b, badHum(data.baH) ? RED : BLACK);
  y += 18;

  /* Ofiss */
  sprintf(b, "Ofiss         %.1f°C  %.0f%%", data.ofT, data.ofH);
  text(290, y, b, (badTemp(data.ofT) || badHum(data.ofH)) ? RED : BLACK);

  rectWithTitle(10, 270, 260, 180, "Atrašanās vieta");

  for (int i = 0; i < 4; i++)
  {
    const char *st = familyStateLV(data.family[i].state);

    sprintf(b, "%s: %s", data.family[i].name, st);
    text(20, 300 + i * 20, b,
         strcmp(data.family[i].state, "home") == 0 ? BLACK : RED);
  }

  rectWithTitle(280, 270, 260, 180, "Tīkls");

  sprintf(b, "Down %.2f Mbps", data.down);
  text(290, 300, b, (data.down < 10) ? RED : BLACK);
  sprintf(b, "Up   %.2f Mbps", data.up);
  text(290, 320, b, (data.up < 5) ? RED : BLACK);
  sprintf(b, "Ping %.0f ms", data.ping);
  text(290, 340, b, (data.ping > 50) ? RED : BLACK);

  rectWithTitle(550, 270, 230, 180, "Elektrība");

  int py = 300;

  sprintf(b, "Cena     %.2f €/MWh", data.powerPrice);
  text(570, py, b, (data.powerPrice > 20) ? RED : BLACK);
  py += 18;

  sprintf(b, "Jauda    %.0f W", data.powerTotal);
  text(570, py, b, (data.powerTotal > 5000) ? RED : BLACK);
  py += 18;

  sprintf(b, "Sprieg.  %.0f V", data.powerVoltage);
  text(570, py, b, (data.powerVoltage < 220 || data.powerVoltage > 240) ? RED : BLACK);
  py += 18;

  sprintf(b, "Frekv.   %.2f Hz", data.powerFrequency);
  text(570, py, b, (data.powerFrequency < 48 || data.powerFrequency > 52) ? RED : BLACK);
  py += 18;

  sprintf(b, "PF       %.2f", data.powerFactor);
  text(570, py, b, (data.powerFactor < 70) ? RED : BLACK);
  py += 18;

  sprintf(b, "Patēriņš %.1f kWh", data.powerMonth);
  text(570, py, b, (data.powerMonth > 320) ? RED : BLACK);

  EPD_init();
  PIC_display(fb);
  EPD_sleep();
}