#include <unity.h>
#include <string>
#include "fixtures.h"
#include "lang.h"

static DashboardData d;

void test_payload_v2_full(void)
{
  TEST_ASSERT_TRUE(loadPayload(SAMPLE_V2, d));
  TEST_ASSERT_TRUE(d.valid);
  TEST_ASSERT_EQUAL_STRING("19.10.2026", d.date);
  TEST_ASSERT_EQUAL_STRING("14:31", d.time);
  TEST_ASSERT_EQUAL(1, d.weekday);
  TEST_ASSERT_EQUAL_STRING("partlycloudy", d.weather.state);
  TEST_ASSERT_EQUAL_FLOAT(9.7f, d.weather.temp);
  TEST_ASSERT_EQUAL(3, d.roomCount);
  TEST_ASSERT_EQUAL_STRING("Guļamistaba", d.rooms[1].name);
  TEST_ASSERT_EQUAL(3, d.familyCount);
  TEST_ASSERT_EQUAL_STRING("Ķengarags", d.family[2].state);
  TEST_ASSERT_EQUAL(2, d.forecastCount);
  TEST_ASSERT_EQUAL_FLOAT(4.2f, d.forecast[0].precipitation);
  TEST_ASSERT_EQUAL_FLOAT(6.4f, d.power.today);
  TEST_ASSERT_EQUAL(4, d.prices.count);
  TEST_ASSERT_EQUAL(2, d.prices.now);
  TEST_ASSERT_EQUAL(3, d.hourly.count);
  TEST_ASSERT_EQUAL(15, d.hourly.startHour);
  TEST_ASSERT_EQUAL_FLOAT(1.6f, d.hourly.precip[2]);
  TEST_ASSERT_EQUAL(1, d.eventCount);
  TEST_ASSERT_EQUAL_STRING("Football", d.events[0].title);
  TEST_ASSERT_EQUAL(5, d.house.lightsOn);
  TEST_ASSERT_EQUAL_STRING("Garage door", d.house.open[0]);
  TEST_ASSERT_EQUAL(0, d.house.lowBattery);
  TEST_ASSERT_EQUAL_STRING("04:58", d.sunrise); // ISO -> HH:MM
  TEST_ASSERT_EQUAL_STRING("18:12", d.sunset);
  TEST_ASSERT_EQUAL_STRING("Hello", d.message);
  TEST_ASSERT_EQUAL(1, d.extraCount);
  TEST_ASSERT_EQUAL_FLOAT(21.5f, d.thermostat.target);
  TEST_ASSERT_EQUAL_STRING("heating", d.thermostat.action);
}

void test_payload_v1_legacy_rooms(void)
{
  TEST_ASSERT_TRUE(loadPayload(SAMPLE_V1, d, "YYYY-MM-DD"));
  TEST_ASSERT_EQUAL_STRING("2026-03-14", d.date);
  TEST_ASSERT_EQUAL_FLOAT(18.5f, d.weather.temp); // numeric string accepted
  TEST_ASSERT_EQUAL(6, d.roomCount);
  TEST_ASSERT_EQUAL_STRING("@living", d.rooms[0].name); // historic order, translated at render
  TEST_ASSERT_EQUAL_STRING("@office", d.rooms[4].name);
  TEST_ASSERT_EQUAL_STRING("Garage", d.rooms[5].name);  // unknown key capitalised
  setLanguage("LV");
  TEST_ASSERT_EQUAL_STRING("Viesistaba", translateName(d.rooms[0].name));
  setLanguage("EN");
  TEST_ASSERT_EQUAL_STRING("Living Room", translateName(d.rooms[0].name));
  TEST_ASSERT_EQUAL(-1, d.hourly.startHour);
  TEST_ASSERT_EQUAL(-1, d.house.lightsOn); // unknown, not zero
}

void test_payload_missing_and_unavailable(void)
{
  TEST_ASSERT_TRUE(loadPayload(R"({"weather":{"temp":"unavailable","humidity":null}})", d));
  TEST_ASSERT_TRUE(isnan(d.weather.temp));
  TEST_ASSERT_TRUE(isnan(d.weather.hum));
  TEST_ASSERT_TRUE(isnan(d.power.total));
  TEST_ASSERT_EQUAL(0, d.roomCount);
  TEST_ASSERT_EQUAL(-1, d.weekday);
  TEST_ASSERT_EQUAL_STRING("--:--", d.time);
  JsonDocument arr;
  deserializeJson(arr, "[1]");
  TEST_ASSERT_FALSE(parsePayload(arr.as<JsonVariantConst>(), d, "DD.MM.YYYY"));
}

