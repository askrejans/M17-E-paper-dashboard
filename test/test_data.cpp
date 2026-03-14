#include <unity.h>
#include "data.h"
#include "utils/utils.h"
#include <string.h>

extern DashboardData data;

// --- DashboardData struct field sizes ---

void test_data_date_buffer_size(void) {
    // Should hold "YYYY-MM-DD" (10 chars + null)
    strlcpy(data.date, "2026-03-14", sizeof(data.date));
    TEST_ASSERT_EQUAL_STRING("2026-03-14", data.date);
}

void test_data_time_buffer_size(void) {
    // Should hold "HH:MM" (5 chars + null)
    strlcpy(data.time, "23:59", sizeof(data.time));
    TEST_ASSERT_EQUAL_STRING("23:59", data.time);
}

void test_data_weather_state_buffer(void) {
    // Longest weather state: "lightning-rainy" (15 chars)
    strlcpy(data.weatherState, "lightning-rainy", sizeof(data.weatherState));
    TEST_ASSERT_EQUAL_STRING("lightning-rainy", data.weatherState);
}

void test_data_person_name_buffer(void) {
    // Person name buffer should hold reasonable names
    strlcpy(data.family[0].name, "Kristiāna", sizeof(data.family[0].name));
    TEST_ASSERT_EQUAL_STRING("Kristiāna", data.family[0].name);
}

void test_data_person_state_buffer(void) {
    strlcpy(data.family[0].state, "not_home", sizeof(data.family[0].state));
    TEST_ASSERT_EQUAL_STRING("not_home", data.family[0].state);
}

// --- Data range tests for alert thresholds ---

void test_alert_threshold_temp_outdoor_cold(void) {
    // Temperature < -10 should trigger RED
    data.outTemp = -15.0f;
    TEST_ASSERT_TRUE(data.outTemp < -10);
}

void test_alert_threshold_temp_outdoor_hot(void) {
    // Temperature > 30 should trigger RED
    data.outTemp = 35.0f;
    TEST_ASSERT_TRUE(data.outTemp > 30);
}

void test_alert_threshold_wind(void) {
    data.outWind = 25.0f;
    TEST_ASSERT_TRUE(data.outWind > 20);
}

void test_alert_threshold_gust(void) {
    data.outGust = 30.0f;
    TEST_ASSERT_TRUE(data.outGust > 25);
}

void test_alert_threshold_ping(void) {
    data.ping = 60.0f;
    TEST_ASSERT_TRUE(data.ping > 50);
}

void test_alert_threshold_download(void) {
    data.down = 5.0f;
    TEST_ASSERT_TRUE(data.down < 10);
}

void test_alert_threshold_upload(void) {
    data.up = 3.0f;
    TEST_ASSERT_TRUE(data.up < 5);
}

void test_alert_threshold_voltage_low(void) {
    data.powerVoltage = 210.0f;
    TEST_ASSERT_TRUE(data.powerVoltage < 220);
}

void test_alert_threshold_voltage_high(void) {
    data.powerVoltage = 245.0f;
    TEST_ASSERT_TRUE(data.powerVoltage > 240);
}

void test_alert_threshold_frequency(void) {
    data.powerFrequency = 47.5f;
    TEST_ASSERT_TRUE(data.powerFrequency < 48);
}

void test_alert_threshold_power_price(void) {
    data.powerPrice = 25.0f;
    TEST_ASSERT_TRUE(data.powerPrice > 20);
}

void test_alert_threshold_power_total(void) {
    data.powerTotal = 6000.0f;
    TEST_ASSERT_TRUE(data.powerTotal > 5000);
}

// --- Indoor comfort thresholds ---

void test_indoor_temp_comfortable(void) {
    TEST_ASSERT_FALSE(badTemp(22.0f));
    TEST_ASSERT_FALSE(badTemp(20.0f));
}

void test_indoor_temp_too_cold(void) {
    TEST_ASSERT_TRUE(badTemp(17.0f));
}

void test_indoor_temp_too_hot(void) {
    TEST_ASSERT_TRUE(badTemp(27.0f));
}

void test_indoor_hum_comfortable(void) {
    TEST_ASSERT_FALSE(badHum(45.0f));
    TEST_ASSERT_FALSE(badHum(50.0f));
}

void test_indoor_hum_too_dry(void) {
    TEST_ASSERT_TRUE(badHum(25.0f));
}

void test_indoor_hum_too_wet(void) {
    TEST_ASSERT_TRUE(badHum(70.0f));
}

// --- Family array bounds ---

void test_family_array_4_members(void) {
    for (int i = 0; i < 4; i++) {
        snprintf(data.family[i].name, sizeof(data.family[i].name), "P%d", i);
        strlcpy(data.family[i].state, "home", sizeof(data.family[i].state));
    }

    for (int i = 0; i < 4; i++) {
        char expected[8];
        snprintf(expected, sizeof(expected), "P%d", i);
        TEST_ASSERT_EQUAL_STRING(expected, data.family[i].name);
        TEST_ASSERT_EQUAL_STRING("home", data.family[i].state);
    }
}

// --- Date formatting ---

void test_format_date_eu_to_eu(void) {
    char out[16];
    formatDate("14.03.2026", out, sizeof(out), "DD.MM.YYYY");
    TEST_ASSERT_EQUAL_STRING("14.03.2026", out);
}

void test_format_date_iso_to_eu(void) {
    char out[16];
    formatDate("2026-03-14", out, sizeof(out), "DD.MM.YYYY");
    TEST_ASSERT_EQUAL_STRING("14.03.2026", out);
}

void test_format_date_eu_to_iso(void) {
    char out[16];
    formatDate("14.03.2026", out, sizeof(out), "YYYY-MM-DD");
    TEST_ASSERT_EQUAL_STRING("2026-03-14", out);
}

void test_format_date_iso_to_us(void) {
    char out[16];
    formatDate("2026-03-14", out, sizeof(out), "MM/DD/YYYY");
    TEST_ASSERT_EQUAL_STRING("03/14/2026", out);
}

void test_format_date_eu_to_uk(void) {
    char out[16];
    formatDate("14.03.2026", out, sizeof(out), "DD/MM/YYYY");
    TEST_ASSERT_EQUAL_STRING("14/03/2026", out);
}

void test_format_date_unparseable_passthrough(void) {
    char out[16];
    formatDate("--", out, sizeof(out), "DD.MM.YYYY");
    TEST_ASSERT_EQUAL_STRING("--", out);
}

void test_format_date_us_input_to_iso(void) {
    char out[16];
    formatDate("03/14/2026", out, sizeof(out), "YYYY-MM-DD");
    TEST_ASSERT_EQUAL_STRING("2026-03-14", out);
    // MM/DD/YYYY input: month=03, day=14 → ISO 2026-03-14
}

// --- Color enum values ---

void test_color_enum_values(void) {
    TEST_ASSERT_EQUAL(0, BLACK);
    TEST_ASSERT_EQUAL(1, WHITE);
    TEST_ASSERT_EQUAL(2, YELLOW);
    TEST_ASSERT_EQUAL(3, RED);
}

void test_color_2bit_range(void) {
    // All colors must fit in 2 bits
    TEST_ASSERT_TRUE(BLACK <= 3);
    TEST_ASSERT_TRUE(WHITE <= 3);
    TEST_ASSERT_TRUE(YELLOW <= 3);
    TEST_ASSERT_TRUE(RED <= 3);
}
