#include <unity.h>
#include "config.h"
#include "display/epd_display.h"
#include "display/drawing.h"
#include "font_8x16.h"
#include "utils/utils.h"
#include "data.h"
#include "lang.h"
#include <ArduinoJson.h>

// Drawing tests (defined in test_drawing.cpp)
void test_px_pack(void);
void test_fillRect_and_pack(void);
void test_rect_border(void);
void test_rectWithTitle_changes_titlebar(void);

// Font tests (defined in test_fonts.cpp)
void test_font_init_nonzero(void);
void test_text_utf8_latvian(void);
void test_text_degree_symbol(void);

// Web tests (defined in test_web.cpp)
void test_web_json_parsing(void);

// Console tests (defined in test_console.cpp)
void test_console_log_functions(void);

// Language tests (defined in test_lang.cpp)
void test_translate_weather_sunny(void);
void test_translate_weather_clear_night(void);
void test_translate_weather_cloudy(void);
void test_translate_weather_rainy(void);
void test_translate_weather_snowy(void);
void test_translate_weather_fog(void);
void test_translate_weather_hail(void);
void test_translate_weather_windy(void);
void test_translate_weather_lightning(void);
void test_translate_weather_thunderstorm(void);
void test_translate_weather_unknown_fallback(void);
void test_translate_family_home(void);
void test_translate_family_not_home(void);
void test_translate_family_unknown_fallback(void);
void test_lang_struct_not_null(void);
void test_lang_outdoor_title(void);
void test_lang_indoor_title(void);
void test_lang_network_title(void);
void test_lang_electricity_title(void);
void test_all_weather_states_covered(void);

// Weather tests (defined in test_weather.cpp)
void test_weather_icon_sunny(void);
void test_weather_icon_clear_night(void);
void test_weather_icon_cloudy(void);
void test_weather_icon_partlycloudy(void);
void test_weather_icon_rainy(void);
void test_weather_icon_pouring(void);
void test_weather_icon_snowy(void);
void test_weather_icon_snowy_rainy(void);
void test_weather_icon_fog(void);
void test_weather_icon_hail(void);
void test_weather_icon_windy(void);
void test_weather_icon_windy_variant(void);
void test_weather_icon_lightning(void);
void test_weather_icon_thunderstorm(void);
void test_weather_icon_lightning_rainy(void);
void test_weather_icon_unknown_fallback(void);
void test_weather_icon_invalid_fallback(void);
void test_forecast_data_structure(void);
void test_forecast_condition_buffer_size(void);
void test_forecast_multiple_days(void);
void test_forecast_negative_temperatures(void);
void test_forecast_zero_precipitation(void);
void test_forecast_heavy_precipitation(void);
void test_icon_data_not_null(void);
void test_icon_size(void);

unsigned char *fb;
DashboardData data;
volatile bool redraw_requested = false;

void test_config_constants(void) {
    TEST_ASSERT_EQUAL(15, UPDATE_INTERVAL_MIN);
    TEST_ASSERT_EQUAL(90, WAKE_BEFORE_SEC);
    TEST_ASSERT_EQUAL(180, LISTEN_WINDOW_SEC);
    TEST_ASSERT_EQUAL(1000000ULL, US_PER_SEC);
    TEST_ASSERT_EQUAL(60000000ULL, US_PER_MIN);
    TEST_ASSERT_EQUAL(800, EPD_WIDTH);
    TEST_ASSERT_EQUAL(480, EPD_HEIGHT);
}

void test_color_get(void) {
    TEST_ASSERT_EQUAL(black, Color_get(0x00));
    TEST_ASSERT_EQUAL(white, Color_get(0x01));
    TEST_ASSERT_EQUAL(yellow, Color_get(0x02));
    TEST_ASSERT_EQUAL(red, Color_get(0x03));
    TEST_ASSERT_EQUAL(black, Color_get(0x04)); // default
}

void test_jf(void) {
    JsonDocument doc;
    doc["temp"] = 22.5;
    TEST_ASSERT_EQUAL_FLOAT(22.5f, jf(doc["temp"]));
    doc["temp"] = "18.5";
    TEST_ASSERT_EQUAL_FLOAT(18.5f, jf(doc["temp"]));
    doc["temp"] = "invalid";
    TEST_ASSERT_EQUAL_FLOAT(0.0f, jf(doc["temp"]));
    doc["temp"] = nullptr;
    TEST_ASSERT_EQUAL_FLOAT(0.0f, jf(doc["temp"]));
}

void test_badTemp(void) {
    TEST_ASSERT_TRUE(badTemp(30));
    TEST_ASSERT_FALSE(badTemp(22));
    TEST_ASSERT_TRUE(badTemp(18));
    TEST_ASSERT_TRUE(badTemp(26));
    TEST_ASSERT_FALSE(badTemp(20));
    TEST_ASSERT_FALSE(badTemp(25));
    TEST_ASSERT_TRUE(badTemp(18.5f));
    TEST_ASSERT_FALSE(badTemp(22.0f));
}

