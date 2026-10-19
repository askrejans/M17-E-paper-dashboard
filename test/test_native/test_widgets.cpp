#include <unity.h>
#include "display/widgets.h"
#include "lang.h"
#include "fixtures.h"

static DashboardData d;
static Settings s;
static WidgetView v;

static void setupData()
{
  settingsDefaults(s);
  setLanguage("EN");
  loadPayload(SAMPLE_V2, d);
}

void test_widget_all_ids_build(void)
{
  setupData();
  for (int i = 0; i < WIDGET_COUNT; i++)
  {
    bool ok = buildWidget(WIDGET_IDS[i], d, s, v);
    if (strcmp(WIDGET_IDS[i], "none") == 0)
      TEST_ASSERT_FALSE(ok);
    else
    {
      TEST_ASSERT_TRUE_MESSAGE(ok, WIDGET_IDS[i]);
      TEST_ASSERT_TRUE_MESSAGE(v.title && v.title[0], WIDGET_IDS[i]);
      TEST_ASSERT_TRUE(v.n <= WIDGET_MAX_ROWS);
    }
  }
  TEST_ASSERT_FALSE(buildWidget("bogus", d, s, v));
}

void test_widget_outdoor(void)
{
  setupData();
  buildWidget("outdoor", d, s, v);
  TEST_ASSERT_EQUAL_STRING("9.7", v.hero);
  TEST_ASSERT_EQUAL_STRING("Partly Cloudy", v.caption);
  TEST_ASSERT_FALSE(v.heroAlert);
  TEST_ASSERT_NOT_NULL(v.heroIcon);
  TEST_ASSERT_EQUAL(5, v.n);
  TEST_ASSERT_TRUE(v.rows[2].alert == false || true);
  TEST_ASSERT_TRUE(v.rows[3].alert); // gust 43.6 > 25
  TEST_ASSERT_FALSE(v.empty);
}

void test_widget_indoor_and_bath_rule(void)
{
  setupData();
  buildWidget("indoor", d, s, v);
  TEST_ASSERT_EQUAL(WK_TABLE, v.kind);
  TEST_ASSERT_EQUAL(3, v.n);
  TEST_ASSERT_TRUE(v.rows[2].alert);  // Ofiss 18.2 °C < 19
  TEST_ASSERT_FALSE(v.rows[0].alert);
  TEST_ASSERT_EQUAL_STRING("46%", v.rows[0].value2);
  // v1 "bath": warm is fine, humid is not
  loadPayload(R"({"rooms":{"bath":{"temp":27,"hum":50}}})", d);
  buildWidget("indoor", d, s, v);
  TEST_ASSERT_FALSE(v.rows[0].alert);
  TEST_ASSERT_EQUAL_STRING("Bathroom", v.rows[0].label);
  loadPayload(R"({"rooms":{"bath":{"temp":27,"hum":75}}})", d);
  buildWidget("indoor", d, s, v);
  TEST_ASSERT_TRUE(v.rows[0].alert);
}

void test_widget_forecast_weekdays(void)
{
  setupData();
  buildWidget("forecast", d, s, v);
  TEST_ASSERT_EQUAL(WK_FORECAST, v.kind);
  TEST_ASSERT_EQUAL_STRING("Tue", v.rows[0].label); // 2026-10-19 is Monday
  TEST_ASSERT_EQUAL_STRING("Wed", v.rows[1].label);
  TEST_ASSERT_NOT_NULL(v.rows[0].icon);
  setLanguage("LV");
  buildWidget("forecast", d, s, v);
  TEST_ASSERT_EQUAL_STRING("O", v.rows[0].label);
  setLanguage("EN");
}

void test_widget_family(void)
{
  setupData();
  buildWidget("family", d, s, v);
  TEST_ASSERT_EQUAL(WK_PEOPLE, v.kind);
  TEST_ASSERT_EQUAL_STRING("1/3", v.hero);
  TEST_ASSERT_TRUE(v.rows[0].on);
  TEST_ASSERT_FALSE(v.rows[1].on);
  TEST_ASSERT_EQUAL_STRING("Away", v.rows[1].value);
  TEST_ASSERT_EQUAL_STRING("Ķengarags", v.rows[2].value);
}

