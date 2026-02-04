#include <unity.h>
#include "lang.h"

extern Translations lang;

void test_translate_weather_sunny(void) {
    const char* result = translateWeatherState("sunny");
    TEST_ASSERT_NOT_NULL(result);
    // Should return either "Saulains" (LV) or "Sunny" (EN)
    TEST_ASSERT_TRUE(strcmp(result, "Saulains") == 0 || strcmp(result, "Sunny") == 0);
}

void test_translate_weather_clear_night(void) {
    const char* result = translateWeatherState("clear-night");
    TEST_ASSERT_NOT_NULL(result);
    // Should return either "Skaidra nakts" (LV) or "Clear Night" (EN)
    TEST_ASSERT_TRUE(strcmp(result, "Skaidra nakts") == 0 || strcmp(result, "Clear Night") == 0);
}

void test_translate_weather_cloudy(void) {
    const char* result = translateWeatherState("cloudy");
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_TRUE(strcmp(result, "Mākoņains") == 0 || strcmp(result, "Cloudy") == 0);
}

void test_translate_weather_rainy(void) {
    const char* result = translateWeatherState("rainy");
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_TRUE(strcmp(result, "Lietus") == 0 || strcmp(result, "Rainy") == 0);
}

void test_translate_weather_snowy(void) {
    const char* result = translateWeatherState("snowy");
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_TRUE(strcmp(result, "Sniegs") == 0 || strcmp(result, "Snowy") == 0);
}

void test_translate_weather_fog(void) {
    const char* result = translateWeatherState("fog");
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_TRUE(strcmp(result, "Migla") == 0 || strcmp(result, "Fog") == 0);
}

void test_translate_weather_hail(void) {
    const char* result = translateWeatherState("hail");
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_TRUE(strcmp(result, "Krusa") == 0 || strcmp(result, "Hail") == 0);
}

void test_translate_weather_windy(void) {
    const char* result = translateWeatherState("windy");
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_TRUE(strcmp(result, "Vējains") == 0 || strcmp(result, "Windy") == 0);
}

void test_translate_weather_lightning(void) {
    const char* result = translateWeatherState("lightning");
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_TRUE(strcmp(result, "Zibens") == 0 || strcmp(result, "Lightning") == 0);
}

void test_translate_weather_thunderstorm(void) {
    const char* result = translateWeatherState("lightning-rainy");
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_TRUE(strcmp(result, "Pērkona negaiss") == 0 || strcmp(result, "Thunderstorm") == 0);
}

void test_translate_weather_unknown_fallback(void) {
    const char* result = translateWeatherState("unknown-state");
    TEST_ASSERT_EQUAL_STRING("unknown-state", result);
}

void test_translate_family_home(void) {
    const char* result = translateFamilyState("home");
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_TRUE(strcmp(result, "Mājās") == 0 || strcmp(result, "Home") == 0);
}

void test_translate_family_not_home(void) {
    const char* result = translateFamilyState("not_home");
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_TRUE(strcmp(result, "Prom") == 0 || strcmp(result, "Away") == 0);
}

void test_translate_family_unknown_fallback(void) {
    const char* result = translateFamilyState("unknown");
    TEST_ASSERT_EQUAL_STRING("unknown", result);
}

void test_lang_struct_not_null(void) {
    // Test that lang structure is initialized
    TEST_ASSERT_NOT_NULL(lang.title_outdoor);
    TEST_ASSERT_NOT_NULL(lang.title_indoor);
    TEST_ASSERT_NOT_NULL(lang.title_network);
    TEST_ASSERT_NOT_NULL(lang.title_electricity);
    TEST_ASSERT_NOT_NULL(lang.label_temperature);
    TEST_ASSERT_NOT_NULL(lang.label_humidity);
    TEST_ASSERT_NOT_NULL(lang.weather_sunny);
    TEST_ASSERT_NOT_NULL(lang.state_home);
}

void test_lang_outdoor_title(void) {
    // Should be either "Ārā" (LV) or "Outdoor" (EN)
    TEST_ASSERT_TRUE(strcmp(lang.title_outdoor, "Ārā") == 0 || 
                     strcmp(lang.title_outdoor, "Outdoor") == 0);
}

void test_lang_indoor_title(void) {
    // Should be either "Telpās" (LV) or "Indoor" (EN)
    TEST_ASSERT_TRUE(strcmp(lang.title_indoor, "Telpās") == 0 || 
                     strcmp(lang.title_indoor, "Indoor") == 0);
}

void test_lang_network_title(void) {
    // Should be either "Tīkls" (LV) or "Network" (EN)
    TEST_ASSERT_TRUE(strcmp(lang.title_network, "Tīkls") == 0 || 
                     strcmp(lang.title_network, "Network") == 0);
}

void test_lang_electricity_title(void) {
    // Should be either "Elektrība" (LV) or "Electricity" (EN)
    TEST_ASSERT_TRUE(strcmp(lang.title_electricity, "Elektrība") == 0 || 
                     strcmp(lang.title_electricity, "Electricity") == 0);
}

void test_all_weather_states_covered(void) {
    // Test all weather states have translations
    TEST_ASSERT_NOT_NULL(translateWeatherState("sunny"));
    TEST_ASSERT_NOT_NULL(translateWeatherState("clear-night"));
    TEST_ASSERT_NOT_NULL(translateWeatherState("cloudy"));
    TEST_ASSERT_NOT_NULL(translateWeatherState("partlycloudy"));
    TEST_ASSERT_NOT_NULL(translateWeatherState("rainy"));
    TEST_ASSERT_NOT_NULL(translateWeatherState("pouring"));
    TEST_ASSERT_NOT_NULL(translateWeatherState("snowy"));
    TEST_ASSERT_NOT_NULL(translateWeatherState("snowy-rainy"));
    TEST_ASSERT_NOT_NULL(translateWeatherState("fog"));
    TEST_ASSERT_NOT_NULL(translateWeatherState("hail"));
    TEST_ASSERT_NOT_NULL(translateWeatherState("windy"));
    TEST_ASSERT_NOT_NULL(translateWeatherState("windy-variant"));
    TEST_ASSERT_NOT_NULL(translateWeatherState("lightning"));
    TEST_ASSERT_NOT_NULL(translateWeatherState("lightning-rainy"));
    TEST_ASSERT_NOT_NULL(translateWeatherState("exceptional"));
}
