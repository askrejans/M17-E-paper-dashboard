#include <unity.h>
#include "fixtures.h"
#include "ha_template.h"
#include <string>

static bool contains(const std::string &s, const char *needle) { return s.find(needle) != std::string::npos; }

static int count(const std::string &s, const char *needle)
{
  int n = 0;
  for (size_t p = s.find(needle); p != std::string::npos; p = s.find(needle, p + 1))
    n++;
  return n;
}

void test_valid_entity_id(void)
{
  TEST_ASSERT_TRUE(validEntityId("sensor.living_temp"));
  TEST_ASSERT_TRUE(validEntityId("weather.home"));
  TEST_ASSERT_FALSE(validEntityId(""));
  TEST_ASSERT_FALSE(validEntityId(nullptr));
  TEST_ASSERT_FALSE(validEntityId("sensor"));
  TEST_ASSERT_FALSE(validEntityId(".x"));
  TEST_ASSERT_FALSE(validEntityId("sensor."));
  TEST_ASSERT_FALSE(validEntityId("sensor.a.b"));
  TEST_ASSERT_FALSE(validEntityId("Sensor.Upper"));
  TEST_ASSERT_FALSE(validEntityId("sensor.x') }}{{ evil"));
}

void test_template_uses_mapping(void)
{
  Settings s;
  settingsDefaults(s);
  applySettings(s, R"({"source":{"map":{"weather":"weather.forecast_home","out_temp":"sensor.out_t",
    "rooms":[{"name":"Living \"A\"","temp":"sensor.lt","hum":"sensor.lh"}],
    "family":[{"name":"","entity":"person.alice"}],"price":"sensor.nordpool","price_series":"sensor.nordpool",
    "extras":[{"name":"","entity":"sensor.co2"}],"calendars":"calendar.family, calendar.work, bad id"}}})");
  std::string t = buildHaTemplate(s.map);
  TEST_ASSERT_TRUE(contains(t, "states('sensor.out_t')") || contains(t, "states(\"sensor.out_t\")"));
  TEST_ASSERT_TRUE(contains(t, "\"weather.forecast_home\""));
  TEST_ASSERT_TRUE(contains(t, "Living \\\"A\\\""));               // quotes escaped in Jinja literal
  TEST_ASSERT_TRUE(contains(t, "state_attr(\"person.alice\", 'friendly_name')"));
  TEST_ASSERT_TRUE(contains(t, "\"calendar.family\", \"calendar.work\"]"));
  TEST_ASSERT_FALSE(contains(t, "bad id"));
  TEST_ASSERT_TRUE(contains(t, "\"prices\""));
  TEST_ASSERT_TRUE(contains(t, "\"house\""));                      // auto-discover on by default
  TEST_ASSERT_TRUE(contains(t, "\"weather_entity\""));
  TEST_ASSERT_TRUE(contains(t, "\"v\": 2}"));
  // balanced Jinja blocks
  TEST_ASSERT_EQUAL(count(t, "{%"), count(t, "%}"));
  TEST_ASSERT_EQUAL(count(t, "{{ "), count(t, " }}"));
  TEST_ASSERT_EQUAL(count(t, "{% for"), count(t, "{% endfor"));
  TEST_ASSERT_EQUAL(count(t, "{% if"), count(t, "{% endif"));
}

void test_template_invalid_entities_become_null(void)
{
  Settings s;
  settingsDefaults(s);
  applySettings(s, R"({"source":{"map":{"net_down":"not an entity","power":"sensor.p"}}})");
  std::string t = buildHaTemplate(s.map);
  TEST_ASSERT_TRUE(contains(t, "\"down\": null"));
  TEST_ASSERT_TRUE(contains(t, "states(\"sensor.p\")"));
}

void test_template_auto_discovery_toggle(void)
{
  Settings s;
  settingsDefaults(s);
  applySettings(s, R"({"source":{"map":{"weather":"","auto_discover":true}}})");
  std::string on = buildHaTemplate(s.map);
  TEST_ASSERT_TRUE(contains(on, "states.weather"));
  TEST_ASSERT_TRUE(contains(on, "states.person"));
  TEST_ASSERT_TRUE(contains(on, "states.calendar"));
  TEST_ASSERT_TRUE(contains(on, "states.climate"));       // thermostat auto-discovery
  applySettings(s, R"({"source":{"map":{"auto_discover":false}}})");
  std::string off = buildHaTemplate(s.map);
  TEST_ASSERT_FALSE(contains(off, "states.weather"));
  TEST_ASSERT_FALSE(contains(off, "states.person"));
  TEST_ASSERT_FALSE(contains(off, "\"house\""));
  TEST_ASSERT_TRUE(contains(off, "\"temp\": null"));
}

void test_template_normal_openings_mapping(void)
{
  Settings s;
  settingsDefaults(s);
  TEST_ASSERT_EQUAL_STRING("", s.map.normalOpenings);
  const std::string original = buildHaTemplate(s.map);
  applySettings(s, R"({"source":{"map":{"normal_openings":"binary_sensor.office_door, binary_sensor.bedroom_door, sensor.not_contact, bad id, binary_sensor.x') }}{{ evil"}}})");
  const std::string configured = buildHaTemplate(s.map);
  TEST_ASSERT_TRUE(contains(configured, "binary_sensor.office_door"));
  TEST_ASSERT_TRUE(contains(configured, "binary_sensor.bedroom_door"));
  TEST_ASSERT_FALSE(contains(configured, "sensor.not_contact"));
  TEST_ASSERT_FALSE(contains(configured, "bad id"));
  TEST_ASSERT_FALSE(contains(configured, "evil"));
  JsonDocument saved;
  settingsToJson(s, saved.to<JsonObject>(), true);
  Settings restored;
  settingsDefaults(restored);
  TEST_ASSERT_TRUE(settingsFromJson(restored, saved.as<JsonVariantConst>()));
  TEST_ASSERT_EQUAL_STRING(s.map.normalOpenings, restored.map.normalOpenings);
  TEST_ASSERT_EQUAL_STRING(configured.c_str(), buildHaTemplate(restored.map).c_str());

  applySettings(s, R"({"source":{"map":{"normal_openings":""}}})");
  TEST_ASSERT_EQUAL_STRING(original.c_str(), buildHaTemplate(s.map).c_str());
  applySettings(s, R"({"source":{"map":{"normal_openings":"sensor.not_contact, bad id"}}})");
  TEST_ASSERT_EQUAL_STRING(original.c_str(), buildHaTemplate(s.map).c_str());
}

void test_entity_search_template(void)
{
  std::string t = buildEntitySearchTemplate("Living", "sensor", 10);
  TEST_ASSERT_TRUE(contains(t, "\"living\""));     // lower-cased
  TEST_ASSERT_TRUE(contains(t, "\"sensor\""));
  TEST_ASSERT_TRUE(contains(t, "loop.index <= 10"));
  TEST_ASSERT_EQUAL_STRING("[]", buildEntitySearchTemplate("x", "bad domain!", 10).c_str());
  std::string q = buildEntitySearchTemplate("a\"b", "", 5);
  TEST_ASSERT_TRUE(contains(q, "a\\\"b"));
}
