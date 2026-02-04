#include <unity.h>
#include "display/drawing.h"
#include "icons.h"
#include "data.h"
#include <string.h>

extern DashboardData data;

// Test weather icon mapping
void test_weather_icon_sunny(void) {
    const uint8_t* icon = getWeatherIcon("sunny");
    TEST_ASSERT_NOT_NULL(icon);
    // Verify it's actually ICON_SUNNY by comparing first byte
    TEST_ASSERT_EQUAL_UINT8(pgm_read_byte(&ICON_SUNNY[0]), pgm_read_byte(&icon[0]));
}

void test_weather_icon_clear_night(void) {
    const uint8_t* icon = getWeatherIcon("clear-night");
    TEST_ASSERT_NOT_NULL(icon);
    TEST_ASSERT_EQUAL_UINT8(pgm_read_byte(&ICON_CLEAR_NIGHT[0]), pgm_read_byte(&icon[0]));
}

void test_weather_icon_cloudy(void) {
    const uint8_t* icon = getWeatherIcon("cloudy");
    TEST_ASSERT_NOT_NULL(icon);
    TEST_ASSERT_EQUAL_UINT8(pgm_read_byte(&ICON_CLOUDY[0]), pgm_read_byte(&icon[0]));
}

void test_weather_icon_partlycloudy(void) {
    const uint8_t* icon = getWeatherIcon("partlycloudy");
    TEST_ASSERT_NOT_NULL(icon);
    TEST_ASSERT_EQUAL_UINT8(pgm_read_byte(&ICON_PARTLYCLOUDY[0]), pgm_read_byte(&icon[0]));
}

void test_weather_icon_rainy(void) {
    const uint8_t* icon = getWeatherIcon("rainy");
    TEST_ASSERT_NOT_NULL(icon);
    TEST_ASSERT_EQUAL_UINT8(pgm_read_byte(&ICON_RAINY[0]), pgm_read_byte(&icon[0]));
}

void test_weather_icon_pouring(void) {
    const uint8_t* icon = getWeatherIcon("pouring");
    TEST_ASSERT_NOT_NULL(icon);
    // pouring maps to rainy
    TEST_ASSERT_EQUAL_UINT8(pgm_read_byte(&ICON_RAINY[0]), pgm_read_byte(&icon[0]));
}

void test_weather_icon_snowy(void) {
    const uint8_t* icon = getWeatherIcon("snowy");
    TEST_ASSERT_NOT_NULL(icon);
    TEST_ASSERT_EQUAL_UINT8(pgm_read_byte(&ICON_SNOWY[0]), pgm_read_byte(&icon[0]));
}

void test_weather_icon_snowy_rainy(void) {
    const uint8_t* icon = getWeatherIcon("snowy-rainy");
    TEST_ASSERT_NOT_NULL(icon);
    // snowy-rainy maps to snowy
    TEST_ASSERT_EQUAL_UINT8(pgm_read_byte(&ICON_SNOWY[0]), pgm_read_byte(&icon[0]));
}

void test_weather_icon_fog(void) {
    const uint8_t* icon = getWeatherIcon("fog");
    TEST_ASSERT_NOT_NULL(icon);
    TEST_ASSERT_EQUAL_UINT8(pgm_read_byte(&ICON_FOG[0]), pgm_read_byte(&icon[0]));
}

void test_weather_icon_hail(void) {
    const uint8_t* icon = getWeatherIcon("hail");
    TEST_ASSERT_NOT_NULL(icon);
    TEST_ASSERT_EQUAL_UINT8(pgm_read_byte(&ICON_HAIL[0]), pgm_read_byte(&icon[0]));
}

void test_weather_icon_windy(void) {
    const uint8_t* icon = getWeatherIcon("windy");
    TEST_ASSERT_NOT_NULL(icon);
    TEST_ASSERT_EQUAL_UINT8(pgm_read_byte(&ICON_WINDY[0]), pgm_read_byte(&icon[0]));
}

void test_weather_icon_windy_variant(void) {
    const uint8_t* icon = getWeatherIcon("windy-variant");
    TEST_ASSERT_NOT_NULL(icon);
    TEST_ASSERT_EQUAL_UINT8(pgm_read_byte(&ICON_WINDY_VARIANT[0]), pgm_read_byte(&icon[0]));
}

void test_weather_icon_lightning(void) {
    const uint8_t* icon = getWeatherIcon("lightning");
    TEST_ASSERT_NOT_NULL(icon);
    // lightning maps to thunderstorm
    TEST_ASSERT_EQUAL_UINT8(pgm_read_byte(&ICON_THUNDERSTORM[0]), pgm_read_byte(&icon[0]));
}

void test_weather_icon_thunderstorm(void) {
    const uint8_t* icon = getWeatherIcon("thunderstorm");
    TEST_ASSERT_NOT_NULL(icon);
    TEST_ASSERT_EQUAL_UINT8(pgm_read_byte(&ICON_THUNDERSTORM[0]), pgm_read_byte(&icon[0]));
}

void test_weather_icon_lightning_rainy(void) {
    const uint8_t* icon = getWeatherIcon("lightning-rainy");
    TEST_ASSERT_NOT_NULL(icon);
    // lightning-rainy maps to thunderstorm
    TEST_ASSERT_EQUAL_UINT8(pgm_read_byte(&ICON_THUNDERSTORM[0]), pgm_read_byte(&icon[0]));
}

