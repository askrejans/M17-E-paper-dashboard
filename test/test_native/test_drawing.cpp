#include <unity.h>
#include "display/drawing.h"
#include "font_8x16.h"
#include "fixtures.h"

static int countColor(int x, int y, int w, int h, int c)
{
  int n = 0;
  for (int yy = y; yy < y + h; yy++)
    for (int xx = x; xx < x + w; xx++)
      n += pxGet(xx, yy) == c;
  return n;
}

void test_px_packing(void)
{
  memset(fb, 0, EPD_WIDTH * EPD_HEIGHT / 4);
  px(0, 0, WHITE);
  TEST_ASSERT_EQUAL_HEX8(0x40, fb[0]);
  px(1, 0, YELLOW);
  TEST_ASSERT_EQUAL_HEX8(0x60, fb[0]);
  px(3, 0, RED);
  TEST_ASSERT_EQUAL_HEX8(0x63, fb[0]);
  px(0, 1, RED); // next row starts at byte 200
  TEST_ASSERT_EQUAL_HEX8(0xC0, fb[200]);
  TEST_ASSERT_EQUAL(YELLOW, pxGet(1, 0));
}

void test_px_clipping(void)
{
  fill(WHITE);
  px(-1, 0, BLACK);
  px(0, -1, BLACK);
  px(EPD_WIDTH, 0, BLACK);
  px(0, EPD_HEIGHT, BLACK);
  TEST_ASSERT_EQUAL(0, countColor(0, 0, EPD_WIDTH, 2, BLACK));
  TEST_ASSERT_EQUAL(WHITE, pxGet(-5, -5)); // out of bounds reads as paper
}

void test_fill_all_colours(void)
{
  const int cols[] = {BLACK, WHITE, YELLOW, RED};
  const uint8_t bytes[] = {0x00, 0x55, 0xAA, 0xFF};
  for (int i = 0; i < 4; i++)
  {
    fill(cols[i]);
    TEST_ASSERT_EQUAL_HEX8(bytes[i], fb[0]);
    TEST_ASSERT_EQUAL_HEX8(bytes[i], fb[EPD_WIDTH * EPD_HEIGHT / 4 - 1]);
  }
}

void test_fill_rect_lines_and_rects(void)
{
  fill(WHITE);
  fillRect(10, 10, 5, 4, RED);
  TEST_ASSERT_EQUAL(20, countColor(0, 0, 40, 40, RED));
  hline(0, 30, 50, BLACK);
  vline(60, 0, 25, BLACK);
  TEST_ASSERT_EQUAL(50 + 25, countColor(0, 0, 100, 40, BLACK));
  fill(WHITE);
  rect(0, 0, 10, 10);
  TEST_ASSERT_EQUAL(36, countColor(0, 0, 10, 10, BLACK));
  rectThick(20, 0, 10, 10, 2, RED);
  TEST_ASSERT_EQUAL(36 + 28, countColor(20, 0, 10, 10, RED));
}

void test_dither_patterns(void)
{
  fill(WHITE);
  fillRectDither(0, 0, 8, 8, BLACK, YELLOW);
  TEST_ASSERT_EQUAL(32, countColor(0, 0, 8, 8, BLACK));
  fillRectDitherDense(0, 0, 8, 8, WHITE, YELLOW);
  TEST_ASSERT_EQUAL(16, countColor(0, 0, 8, 8, YELLOW));
  fillRectShadeLight(0, 0, 16, 16, WHITE, RED);
  TEST_ASSERT_EQUAL(16, countColor(0, 0, 16, 16, RED));
  fillRectLevel(0, 0, 16, 16, WHITE, BLACK, 0);
  TEST_ASSERT_EQUAL(0, countColor(0, 0, 16, 16, BLACK));
  fillRectLevel(0, 0, 16, 16, WHITE, BLACK, 256);
  TEST_ASSERT_EQUAL(256, countColor(0, 0, 16, 16, BLACK));
  fillRectLevel(0, 0, 16, 16, WHITE, BLACK, 128);
  TEST_ASSERT_EQUAL(128, countColor(0, 0, 16, 16, BLACK)); // Bayer: exact 50 %
}