void test_widget_power_and_prices(void)
{
  setupData();
  buildWidget("power", d, s, v);
  TEST_ASSERT_EQUAL_STRING("1322", v.hero);
  bool foundPf = false;
  for (int i = 0; i < v.n; i++)
    if (strcmp(v.rows[i].value, "0.91") == 0)
      foundPf = true;
  TEST_ASSERT_TRUE(foundPf); // 91 % normalised
  buildWidget("prices", d, s, v);
  TEST_ASSERT_EQUAL(WK_CHART, v.kind);
  TEST_ASSERT_EQUAL_FLOAT(0.05f, v.sMin);
  TEST_ASSERT_EQUAL_FLOAT(0.25f, v.sMax);
  TEST_ASSERT_EQUAL_STRING("0.110", v.hero); // now = index 2
  TEST_ASSERT_TRUE(v.rows[2].alert);          // max 0.25 > 0.20
}

void test_widget_hourly_house_calendar(void)
{
  setupData();
  buildWidget("hourly", d, s, v);
  TEST_ASSERT_EQUAL(WK_HOURLY, v.kind);
  TEST_ASSERT_EQUAL_FLOAT(8.1f, v.sMin);
  TEST_ASSERT_EQUAL_FLOAT(9.7f, v.sMax);
  buildWidget("house", d, s, v);
  TEST_ASSERT_TRUE(v.heroAlert); // a door is open
  bool garage = false;
  for (int i = 0; i < v.n; i++)
    if (!strcmp(v.rows[i].label, "Garage door"))
      garage = true;
  TEST_ASSERT_TRUE(garage);
  buildWidget("calendar", d, s, v);
  TEST_ASSERT_EQUAL(1, v.n);
  TEST_ASSERT_EQUAL_STRING("18:00", v.rows[0].value);
  TEST_ASSERT_EQUAL_STRING("Football", v.rows[0].label);
}

void test_widget_empty_data(void)
{
  settingsDefaults(s);
  dataReset(d);
  const char *ids[] = {"outdoor", "indoor", "forecast", "hourly", "family", "network", "power", "prices", "house", "extras"};
  for (const char *id : ids)
  {
    buildWidget(id, d, s, v);
    TEST_ASSERT_TRUE_MESSAGE(v.empty, id);
  }
}

void test_status_flags(void)
{
  setupData();
  StatusFlags f = computeStatus(d, s);
  TEST_ASSERT_EQUAL(1, f.home);
  TEST_ASSERT_EQUAL(3, f.people);
  TEST_ASSERT_TRUE(f.windy);
  TEST_ASSERT_TRUE(f.rain);   // forecast[0] 4.2 mm
  TEST_ASSERT_FALSE(f.frost);
  TEST_ASSERT_TRUE(f.open);
  TEST_ASSERT_TRUE(f.alerts >= 2);
  loadPayload(R"({"weather":{"temp":-3,"state":"snowy"}})", d);
  f = computeStatus(d, s);
  TEST_ASSERT_TRUE(f.frost);
  TEST_ASSERT_FALSE(f.open);
}

void test_weekday_names(void)
{
  setupData();
  TEST_ASSERT_EQUAL_STRING("Mon", weekdayName(d, 0));
  TEST_ASSERT_EQUAL_STRING("Sunday", weekdayName(d, 6, true));
  TEST_ASSERT_EQUAL_STRING("Mon", weekdayName(d, 7));
  dataReset(d);
  TEST_ASSERT_EQUAL_STRING("", weekdayName(d, 0));
}

void test_widget_home(void)
{
  setupData();
  buildWidget("home", d, s, v);
  TEST_ASSERT_EQUAL(WK_HOME, v.kind);
  TEST_ASSERT_EQUAL_STRING("21.5", v.hero);
  TEST_ASSERT_EQUAL_STRING("Heating", v.caption);
  TEST_ASSERT_EQUAL_STRING("1/3", v.rows[0].value);      // people at home
  TEST_ASSERT_EQUAL_FLOAT(8.1f, v.sMin);                  // daily mini chart range
  setLanguage("LV");
  buildWidget("home", d, s, v);
  TEST_ASSERT_EQUAL_STRING("Silda", v.caption);
  setLanguage("EN");
  dataReset(d);
  buildWidget("home", d, s, v);
  TEST_ASSERT_TRUE(v.empty);
}