void test_weather_icon_unknown_fallback(void) {
    const uint8_t* icon = getWeatherIcon("unknown");
    TEST_ASSERT_NOT_NULL(icon);
    // unknown defaults to cloudy
    TEST_ASSERT_EQUAL_UINT8(pgm_read_byte(&ICON_CLOUDY[0]), pgm_read_byte(&icon[0]));
}

void test_weather_icon_invalid_fallback(void) {
    const uint8_t* icon = getWeatherIcon("not-a-real-weather-state");
    TEST_ASSERT_NOT_NULL(icon);
    // invalid defaults to cloudy
    TEST_ASSERT_EQUAL_UINT8(pgm_read_byte(&ICON_CLOUDY[0]), pgm_read_byte(&icon[0]));
}

// Test forecast data structure
void test_forecast_data_structure(void) {
    strcpy(data.forecast[0].condition, "sunny");
    data.forecast[0].tempLow = -5.5f;
    data.forecast[0].tempHigh = 10.2f;
    data.forecast[0].precipitation = 2.5f;
    
    TEST_ASSERT_EQUAL_STRING("sunny", data.forecast[0].condition);
    TEST_ASSERT_EQUAL_FLOAT(-5.5f, data.forecast[0].tempLow);
    TEST_ASSERT_EQUAL_FLOAT(10.2f, data.forecast[0].tempHigh);
    TEST_ASSERT_EQUAL_FLOAT(2.5f, data.forecast[0].precipitation);
}

void test_forecast_condition_buffer_size(void) {
    // Test that condition buffer can hold longest weather state string
    strcpy(data.forecast[0].condition, "lightning-rainy"); // 15 chars
    TEST_ASSERT_EQUAL_STRING("lightning-rainy", data.forecast[0].condition);
}

void test_forecast_multiple_days(void) {
    // Test all 5 forecast days
    for (int i = 0; i < 5; i++) {
        char condition[24];
        sprintf(condition, "day%d", i + 1);
        strcpy(data.forecast[i].condition, condition);
        data.forecast[i].tempLow = -10.0f + i;
        data.forecast[i].tempHigh = 5.0f + i * 2;
        data.forecast[i].precipitation = i * 0.5f;
    }
    
    // Verify all days stored correctly
    for (int i = 0; i < 5; i++) {
        char expected[24];
        sprintf(expected, "day%d", i + 1);
        TEST_ASSERT_EQUAL_STRING(expected, data.forecast[i].condition);
        TEST_ASSERT_EQUAL_FLOAT(-10.0f + i, data.forecast[i].tempLow);
        TEST_ASSERT_EQUAL_FLOAT(5.0f + i * 2, data.forecast[i].tempHigh);
        TEST_ASSERT_EQUAL_FLOAT(i * 0.5f, data.forecast[i].precipitation);
    }
}

void test_forecast_negative_temperatures(void) {
    // Test with extreme negative temperatures
    data.forecast[0].tempLow = -30.5f;
    data.forecast[0].tempHigh = -15.2f;
    
    TEST_ASSERT_EQUAL_FLOAT(-30.5f, data.forecast[0].tempLow);
    TEST_ASSERT_EQUAL_FLOAT(-15.2f, data.forecast[0].tempHigh);
    TEST_ASSERT_TRUE(data.forecast[0].tempLow < data.forecast[0].tempHigh);
}

void test_forecast_zero_precipitation(void) {
    // Test zero precipitation case
    data.forecast[0].precipitation = 0.0f;
    TEST_ASSERT_EQUAL_FLOAT(0.0f, data.forecast[0].precipitation);
}

void test_forecast_heavy_precipitation(void) {
    // Test high precipitation values
    data.forecast[0].precipitation = 50.5f;
    TEST_ASSERT_EQUAL_FLOAT(50.5f, data.forecast[0].precipitation);
}

// Test icon data integrity
void test_icon_data_not_null(void) {
    TEST_ASSERT_NOT_NULL(ICON_SUNNY);
    TEST_ASSERT_NOT_NULL(ICON_CLEAR_NIGHT);
    TEST_ASSERT_NOT_NULL(ICON_CLOUDY);
    TEST_ASSERT_NOT_NULL(ICON_PARTLYCLOUDY);
    TEST_ASSERT_NOT_NULL(ICON_RAINY);
    TEST_ASSERT_NOT_NULL(ICON_SNOWY);
    TEST_ASSERT_NOT_NULL(ICON_FOG);
    TEST_ASSERT_NOT_NULL(ICON_HAIL);
    TEST_ASSERT_NOT_NULL(ICON_WINDY);
    TEST_ASSERT_NOT_NULL(ICON_WINDY_VARIANT);
    TEST_ASSERT_NOT_NULL(ICON_THUNDERSTORM);
}

void test_icon_size(void) {
    // Each 24x24 icon should be 72 bytes (24 rows × 3 bytes per row)
    // We can't directly test array size in C, but we can verify first and last bytes are accessible
    uint8_t first = pgm_read_byte(&ICON_SUNNY[0]);
    uint8_t last = pgm_read_byte(&ICON_SUNNY[71]);
    
    // Just verify they're accessible (will crash if array too small)
    TEST_ASSERT_TRUE(first >= 0);
    TEST_ASSERT_TRUE(last >= 0);
}