void test_gradient_fades_left_to_right(void)
{
  fill(WHITE);
  fillRectGradientH(0, 0, 200, 16, BLACK, YELLOW);
  TEST_ASSERT_EQUAL(0, countColor(0, 0, 100, 16, YELLOW));    // first 60 % solid
  TEST_ASSERT_TRUE(countColor(190, 0, 10, 16, YELLOW) > 130); // ends nearly solid
}

void test_line_and_circle(void)
{
  fill(WHITE);
  line(0, 0, 9, 9, BLACK);
  TEST_ASSERT_EQUAL(10, countColor(0, 0, 10, 10, BLACK));
  line(20, 5, 10, 5, RED); // reversed direction
  TEST_ASSERT_EQUAL(11, countColor(10, 5, 11, 1, RED));
  fill(WHITE);
  circle(50, 50, 10, BLACK);
  TEST_ASSERT_EQUAL(BLACK, pxGet(60, 50));
  TEST_ASSERT_EQUAL(BLACK, pxGet(50, 40));
  TEST_ASSERT_EQUAL(WHITE, pxGet(50, 50));
  fillCircle(100, 100, 5, RED);
  TEST_ASSERT_EQUAL(RED, pxGet(100, 100));
  TEST_ASSERT_EQUAL(WHITE, pxGet(106, 100));
  fillRoundRect(200, 200, 20, 10, 3, YELLOW);
  TEST_ASSERT_EQUAL(WHITE, pxGet(200, 200)); // rounded corner
  TEST_ASSERT_EQUAL(YELLOW, pxGet(210, 205));
}

void test_dotted_lines(void)
{
  fill(WHITE);
  hlineDotted(0, 0, 30, BLACK, 1, 2);
  TEST_ASSERT_EQUAL(10, countColor(0, 0, 30, 1, BLACK));
  vlineDotted(0, 10, 20, RED, 2, 2);
  TEST_ASSERT_EQUAL(10, countColor(0, 10, 1, 20, RED));
}

void test_utf8_decoding(void)
{
  const char *s = "Ā€°—x\xF0\x9F\x98\x80";
  TEST_ASSERT_EQUAL_HEX8(0x80, fontNextChar(&s)); // Ā
  TEST_ASSERT_EQUAL_HEX8(0x96, fontNextChar(&s)); // €
  TEST_ASSERT_EQUAL_HEX8(0x94, fontNextChar(&s)); // °
  TEST_ASSERT_EQUAL_HEX8('-', fontNextChar(&s));  // em dash folded
  TEST_ASSERT_EQUAL_HEX8('x', fontNextChar(&s));
  TEST_ASSERT_EQUAL_HEX8('?', fontNextChar(&s));  // emoji unsupported, consumed whole
  TEST_ASSERT_EQUAL(0, *s);
  const char *raw = "\xAB";                      // raw glyph index (G_* macro)
  TEST_ASSERT_EQUAL_HEX8(0xAB, fontNextChar(&raw));
}

void test_text_rendering(void)
{
  fill(WHITE);
  text(0, 0, "Hi", BLACK);
  TEST_ASSERT_TRUE(countColor(0, 0, 16, 16, BLACK) > 10);
  TEST_ASSERT_EQUAL(0, countColor(16, 0, 16, 16, BLACK));
  TEST_ASSERT_EQUAL(16, textWidth("Hi"));
  TEST_ASSERT_EQUAL(5 * 8, textWidth("Ķēķis"));
  TEST_ASSERT_EQUAL(5 * 8 * 3, textWidth("Ķēķis", 3));
  fill(WHITE);
  textScaled(0, 0, "A", RED, 2);
  int n2 = countColor(0, 0, 16, 32, RED);
  fill(WHITE);
  text(0, 0, "A", RED);
  int n1 = countColor(0, 0, 8, 16, RED);
  TEST_ASSERT_TRUE(n2 >= 3 * n1 && n2 <= 5 * n1); // ~4x area, Scale2x adjusts edges
  fill(WHITE);
  textRight(100, 0, "AB", BLACK);
  TEST_ASSERT_EQUAL(0, countColor(100, 0, 20, 16, BLACK));
  TEST_ASSERT_TRUE(countColor(84, 0, 16, 16, BLACK) > 0);
  fill(WHITE);
  textDim(0, 0, "M", BLACK);
  text(20, 0, "M", BLACK);
  TEST_ASSERT_TRUE(countColor(0, 0, 8, 16, BLACK) < countColor(20, 0, 8, 16, BLACK));
}

