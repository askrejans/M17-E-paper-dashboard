#include <unity.h>
#include "fixtures.h"
#include <string>

void test_settings_defaults(void)
{
  Settings s;
  settingsDefaults(s);
  TEST_ASSERT_EQUAL_STRING("M17", s.name);
  TEST_ASSERT_EQUAL_STRING("classic", s.theme);
  TEST_ASSERT_EQUAL_STRING("admin", s.adminUser);
  TEST_ASSERT_EQUAL_STRING("", s.adminPass); // admin panel open until a password is set
  TEST_ASSERT_EQUAL(15, s.intervalMin);
  TEST_ASSERT_EQUAL(POWER_ALWAYS_ON, s.powerMode);
  TEST_ASSERT_EQUAL(SOURCE_PUSH, s.sourceMode);
  TEST_ASSERT_EQUAL(-1, s.quietStart);
  TEST_ASSERT_TRUE(s.map.autoDiscover);
  TEST_ASSERT_EQUAL_STRING("", s.map.normalOpenings);
  TEST_ASSERT_TRUE(s.skipUnchanged);
  TEST_ASSERT_FALSE(s.fastRefresh);
  const char *slots[] = {"outdoor", "indoor", "forecast", "family", "network", "power"};
  for (int i = 0; i < DASH_SLOTS; i++)
    TEST_ASSERT_EQUAL_STRING(slots[i], s.slots[i]);
}

void test_settings_roundtrip(void)
{
  Settings a, b;
  settingsDefaults(a);
  TEST_ASSERT_TRUE(applySettings(a, R"({"device":{"name":"Home","language":"LV","admin_user":"root","admin_password":"secret1"},
    "wifi":{"ssid":"net","password":"pw"},"power":{"mode":"always_on","interval_min":30,"quiet_start":23,"quiet_end":6,"timezone":"EET-2"},
    "source":{"mode":"pull","ha_url":"http://ha:8123/","ha_token":"tok","map":{"weather":"weather.home",
      "rooms":[{"name":"Living","temp":"sensor.t","hum":"sensor.h"}],"family":[{"name":"","entity":"person.a"}],
      "extras":[{"name":"CO2","entity":"sensor.co2"}],"calendars":"calendar.x","normal_openings":"binary_sensor.study_door, binary_sensor.hall_door","auto_discover":false}},
    "display":{"theme":"vfd","slots":["hourly","house","calendar","prices","extras","system"],"refresh":"fast","skip_unchanged":false},
    "thresholds":{"price_high":0.3,"rssi_low":-80}})"));
  JsonDocument doc;
  settingsToJson(a, doc.to<JsonObject>(), false);
  settingsDefaults(b);
  TEST_ASSERT_TRUE(settingsFromJson(b, doc.as<JsonVariantConst>()));
  JsonDocument doc2;
  settingsToJson(b, doc2.to<JsonObject>(), false);
  std::string j1, j2;
  serializeJson(doc, j1);
  serializeJson(doc2, j2);
  TEST_ASSERT_EQUAL_STRING(j1.c_str(), j2.c_str());
  TEST_ASSERT_EQUAL_STRING("http://ha:8123", b.haUrl); // trailing slash trimmed
  TEST_ASSERT_EQUAL(1, b.map.roomCount);
  TEST_ASSERT_EQUAL_STRING("sensor.h", b.map.rooms[0].b);
  TEST_ASSERT_EQUAL_STRING("binary_sensor.study_door, binary_sensor.hall_door", b.map.normalOpenings);
  TEST_ASSERT_EQUAL(23, b.quietStart);
  TEST_ASSERT_TRUE(b.fastRefresh);
  TEST_ASSERT_EQUAL_FLOAT(0.3f, b.th.priceHigh);
}