void test_payload_capacity_limits(void)
{
  JsonDocument doc;
  for (int i = 0; i < 20; i++)
  {
    doc["rooms"][i]["name"] = "R";
    doc["family"][i]["name"] = "P";
    doc["forecast"][i]["condition"] = "sunny";
    doc["extras"][i]["label"] = "E";
    doc["events"][i]["title"] = "T";
  }
  for (int i = 0; i < 100; i++)
  {
    doc["prices"][i] = i;
    doc["hourly"]["temp"][i] = i;
  }
  TEST_ASSERT_TRUE(parsePayload(doc.as<JsonVariantConst>(), d, "DD.MM.YYYY"));
  TEST_ASSERT_EQUAL(MAX_ROOMS, d.roomCount);
  TEST_ASSERT_EQUAL(MAX_FAMILY, d.familyCount);
  TEST_ASSERT_EQUAL(MAX_FORECAST, d.forecastCount);
  TEST_ASSERT_EQUAL(MAX_EXTRAS, d.extraCount);
  TEST_ASSERT_EQUAL(MAX_EVENTS, d.eventCount);
  TEST_ASSERT_EQUAL(MAX_SERIES, d.prices.count);
  TEST_ASSERT_EQUAL(MAX_HOURLY, d.hourly.count);
}

void test_payload_long_strings_truncated(void)
{
  TEST_ASSERT_TRUE(loadPayload(R"({"weather":{"state":"an-extremely-long-weather-condition-name"},
    "family":[{"name":"Bartholomew-Maximilian","state":"x"}],"message":"m"})", d));
  TEST_ASSERT_EQUAL(sizeof(d.weather.state) - 1, strlen(d.weather.state));
  TEST_ASSERT_EQUAL(sizeof(d.family[0].name) - 1, strlen(d.family[0].name));
}

void test_payload_utf8_strings_truncated(void)
{
  // Each valid code point crosses the byte limit, as long HA labels can.
  const char *glyphs[] = {"ā", "€", "🙂"};
  for (const char *glyph : glyphs)
    for (size_t cut = 1; cut < strlen(glyph); cut++)
    {
      std::string name(19 - cut, 'a');
      std::string room(23 - cut, 'b');
      std::string title(39 - cut, 'c');
      std::string message(95 - cut, 'd');
      JsonDocument input;
      input["house"]["low_battery"][0] = name + glyph + "end";
      input["house"]["open"][0] = name + glyph + "end";
      input["rooms"][0]["name"] = room + glyph + "end";
      input["events"][0]["title"] = title + glyph + "end";
      input["message"] = message + glyph + "end";
      TEST_ASSERT_TRUE(parsePayload(input.as<JsonVariantConst>(), d, "DD.MM.YYYY"));
      TEST_ASSERT_EQUAL_STRING(name.c_str(), d.house.battery[0]);
      TEST_ASSERT_EQUAL_STRING(name.c_str(), d.house.open[0]);
      TEST_ASSERT_EQUAL_STRING(room.c_str(), d.rooms[0].name);
      TEST_ASSERT_EQUAL_STRING(title.c_str(), d.events[0].title);
      TEST_ASSERT_EQUAL_STRING(message.c_str(), d.message);
      JsonDocument state, roundtrip;
      dataToJson(d, state.to<JsonObject>());
      std::string json;
      serializeJson(state, json);
      TEST_ASSERT_FALSE(deserializeJson(roundtrip, json));
      TEST_ASSERT_EQUAL_STRING(name.c_str(), roundtrip["house"]["low_battery"][0].as<const char *>());
      TEST_ASSERT_EQUAL_STRING(message.c_str(), roundtrip["message"].as<const char *>());

      // Legacy room object keys have their own fixed-buffer copy path.
      input.clear();
      input["rooms"][room + glyph + "end"]["temp"] = 20;
      TEST_ASSERT_TRUE(parsePayload(input.as<JsonVariantConst>(), d, "DD.MM.YYYY"));
      room[0] = 'B';
      TEST_ASSERT_EQUAL_STRING(room.c_str(), d.rooms[0].name);
    }

  TEST_ASSERT_TRUE(loadPayload(R"({"house":{"low_battery":["Terases gaismas slēdzis","abcdefghijklmnopqā"]}})", d));
  TEST_ASSERT_EQUAL_STRING("Terases gaismas sl", d.house.battery[0]);
  TEST_ASSERT_EQUAL_STRING("abcdefghijklmnopqā", d.house.battery[1]); // complete glyph fits exactly
}

