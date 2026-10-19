#include <unity.h>
#include "utils/utils.h"
#include "fixtures.h"

void test_parse_date_formats(void)
{
  int d, m, y;
  TEST_ASSERT_TRUE(parseDate("2026-03-14", d, m, y));
  TEST_ASSERT_EQUAL(14, d); TEST_ASSERT_EQUAL(3, m); TEST_ASSERT_EQUAL(2026, y);
  TEST_ASSERT_TRUE(parseDate("14.03.2026", d, m, y));
  TEST_ASSERT_EQUAL(14, d); TEST_ASSERT_EQUAL(3, m);
  TEST_ASSERT_TRUE(parseDate("03/14/2026", d, m, y)); // US
  TEST_ASSERT_EQUAL(14, d); TEST_ASSERT_EQUAL(3, m);
  TEST_ASSERT_TRUE(parseDate("25/12/2026", d, m, y)); // first field > 12 -> DD/MM
  TEST_ASSERT_EQUAL(25, d); TEST_ASSERT_EQUAL(12, m);
  TEST_ASSERT_TRUE(parseDate("2026-10-19T14:31:00+03:00", d, m, y));
  TEST_ASSERT_EQUAL(19, d);
}

void test_parse_date_rejects_garbage(void)
{
  int d, m, y;
  TEST_ASSERT_FALSE(parseDate("", d, m, y));
  TEST_ASSERT_FALSE(parseDate("--", d, m, y));
  TEST_ASSERT_FALSE(parseDate("2026-13-01", d, m, y));
  TEST_ASSERT_FALSE(parseDate("hello world", d, m, y));
  TEST_ASSERT_FALSE(parseDate(nullptr, d, m, y));
}

void test_format_date_all_targets(void)
{
  char o[16];
  formatDate("2026-03-14", o, sizeof(o), "DD.MM.YYYY");
  TEST_ASSERT_EQUAL_STRING("14.03.2026", o);
  formatDate("14.03.2026", o, sizeof(o), "YYYY-MM-DD");
  TEST_ASSERT_EQUAL_STRING("2026-03-14", o);
  formatDate("2026-03-14", o, sizeof(o), "MM/DD/YYYY");
  TEST_ASSERT_EQUAL_STRING("03/14/2026", o);
  formatDate("2026-03-14", o, sizeof(o), "DD/MM/YYYY");
  TEST_ASSERT_EQUAL_STRING("14/03/2026", o);
  formatDate("not a date", o, sizeof(o), "DD.MM.YYYY");
  TEST_ASSERT_EQUAL_STRING("not a date", o);
}

void test_day_of_week(void)
{
  TEST_ASSERT_EQUAL(1, dayOfWeek(2026, 10, 19)); // Monday
  TEST_ASSERT_EQUAL(6, dayOfWeek(2026, 3, 14));  // Saturday
  TEST_ASSERT_EQUAL(0, dayOfWeek(2000, 1, 2));   // Sunday
  TEST_ASSERT_EQUAL(4, dayOfWeek(2024, 2, 29));  // leap day: Thursday
}

void test_extract_hhmm(void)
{
  char o[6];
  TEST_ASSERT_TRUE(extractHHMM("07:58", o));
  TEST_ASSERT_EQUAL_STRING("07:58", o);
  TEST_ASSERT_TRUE(extractHHMM("2026-10-19T04:58:00+00:00", o));
  TEST_ASSERT_EQUAL_STRING("04:58", o);
  TEST_ASSERT_TRUE(extractHHMM("18:12:59", o));
  TEST_ASSERT_EQUAL_STRING("18:12", o);
  TEST_ASSERT_FALSE(extractHHMM("soon", o));
  TEST_ASSERT_FALSE(extractHHMM(nullptr, o));
}

void test_fmt_num(void)
{
  char o[24];
  fmtNum(o, sizeof(o), 21.456f, 1, "\xC2\xB0" "C");
  TEST_ASSERT_EQUAL_STRING("21.5 \xC2\xB0" "C", o);
  fmtNum(o, sizeof(o), NAN, 1, "W");
  TEST_ASSERT_EQUAL_STRING("-- W", o);
  fmtNum(o, sizeof(o), NAN, 0, "");
  TEST_ASSERT_EQUAL_STRING("--", o);
  fmtNum(o, sizeof(o), -3.0f, 0, "");
  TEST_ASSERT_EQUAL_STRING("-3", o);
}

void test_utf8_len_and_clip(void)
{
  TEST_ASSERT_EQUAL(5, utf8Len("Ķengā"));
  TEST_ASSERT_EQUAL(3, utf8Len("a€b"));
  char o[16];
  utf8Clip(o, sizeof(o), "Guļamistaba", 4);
  TEST_ASSERT_EQUAL_STRING("Guļa", o);
  utf8Clip(o, 4, "āāā", 3); // buffer too small for 3x2 bytes -> never split a sequence
  TEST_ASSERT_EQUAL_STRING("ā", o);
}

void test_utf8_upper_latvian(void)
{
  char s[64] = "šodien ārā guļ ķēķis ņ ž č ģ ī ū abc";
  utf8Upper(s);
  TEST_ASSERT_EQUAL_STRING("ŠODIEN ĀRĀ GUĻ ĶĒĶIS Ņ Ž Č Ģ Ī Ū ABC", s);
  char t[16] = "ŠĀ°";
  utf8Upper(t);
  TEST_ASSERT_EQUAL_STRING("ŠĀ°", t);
}

