#include <unity.h>
#include "display/drawing.h"
#include "display/theme.h"
#include "lang.h"
#include "fixtures.h"
#include <initializer_list>

static DashboardData d;
static Settings s;

static uint32_t hashFb()
{
  uint32_t h = 2166136261u;
  for (uint32_t i = 0; i < EPD_WIDTH * EPD_HEIGHT / 4; i++)
    h = (h ^ fb[i]) * 16777619u;
  return h;
}

static long countColor(int c)
{
  long n = 0;
  for (int y = 0; y < EPD_HEIGHT; y++)
    for (int x = 0; x < EPD_WIDTH; x++)
      n += pxGet(x, y) == c;
  return n;
}

void test_themes_render_sample_all_languages(void)
{
  settingsDefaults(s);
  loadPayload(SAMPLE_V2, d);
  for (const char *l : {"EN", "LV"})
  {
    setLanguage(l);
    for (int i = 0; i < THEME_COUNT; i++)
    {
      fill(RED);
      renderDashboard(d, s, THEME_IDS[i]);
      // every theme paints the whole screen and uses at least 3 of 4 inks
      int used = 0;
      for (int c = 0; c < 4; c++)
        used += countColor(c) > 0;
      TEST_ASSERT_TRUE_MESSAGE(used >= 3, THEME_IDS[i]);
      TEST_ASSERT_TRUE_MESSAGE(countColor(RED) < EPD_WIDTH * EPD_HEIGHT / 4, THEME_IDS[i]);
    }
  }
  setLanguage("EN");
}

void test_themes_background_identity(void)
{
  settingsDefaults(s);
  loadPayload(SAMPLE_V2, d);
  renderDashboard(d, s, "vfd");
  TEST_ASSERT_TRUE(countColor(BLACK) > EPD_WIDTH * EPD_HEIGHT / 2); // black glass
  renderDashboard(d, s, "classic");
  TEST_ASSERT_TRUE(countColor(WHITE) > EPD_WIDTH * EPD_HEIGHT / 2); // paper
  renderDashboard(d, s, "terminal");
  TEST_ASSERT_TRUE(countColor(BLACK) > EPD_WIDTH * EPD_HEIGHT / 2);
  renderDashboard(d, s, "printout");
  TEST_ASSERT_TRUE(countColor(WHITE) > EPD_WIDTH * EPD_HEIGHT / 3);
}

void test_themes_deterministic(void)
{
  settingsDefaults(s);
  loadPayload(SAMPLE_V2, d);
  for (int i = 0; i < THEME_COUNT; i++)
  {
    renderDashboard(d, s, THEME_IDS[i]);
    uint32_t a = hashFb();
    renderDashboard(d, s, THEME_IDS[i]);
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(a, hashFb(), THEME_IDS[i]);
  }
}

void test_themes_every_widget_in_every_slot(void)
{
  settingsDefaults(s);
  loadPayload(SAMPLE_V2, d);
  for (int t = 0; t < THEME_COUNT; t++)
    for (int w = 0; w < WIDGET_COUNT; w++)
    {
      for (int i = 0; i < DASH_SLOTS; i++)
        strlcpy(s.slots[i], WIDGET_IDS[w], sizeof(s.slots[i]));
      renderDashboard(d, s, THEME_IDS[t]); // must not crash / overrun
    }
  TEST_PASS();
}

void test_themes_empty_and_v1_data(void)
{
  settingsDefaults(s);
  dataReset(d);
  for (int i = 0; i < THEME_COUNT; i++)
    renderDashboard(d, s, THEME_IDS[i]);
  loadPayload(SAMPLE_V1, d);
  for (int i = 0; i < THEME_COUNT; i++)
    renderDashboard(d, s, THEME_IDS[i]);
  TEST_PASS();
}

void test_theme_slot_mapping_changes_output(void)
{
  settingsDefaults(s);
  loadPayload(SAMPLE_V2, d);
  renderDashboard(d, s, "vfd");
  uint32_t a = hashFb();
  strlcpy(s.slots[0], "hourly", sizeof(s.slots[0]));
  renderDashboard(d, s, "vfd");
  TEST_ASSERT_NOT_EQUAL(a, hashFb());
}

void test_theme_unknown_falls_back_to_classic(void)
{
  settingsDefaults(s);
  loadPayload(SAMPLE_V2, d);
  renderDashboard(d, s, "classic");
  uint32_t a = hashFb();
  renderDashboard(d, s, "does-not-exist");
  TEST_ASSERT_EQUAL_HEX32(a, hashFb());
}

void test_status_screens(void)
{
  renderSetupScreen("M17-Setup-ABCD", "http://192.168.4.1");
  TEST_ASSERT_TRUE(countColor(RED) > 100);
  renderMessageScreen("Title", "line 1", "line 2", "http://x");
  TEST_ASSERT_TRUE(countColor(BLACK) > 1000);
}

void test_themes_banded_render_matches_full(void)
{
  // The firmware renders 60-row bands into a small buffer; the result must be
  // byte-identical to a full-screen render for every theme.
  settingsDefaults(s);
  loadPayload(SAMPLE_V2, d);
  const size_t full = EPD_WIDTH * EPD_HEIGHT / 4, rowBytes = EPD_WIDTH / 4;
  unsigned char *ref = (unsigned char *)malloc(full);
  unsigned char *band = (unsigned char *)malloc(60 * rowBytes);
  unsigned char *screen = fb;
  for (int t = 0; t < THEME_COUNT; t++)
  {
    fbSetBand(0, EPD_HEIGHT);
    renderDashboard(d, s, THEME_IDS[t]);
    memcpy(ref, screen, full);
    fb = band;
    for (int y0 = 0; y0 < EPD_HEIGHT; y0 += 60)
    {
      fbSetBand(y0, 60);
      renderDashboard(d, s, THEME_IDS[t]);
      TEST_ASSERT_EQUAL_MEMORY_MESSAGE(ref + y0 * rowBytes, band, 60 * rowBytes, THEME_IDS[t]);
    }
    fb = screen;
  }
  fbSetBand(0, EPD_HEIGHT);
  free(ref);
  free(band);
}