void test_badHum(void) {
    TEST_ASSERT_TRUE(badHum(30));
    TEST_ASSERT_FALSE(badHum(50));
    TEST_ASSERT_TRUE(badHum(34));
    TEST_ASSERT_TRUE(badHum(61));
    TEST_ASSERT_FALSE(badHum(40));
    TEST_ASSERT_FALSE(badHum(60));
    TEST_ASSERT_TRUE(badHum(34.9f));
    TEST_ASSERT_FALSE(badHum(50.0f));
}

void test_badTemp_boundary(void) {
    TEST_ASSERT_FALSE(badTemp(19));
    TEST_ASSERT_FALSE(badTemp(25));
    TEST_ASSERT_TRUE(badTemp(18.9f));
    TEST_ASSERT_TRUE(badTemp(25.1f));
    TEST_ASSERT_TRUE(badTemp(18));
    TEST_ASSERT_TRUE(badTemp(26));
}

void test_badHum_boundary(void) {
    TEST_ASSERT_FALSE(badHum(35));
    TEST_ASSERT_FALSE(badHum(60));
    TEST_ASSERT_TRUE(badHum(34.9f));
    TEST_ASSERT_TRUE(badHum(60.1f));
    TEST_ASSERT_TRUE(badHum(34));
    TEST_ASSERT_TRUE(badHum(61));
}

void test_fill(void) {
    fill(white);
    TEST_ASSERT_EQUAL(0x55, fb[0]);
    fill(black);
    TEST_ASSERT_EQUAL(0x00, fb[0]);
    fill(yellow);
    TEST_ASSERT_EQUAL(0x00, fb[0]);
}

#ifdef PIO_UNIT_TESTING
void setup() {
    fb = (unsigned char *)malloc(ALLSCREEN_BYTES);
    font8x16_init();
    initLanguage();
    
    UNITY_BEGIN();
    RUN_TEST(test_config_constants);
    RUN_TEST(test_color_get);
    RUN_TEST(test_jf);
    RUN_TEST(test_badTemp);
    RUN_TEST(test_badHum);
    RUN_TEST(test_badTemp_boundary);
    RUN_TEST(test_badHum_boundary);
    RUN_TEST(test_fill);
    
    // Drawing tests
    RUN_TEST(test_px_pack);
    RUN_TEST(test_fillRect_and_pack);
    RUN_TEST(test_rect_border);
    RUN_TEST(test_rectWithTitle_changes_titlebar);
    
    // Font tests
    RUN_TEST(test_font_init_nonzero);
    RUN_TEST(test_text_utf8_latvian);
    RUN_TEST(test_text_degree_symbol);
    
    // Web tests
    RUN_TEST(test_web_json_parsing);
    
    // Console tests
    RUN_TEST(test_console_log_functions);
    
    // Language tests
    RUN_TEST(test_translate_weather_sunny);
    RUN_TEST(test_translate_weather_clear_night);
    RUN_TEST(test_translate_weather_cloudy);
    RUN_TEST(test_translate_weather_rainy);
    RUN_TEST(test_translate_weather_snowy);
    RUN_TEST(test_translate_weather_fog);
    RUN_TEST(test_translate_weather_hail);
    RUN_TEST(test_translate_weather_windy);
    RUN_TEST(test_translate_weather_lightning);
    RUN_TEST(test_translate_weather_thunderstorm);
    RUN_TEST(test_translate_weather_unknown_fallback);
    RUN_TEST(test_translate_family_home);
    RUN_TEST(test_translate_family_not_home);
    RUN_TEST(test_translate_family_unknown_fallback);
    RUN_TEST(test_lang_struct_not_null);
    RUN_TEST(test_lang_outdoor_title);
    RUN_TEST(test_lang_indoor_title);
    RUN_TEST(test_lang_network_title);
    RUN_TEST(test_lang_electricity_title);
    RUN_TEST(test_all_weather_states_covered);
    
    // Weather tests
    RUN_TEST(test_weather_icon_sunny);
    RUN_TEST(test_weather_icon_clear_night);
    RUN_TEST(test_weather_icon_cloudy);
    RUN_TEST(test_weather_icon_partlycloudy);
    RUN_TEST(test_weather_icon_rainy);
    RUN_TEST(test_weather_icon_pouring);
    RUN_TEST(test_weather_icon_snowy);
    RUN_TEST(test_weather_icon_snowy_rainy);
    RUN_TEST(test_weather_icon_fog);
    RUN_TEST(test_weather_icon_hail);
    RUN_TEST(test_weather_icon_windy);
    RUN_TEST(test_weather_icon_windy_variant);
    RUN_TEST(test_weather_icon_lightning);
    RUN_TEST(test_weather_icon_thunderstorm);
    RUN_TEST(test_weather_icon_lightning_rainy);
    RUN_TEST(test_weather_icon_unknown_fallback);
    RUN_TEST(test_weather_icon_invalid_fallback);
    RUN_TEST(test_forecast_data_structure);
    RUN_TEST(test_forecast_condition_buffer_size);
    RUN_TEST(test_forecast_multiple_days);
    RUN_TEST(test_forecast_negative_temperatures);
    RUN_TEST(test_forecast_zero_precipitation);
    RUN_TEST(test_forecast_heavy_precipitation);
    RUN_TEST(test_icon_data_not_null);
    RUN_TEST(test_icon_size);
    
    UNITY_END();
}

void loop() {
    // Nothing
}
#endif