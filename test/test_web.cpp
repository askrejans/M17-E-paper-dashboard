#include <unity.h>
#include <ArduinoJson.h>
#include "data.h"
#include "utils/utils.h"
#include <string.h>

void test_web_json_parsing(void) {
    JsonDocument d;

    // Minimal payload similar to REST /update
    d["date"] = "2026-01-25";
    d["time"] = "12:34";
    d["weather"]["temp"] = 5.5;
    d["weather"]["humidity"] = 60;
    d["rooms"]["living"]["temp"] = 21.0;
    d["rooms"]["living"]["hum"] = 45;
    d["internet"]["down"] = 50.2;
    d["power"]["price"] = 10.5;
    d["family"][0]["name"] = "Alice";
    d["family"][0]["state"] = "home";

    // Apply parsing logic (same as in web handler)
    extern DashboardData data;
    strcpy(data.date, d["date"] | "--");
    strcpy(data.time, d["time"] | "--");

    data.outTemp = jf(d["weather"]["temp"]);
    data.outHum = jf(d["weather"]["humidity"]);

    data.lrT = jf(d["rooms"]["living"]["temp"]);
    data.lrH = jf(d["rooms"]["living"]["hum"]);

    data.down = jf(d["internet"]["down"]);
    data.powerPrice = jf(d["power"]["price"]);

    strcpy(data.family[0].name, d["family"][0]["name"] | "?");
    strcpy(data.family[0].state, d["family"][0]["state"] | "?");

    TEST_ASSERT_EQUAL_STRING("2026-01-25", data.date);
    TEST_ASSERT_EQUAL_FLOAT(5.5f, data.outTemp);
    TEST_ASSERT_EQUAL_FLOAT(21.0f, data.lrT);
    TEST_ASSERT_EQUAL_FLOAT(50.2f, data.down);
    TEST_ASSERT_EQUAL_STRING("Alice", data.family[0].name);
}
