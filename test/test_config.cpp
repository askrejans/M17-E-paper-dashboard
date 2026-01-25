#include <unity.h>
#include "config.h"
#include "display/epd_display.h"
#include "display/drawing.h"
#include "font_8x16.h"
#include "utils/utils.h"
#include "data.h"
#include <ArduinoJson.h>

// Drawing tests (defined in test_drawing.cpp)
void test_px_pack(void);
void test_fillRect_and_pack(void);
void test_rect_border(void);
void test_rectWithTitle_changes_titlebar(void);

// More tests
void test_font_init_nonzero(void);
void test_text_utf8_latvian(void);
void test_text_degree_symbol(void);
void test_web_json_parsing(void);
void test_console_log_functions(void);

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

void test_familyStateLV(void) {
    TEST_ASSERT_EQUAL_STRING("Mājās", familyStateLV("home"));
    TEST_ASSERT_EQUAL_STRING("Prom", familyStateLV("not_home"));
    TEST_ASSERT_EQUAL_STRING("unknown", familyStateLV("unknown"));
    TEST_ASSERT_EQUAL_STRING("", familyStateLV(""));
    TEST_ASSERT_EQUAL_STRING("away", familyStateLV("away"));
    TEST_ASSERT_EQUAL_STRING("Mājās", familyStateLV("home"));
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
    UNITY_BEGIN();
    RUN_TEST(test_config_constants);
    RUN_TEST(test_color_get);
    RUN_TEST(test_jf);
    RUN_TEST(test_badTemp);
    RUN_TEST(test_badHum);
    RUN_TEST(test_familyStateLV);
    RUN_TEST(test_badTemp_boundary);
    RUN_TEST(test_badHum_boundary);
    RUN_TEST(test_fill);
    // drawing tests
    RUN_TEST(test_px_pack);
    RUN_TEST(test_fillRect_and_pack);
    RUN_TEST(test_rect_border);
    RUN_TEST(test_rectWithTitle_changes_titlebar);
    RUN_TEST(test_font_init_nonzero);
    RUN_TEST(test_text_utf8_latvian);
    RUN_TEST(test_text_degree_symbol);
    RUN_TEST(test_web_json_parsing);
    RUN_TEST(test_console_log_functions);
    UNITY_END();
}

void loop() {
    // Nothing
}
#endif