void test_payload_prices_variants(void)
{
  TEST_ASSERT_TRUE(loadPayload(R"({"prices":[0.1,0.2,0.3]})", d));
  TEST_ASSERT_EQUAL(3, d.prices.count);
  TEST_ASSERT_EQUAL(0, d.prices.now);
  // Nord Pool "raw" objects + out-of-range now clamps to last
  TEST_ASSERT_TRUE(loadPayload(R"({"prices":{"now":99,"values":[{"value":0.5},{"value":0.7}]}})", d));
  TEST_ASSERT_EQUAL_FLOAT(0.7f, d.prices.v[1]);
  TEST_ASSERT_EQUAL(1, d.prices.now);
}

void test_payload_forecast_native_ha_names(void)
{
  TEST_ASSERT_TRUE(loadPayload(R"({"forecast":[{"condition":"rainy","temperature":12,"templow":5,"precipitation":1},
    {"condition":"cloudy","temperature":8}]})", d));
  TEST_ASSERT_EQUAL_FLOAT(12, d.forecast[0].tempHigh);
  TEST_ASSERT_EQUAL_FLOAT(5, d.forecast[0].tempLow);
  TEST_ASSERT_EQUAL_FLOAT(8, d.forecast[1].tempLow); // missing low -> high
}

void test_ha_forecast_skips_today(void)
{
  JsonDocument doc;
  deserializeJson(doc, R"([{"datetime":"2026-10-19T00:00:00+03:00","condition":"sunny","temperature":10},
    {"datetime":"2026-10-20T00:00:00+03:00","condition":"rainy","temperature":9,"templow":3,"precipitation":2}])");
  dataReset(d);
  parseHaForecast(doc.as<JsonArrayConst>(), d, "2026-10-19");
  TEST_ASSERT_EQUAL(1, d.forecastCount);
  TEST_ASSERT_EQUAL_STRING("rainy", d.forecast[0].condition);
  TEST_ASSERT_EQUAL_FLOAT(3, d.forecast[0].tempLow);
}

void test_ha_hourly_list(void)
{
  JsonDocument doc;
  deserializeJson(doc, R"([{"datetime":"2026-10-19T15:00:00+03:00","temperature":9.5,"precipitation":0},
    {"datetime":"2026-10-19T16:00:00+03:00","temperature":9.1,"precipitation":0.4}])");
  dataReset(d);
  parseHaHourly(doc.as<JsonArrayConst>(), d);
  TEST_ASSERT_EQUAL(2, d.hourly.count);
  TEST_ASSERT_EQUAL(15, d.hourly.startHour);
  TEST_ASSERT_EQUAL_FLOAT(0.4f, d.hourly.precip[1]);
}

void test_data_to_json_roundtrip(void)
{
  DashboardData a, b;
  TEST_ASSERT_TRUE(loadPayload(SAMPLE_V2, a, "YYYY-MM-DD"));
  JsonDocument doc;
  dataToJson(a, doc.to<JsonObject>());
  TEST_ASSERT_TRUE(parsePayload(doc.as<JsonVariantConst>(), b, "YYYY-MM-DD"));
  TEST_ASSERT_EQUAL_STRING(a.date, b.date);
  TEST_ASSERT_EQUAL_FLOAT(a.weather.gust, b.weather.gust);
  TEST_ASSERT_EQUAL(a.roomCount, b.roomCount);
  TEST_ASSERT_EQUAL(a.prices.count, b.prices.count);
  TEST_ASSERT_EQUAL(a.prices.now, b.prices.now);
  TEST_ASSERT_EQUAL(a.hourly.count, b.hourly.count);
  TEST_ASSERT_EQUAL_STRING(a.house.media, b.house.media);
  TEST_ASSERT_EQUAL_STRING(a.sunrise, b.sunrise);
  TEST_ASSERT_TRUE(isnan(b.weather.clouds) == isnan(a.weather.clouds));
}

void test_jf_number_parsing(void)
{
  JsonDocument doc;
  deserializeJson(doc, R"({"a":22.5,"b":"18.5","c":"invalid","d":null,"e":7})");
  TEST_ASSERT_EQUAL_FLOAT(22.5f, jf(doc["a"]));
  TEST_ASSERT_EQUAL_FLOAT(18.5f, jf(doc["b"]));
  TEST_ASSERT_TRUE(isnan(jf(doc["c"])));
  TEST_ASSERT_TRUE(isnan(jf(doc["d"])));
  TEST_ASSERT_EQUAL_FLOAT(7, jf(doc["e"]));
  TEST_ASSERT_TRUE(isnan(jf(doc["missing"])));
}
