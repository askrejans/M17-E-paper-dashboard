#include <unity.h>
#include "font_8x16.h"
#include "font_8x16_basic.h"
#include <string.h>
#include <stdio.h>

struct Lv
{
  uint8_t slot;
  char base;
  enum { ABOVE, BELOW } accent;
};

// Every Latvian letter with its base letter and accent placement.
static const Lv LV_LETTERS[] = {
    {0x80, 'A', Lv::ABOVE}, {0x81, 'a', Lv::ABOVE}, {0x82, 'C', Lv::ABOVE}, {0x83, 'c', Lv::ABOVE},
    {0x84, 'E', Lv::ABOVE}, {0x85, 'e', Lv::ABOVE}, {0x86, 'G', Lv::BELOW}, {0x87, 'g', Lv::ABOVE},
    {0x88, 'I', Lv::ABOVE}, {0x89, 'i', Lv::ABOVE}, {0x8A, 'L', Lv::BELOW}, {0x8B, 'l', Lv::BELOW},
    {0x8C, 'N', Lv::BELOW}, {0x8D, 'n', Lv::BELOW}, {0x8E, 'S', Lv::ABOVE}, {0x8F, 's', Lv::ABOVE},
    {0x90, 'U', Lv::ABOVE}, {0x91, 'u', Lv::ABOVE}, {0x92, 'Z', Lv::ABOVE}, {0x93, 'z', Lv::ABOVE},
    {0x95, 'K', Lv::BELOW}, {0x97, 'k', Lv::BELOW},
};

static int firstRow(const uint8_t *g)
{
  for (int r = 0; r < 16; r++)
    if (g[r])
      return r;
  return -1;
}

static int lastRow(const uint8_t *g)
{
  for (int r = 15; r >= 0; r--)
    if (g[r])
      return r;
  return -1;
}

void test_font_ascii_preserved(void)
{
  font8x16_init();
  for (int c = 0; c < 128; c++)
    TEST_ASSERT_EQUAL_MEMORY(font8x16_basic[c], font8x16[c], 16);
}

void test_font_latvian_glyphs_present(void)
{
  for (const Lv &l : LV_LETTERS)
  {
    TEST_ASSERT_TRUE(firstRow(font8x16[l.slot]) >= 0);
    TEST_ASSERT_TRUE_MESSAGE(memcmp(font8x16[l.slot], font8x16[(uint8_t)l.base], 16) != 0, "differs from base");
  }
}

void test_font_latvian_metrics(void)
{
  // Accents never collide with the letter body's baseline/cap metrics:
  //  - capitals with accents above keep the base glyph's bottom row (baseline)
  //    and put the accent in rows 0-1
  //  - lowercase accents above sit in rows 1-3, body identical to the base
  //  - letters with a cedilla keep the full base body (rows 0-11) unchanged
  for (const Lv &l : LV_LETTERS)
  {
    const uint8_t *g = font8x16[l.slot], *b = font8x16[(uint8_t)l.base];
    char msg[16];
    snprintf(msg, sizeof(msg), "slot %02X", l.slot);
    bool upper = l.base >= 'A' && l.base <= 'Z';
    if (l.accent == Lv::BELOW)
    {
      TEST_ASSERT_EQUAL_MEMORY_MESSAGE(b, g, 12, msg);
      TEST_ASSERT_TRUE_MESSAGE(g[12] || g[13], msg);
    }
    else if (upper)
    {
      TEST_ASSERT_EQUAL_MESSAGE(lastRow(b), lastRow(g), msg);       // same baseline
      TEST_ASSERT_TRUE_MESSAGE(firstRow(g) <= 1, msg);               // accent at top
      TEST_ASSERT_TRUE_MESSAGE(lastRow(g) - 2 >= 8, msg);            // body >= 9 rows tall
    }
    else
    {
      TEST_ASSERT_EQUAL_MEMORY_MESSAGE(b + 5, g + 5, 11, msg);       // x-height body identical
      TEST_ASSERT_TRUE_MESSAGE(firstRow(g) >= 1 && firstRow(g) <= 3, msg);
    }
  }
}

void test_font_macron_capitals_full_height(void)
{
  // Ā Ē Ī Ū keep the complete base letter (rows 2-11)
  const uint8_t slots[] = {0x80, 0x84, 0x88, 0x90};
  const char bases[] = {'A', 'E', 'I', 'U'};
  for (int i = 0; i < 4; i++)
    TEST_ASSERT_EQUAL_MEMORY(font8x16[(uint8_t)bases[i]] + 2, font8x16[slots[i]] + 2, 10);
}

void test_font_symbols(void)
{
  const uint8_t syms[] = {0x94, 0x96, 0x98, 0x9C, 0x9D, 0xA0, 0xA1, 0xA5, 0xA8, 0xAB, 0xAC, 0xB5, 0xB6, 0xB7, 0xB8};
  for (uint8_t s : syms)
    TEST_ASSERT_TRUE(firstRow(font8x16[s]) >= 0);
  for (int r = 0; r < 16; r++)
    TEST_ASSERT_EQUAL_HEX8(0xFF, font8x16[0xB6][r]); // full block
}