void test_parse_number(void)
{
  TEST_ASSERT_EQUAL_FLOAT(18.5f, parseNumber("18.5"));
  TEST_ASSERT_EQUAL_FLOAT(-2.0f, parseNumber("-2"));
  TEST_ASSERT_TRUE(isnan(parseNumber("unavailable")));
  TEST_ASSERT_TRUE(isnan(parseNumber("")));
  TEST_ASSERT_TRUE(isnan(parseNumber(nullptr)));
}

void test_alerts(void)
{
  Settings s;
  settingsDefaults(s);
  const Thresholds &t = s.th;
  TEST_ASSERT_TRUE(alertOutTemp(t, -11));
  TEST_ASSERT_TRUE(alertOutTemp(t, 31));
  TEST_ASSERT_FALSE(alertOutTemp(t, 10));
  TEST_ASSERT_FALSE(alertOutTemp(t, NAN));
  TEST_ASSERT_TRUE(alertWind(t, 20.1f));
  TEST_ASSERT_TRUE(alertGust(t, 26));
  TEST_ASSERT_TRUE(alertInTemp(t, 18.9f));
  TEST_ASSERT_FALSE(alertInTemp(t, 19));
  TEST_ASSERT_FALSE(alertInTemp(t, 25));
  TEST_ASSERT_TRUE(alertInHum(t, 60.1f));
  TEST_ASSERT_TRUE(alertDown(t, 9));
  TEST_ASSERT_TRUE(alertUp(t, 4));
  TEST_ASSERT_TRUE(alertPing(t, 51));
  TEST_ASSERT_TRUE(alertPrice(t, 0.21f));
  TEST_ASSERT_FALSE(alertPrice(t, 0.11f));
  TEST_ASSERT_TRUE(alertPower(t, 5001));
  TEST_ASSERT_TRUE(alertVoltage(t, 219));
  TEST_ASSERT_TRUE(alertVoltage(t, 241));
  TEST_ASSERT_TRUE(alertFrequency(t, 49.4f));
  TEST_ASSERT_FALSE(alertFrequency(t, 49.99f));
  TEST_ASSERT_TRUE(alertMonth(t, 321));
  TEST_ASSERT_TRUE(alertRssi(t, -80));
  TEST_ASSERT_FALSE(alertRssi(t, 0)); // 0 = unknown
}

void test_power_factor_normalisation(void)
{
  Settings s;
  settingsDefaults(s);
  TEST_ASSERT_EQUAL_FLOAT(0.91f, normFactor(91));
  TEST_ASSERT_EQUAL_FLOAT(0.98f, normFactor(0.98f));
  TEST_ASSERT_TRUE(alertFactor(s.th, 60));    // 60 % < 0.70
  TEST_ASSERT_FALSE(alertFactor(s.th, 0.95f));
  TEST_ASSERT_TRUE(isnan(normFactor(NAN)));
}

void test_legacy_bad_temp_hum(void)
{
  settingsDefaults(settings);
  TEST_ASSERT_TRUE(badTemp(18));
  TEST_ASSERT_FALSE(badTemp(22));
  TEST_ASSERT_TRUE(badHum(34));
  TEST_ASSERT_FALSE(badHum(50));
}

void test_quiet_hours(void)
{
  TEST_ASSERT_FALSE(inQuietHours(-1, 6, 3));
  TEST_ASSERT_TRUE(inQuietHours(0, 6, 3));
  TEST_ASSERT_FALSE(inQuietHours(0, 6, 6));
  TEST_ASSERT_TRUE(inQuietHours(23, 6, 23));  // wraps midnight
  TEST_ASSERT_TRUE(inQuietHours(23, 6, 2));
  TEST_ASSERT_FALSE(inQuietHours(23, 6, 12));
  TEST_ASSERT_EQUAL_UINT32(3 * 3600, secondsUntilQuietEnd(0, 6, 3, 0, 0));
  TEST_ASSERT_EQUAL_UINT32(7 * 3600 - 1800, secondsUntilQuietEnd(23, 6, 23, 30, 0));
  TEST_ASSERT_EQUAL_UINT32(0, secondsUntilQuietEnd(0, 6, 12, 0, 0));
}

void test_seconds_to_slot(void)
{
  // 14:31:00 -> next 15-min slot 14:45:00 = 840 s
  int64_t t = 1792420260; // 2026-10-19 14:31:00 UTC
  TEST_ASSERT_EQUAL_UINT32(840, secondsToSlot(t, 900, 0, 30));
  TEST_ASSERT_EQUAL_UINT32(750, secondsToSlot(t, 900, 90, 30)); // wake 90 s early
  TEST_ASSERT_EQUAL_UINT32(842, secondsToSlot(t, 900, -2, 30)); // pull: just after
  // too close to the slot -> skip to the following one
  TEST_ASSERT_EQUAL_UINT32(900 + 10, secondsToSlot(t + 830, 900, 0, 30));
  TEST_ASSERT_EQUAL_UINT32(60, secondsToSlot(0, 0, 0, 30)); // interval 0 guarded
}