void test_seven_segment(void)
{
  fill(WHITE);
  TEST_ASSERT_EQUAL(sevenSegWidth("8", 40) * 2, sevenSegWidth("88", 40));
  TEST_ASSERT_TRUE(sevenSegWidth("1.5", 40) < sevenSegWidth("185", 40));
  sevenSeg(0, 0, "8", 40, BLACK);
  int all = countColor(0, 0, 30, 40, BLACK);
  fill(WHITE);
  sevenSeg(0, 0, "1", 40, BLACK);
  int one = countColor(0, 0, 30, 40, BLACK);
  TEST_ASSERT_TRUE(one > 0 && one < all / 2);
  fill(WHITE);
  sevenSeg(0, 0, "1", 40, BLACK, YELLOW); // ghosts drawn for unlit segments
  TEST_ASSERT_TRUE(countColor(0, 0, 30, 40, YELLOW) > 0);
  TEST_ASSERT_EQUAL(one, countColor(0, 0, 30, 40, BLACK));
  fill(WHITE);
  sevenSeg(0, 0, "1", 20, BLACK, YELLOW); // small digits: no ghosts
  TEST_ASSERT_EQUAL(0, countColor(0, 0, 20, 20, YELLOW));
}

void test_icons(void)
{
  fill(WHITE);
  icon(0, 0, getWeatherIcon("sunny"), BLACK);
  int n = countColor(0, 0, 24, 24, BLACK);
  TEST_ASSERT_TRUE(n > 20);
  fill(WHITE);
  iconScaled(0, 0, getWeatherIcon("sunny"), BLACK, 2);
  TEST_ASSERT_EQUAL(4 * n, countColor(0, 0, 48, 48, BLACK));
  TEST_ASSERT_TRUE(getWeatherIcon("pouring") != getWeatherIcon("rainy")); // every state has its own icon
  TEST_ASSERT_TRUE(getWeatherIcon("unknown-state") == getWeatherIcon("cloudy"));
  const char *states[] = {"sunny", "clear-night", "cloudy", "partlycloudy", "rainy", "pouring", "snowy", "snowy-rainy",
                          "fog", "hail", "windy", "windy-variant", "lightning", "lightning-rainy", "exceptional"};
  for (const char *s : states)
    TEST_ASSERT_NOT_NULL(getWeatherIcon(s));
  for (int i = 0; i < 15; i++) // all distinct
    for (int j = i + 1; j < 15; j++)
      TEST_ASSERT_TRUE(getWeatherIcon(states[i]) != getWeatherIcon(states[j]));
}

void test_bmp_export(void)
{
  fill(WHITE);
  px(0, 0, RED);        // top-left -> last row in a bottom-up BMP
  px(1, EPD_HEIGHT - 1, YELLOW);
  TEST_ASSERT_EQUAL(118 + 400 * 480, bmpSize());
  uint8_t hdr[118];
  TEST_ASSERT_EQUAL(118, bmpRead(hdr, sizeof(hdr), 0));
  TEST_ASSERT_EQUAL('B', hdr[0]);
  TEST_ASSERT_EQUAL('M', hdr[1]);
  TEST_ASSERT_EQUAL(4, hdr[28]); // bpp
  TEST_ASSERT_EQUAL(800, hdr[18] | hdr[19] << 8);
  uint8_t b;
  bmpRead(&b, 1, 118 + 0); // first stored row = bottom row
  TEST_ASSERT_EQUAL_HEX8((WHITE << 4) | YELLOW, b);
  bmpRead(&b, 1, 118 + 400 * 479);
  TEST_ASSERT_EQUAL_HEX8((RED << 4) | WHITE, b);
  uint8_t tail[16];
  TEST_ASSERT_EQUAL(0, bmpRead(tail, sizeof(tail), bmpSize())); // past the end
}