void test_settings_masks_secrets(void)
{
  Settings s;
  settingsDefaults(s);
  strlcpy(s.haToken, "tok", sizeof(s.haToken));
  strlcpy(s.adminPass, "pw", sizeof(s.adminPass));
  JsonDocument doc;
  settingsToJson(s, doc.to<JsonObject>(), true);
  TEST_ASSERT_EQUAL_STRING(SECRET_MASK, doc["source"]["ha_token"]);
  TEST_ASSERT_EQUAL_STRING(SECRET_MASK, doc["device"]["admin_password"]);
  TEST_ASSERT_EQUAL_STRING("", doc["source"]["push_token"]); // unset stays empty
  // Sending the mask back must not overwrite the stored secret
  TEST_ASSERT_TRUE(settingsFromJson(s, doc.as<JsonVariantConst>()));
  TEST_ASSERT_EQUAL_STRING("tok", s.haToken);
  TEST_ASSERT_EQUAL_STRING("pw", s.adminPass);
  // ...while an explicit empty string clears it
  TEST_ASSERT_TRUE(applySettings(s, R"({"device":{"admin_password":""}})"));
  TEST_ASSERT_EQUAL_STRING("", s.adminPass);
}

void test_settings_partial_update(void)
{
  Settings s;
  settingsDefaults(s);
  TEST_ASSERT_TRUE(applySettings(s, R"({"display":{"theme":"terminal"}})"));
  TEST_ASSERT_EQUAL_STRING("terminal", s.theme);
  TEST_ASSERT_EQUAL_STRING("M17", s.name);           // untouched
  TEST_ASSERT_EQUAL_STRING("outdoor", s.slots[0]);   // untouched
  TEST_ASSERT_TRUE(applySettings(s, R"({"source":{"map":{"normal_openings":"binary_sensor.study_door"}}})"));
  TEST_ASSERT_TRUE(applySettings(s, R"({"source":{"map":{"auto_discover":false}}})"));
  TEST_ASSERT_EQUAL_STRING("binary_sensor.study_door", s.map.normalOpenings); // omitted value is preserved
  TEST_ASSERT_TRUE(applySettings(s, R"({"source":{"map":{"normal_openings":""}}})"));
  TEST_ASSERT_EQUAL_STRING("", s.map.normalOpenings); // explicit empty restores default counts
}

void test_settings_validation(void)
{
  Settings s;
  settingsDefaults(s);
  TEST_ASSERT_TRUE(applySettings(s, R"({"display":{"theme":"nope","slots":["outdoor","bogus"]},
    "device":{"language":"DE","date_format":"YY"},"power":{"interval_min":0,"listen_window_s":5,"quiet_start":42}})"));
  TEST_ASSERT_EQUAL_STRING("classic", s.theme);       // unknown theme rejected
  TEST_ASSERT_EQUAL_STRING("none", s.slots[1]);       // unknown widget -> none
  TEST_ASSERT_EQUAL_STRING("none", s.slots[5]);       // missing slots -> none
  TEST_ASSERT_EQUAL_STRING("EN", s.language);
  TEST_ASSERT_EQUAL_STRING("DD.MM.YYYY", s.dateFormat);
  TEST_ASSERT_EQUAL(1, s.intervalMin);                // clamped
  TEST_ASSERT_EQUAL(30, s.listenWindowSec);
  TEST_ASSERT_EQUAL(23, s.quietStart);
  TEST_ASSERT_TRUE(s.wakeBeforeSec < s.intervalMin * 60);
  TEST_ASSERT_FALSE(applySettings(s, "[1,2]"));       // not an object
}

void test_settings_binding_limits(void)
{
  Settings s;
  settingsDefaults(s);
  TEST_ASSERT_TRUE(applySettings(s, R"({"source":{"map":{"rooms":[
    {"name":"1"},{"name":"2"},{"name":"3"},{"name":"4"},{"name":"5"},{"name":"6"},{"name":"7"},{"name":"8"},{"name":"9"},
    {"name":"","temp":""}]}}})"));
  TEST_ASSERT_EQUAL(MAX_ROOMS, s.map.roomCount);
  TEST_ASSERT_TRUE(applySettings(s, R"({"source":{"map":{"rooms":[{"name":"","temp":"","hum":""}]}}})"));
  TEST_ASSERT_EQUAL(0, s.map.roomCount); // empty rows dropped
}

void test_widget_and_theme_registry(void)
{
  TEST_ASSERT_TRUE(isWidgetId("hourly"));
  TEST_ASSERT_TRUE(isWidgetId("none"));
  TEST_ASSERT_FALSE(isWidgetId("weather"));
  TEST_ASSERT_TRUE(isThemeId("vfd"));
  TEST_ASSERT_FALSE(isThemeId("frontpanel"));
  TEST_ASSERT_EQUAL(4, THEME_COUNT);
}
