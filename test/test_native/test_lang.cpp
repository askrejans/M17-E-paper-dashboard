#include <unity.h>
#include "lang.h"
#include <initializer_list>

void test_lang_all_strings_present(void)
{
  // Every Translations member is a pointer; none may be null in either language.
  const char *codes[] = {"EN", "LV"};
  for (const char *c : codes)
  {
    setLanguage(c);
    const void *const *p = (const void *const *)&lang;
    size_t n = sizeof(Translations) / sizeof(void *);
    for (size_t i = 0; i < n; i++)
      TEST_ASSERT_NOT_NULL_MESSAGE(p[i], c);
    for (int w = 0; w < 7; w++)
    {
      TEST_ASSERT_NOT_NULL(lang.weekdays[w]);
      TEST_ASSERT_NOT_NULL(lang.weekdays_long[w]);
    }
  }
  setLanguage("EN");
}

void test_lang_switching(void)
{
  setLanguage("LV");
  TEST_ASSERT_EQUAL_STRING("LV", lang.code);
  TEST_ASSERT_EQUAL_STRING("Ārā", lang.title_outdoor);
  TEST_ASSERT_EQUAL_STRING("Pirmdiena", lang.weekdays_long[1]);
  setLanguage("XX"); // unknown -> English
  TEST_ASSERT_EQUAL_STRING("EN", lang.code);
  setLanguage(nullptr);
  TEST_ASSERT_EQUAL_STRING("Outdoor", lang.title_outdoor);
}

void test_translate_weather_states(void)
{
  const char *states[] = {"sunny", "clear-night", "cloudy", "partlycloudy", "rainy", "pouring", "snowy", "snowy-rainy",
                          "fog", "hail", "windy", "windy-variant", "lightning", "lightning-rainy", "exceptional"};
  for (const char *c : {"EN", "LV"})
  {
    setLanguage(c);
    for (const char *s : states)
      TEST_ASSERT_TRUE_MESSAGE(strcmp(translateWeatherState(s), s) != 0, s);
  }
  setLanguage("LV");
  TEST_ASSERT_EQUAL_STRING("Daļēji mākoņains", translateWeatherState("partlycloudy"));
  setLanguage("EN");
  TEST_ASSERT_EQUAL_STRING("Thunderstorm", translateWeatherState("lightning-rainy"));
  TEST_ASSERT_EQUAL_STRING("tornado", translateWeatherState("tornado")); // passthrough
}

void test_translate_family_states(void)
{
  setLanguage("EN");
  TEST_ASSERT_EQUAL_STRING("Home", translateFamilyState("home"));
  TEST_ASSERT_EQUAL_STRING("Away", translateFamilyState("not_home"));
  TEST_ASSERT_EQUAL_STRING("Away", translateFamilyState("away"));
  TEST_ASSERT_EQUAL_STRING("Work", translateFamilyState("Work")); // zone names pass through
  setLanguage("LV");
  TEST_ASSERT_EQUAL_STRING("Mājās", translateFamilyState("home"));
  setLanguage("EN");
}

void test_translate_names(void)
{
  setLanguage("EN");
  TEST_ASSERT_EQUAL_STRING("Bathroom", translateName("@bath"));
  TEST_ASSERT_EQUAL_STRING("garage", translateName("@garage"));
  TEST_ASSERT_EQUAL_STRING("Kitchen", translateName("Kitchen"));
}
