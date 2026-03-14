#include "web/web.h"
#include <ArduinoJson.h>
#include "data.h"
#include "utils/utils.h"
#include "lang.h"
#include "display/drawing.h"
#include "config.h"
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include "esp_wifi.h"

extern "C" uint8_t temprature_sens_read();

void setupWebServer(AsyncWebServer& server)
{
  /* -------- REST UPDATE -------- */
  server.on("/update", HTTP_POST, [](AsyncWebServerRequest *) {}, NULL, [](AsyncWebServerRequest *r, uint8_t *body, size_t len, size_t index, size_t total)
            {
              static String json;

              if (index == 0)
              {
                logTitle("HTTP UPDATE");
                json = "";
              }

              json += String((char *)body, len);

              if (index + len != total)
                return;

              /* RAW PAYLOAD */
              Serial.println("[RAW JSON PAYLOAD]");
              Serial.println(json);
              Serial.println();

              JsonDocument d;

              DeserializationError err = deserializeJson(d, json);
              if (err)
              {
                Serial.print("[JSON ERROR] ");
                Serial.println(err.c_str());
                r->send(400, "text/plain", "Bad JSON");
                return;
              }

              /* DATE / TIME */
              {
                const char *rawDate = d["date"] | "--";
                formatDate(rawDate, data.date, sizeof(data.date), DATE_FORMAT);
              }
              strlcpy(data.time, d["time"] | "--", sizeof(data.time));

              logKV("Date", data.date);
              logKV("Time", data.time);

              /* WEATHER */
              logTitle(lang.title_outdoor);

              data.outTemp = jf(d["weather"]["temp"]);
              data.outHum = jf(d["weather"]["humidity"]);
              data.outPress = jf(d["weather"]["pressure"]);
              data.outWind = jf(d["weather"]["wind"]);
              data.outGust = jf(d["weather"]["gust"]);
              strlcpy(data.weatherState, d["weather"]["state"] | "--", sizeof(data.weatherState));
              data.clouds = jf(d["weather"]["clouds"]);

              logKVf(lang.label_temperature, data.outTemp, "C");
              logKVf(lang.label_humidity, data.outHum, "%");
              logKVf(lang.label_pressure, data.outPress, "hPa");
              logKVf(lang.label_wind, data.outWind, "km/h");
              logKVf(lang.label_gusts, data.outGust, "km/h");
              logKVf(lang.label_clouds, data.clouds, "%");
              logKV(lang.web_state, data.weatherState);

              /* FORECAST */
              logTitle(lang.title_forecast);

              if (!d["forecast"].isNull() && d["forecast"].is<JsonArray>())
              {
                JsonArray forecastArray = d["forecast"].as<JsonArray>();

                for (int i = 0; i < 5 && i < forecastArray.size(); i++)
                {
                  strlcpy(data.forecast[i].condition, forecastArray[i]["condition"] | "unknown", sizeof(data.forecast[i].condition));
                  data.forecast[i].tempLow = jf(forecastArray[i]["temp_low"]);
                  data.forecast[i].tempHigh = jf(forecastArray[i]["temp_high"]);
                  data.forecast[i].precipitation = jf(forecastArray[i]["precipitation"]);

                  char label[16];
                  sprintf(label, "D+%d", i + 1);
                  Serial.printf(" %-18s: %-15s  %.1f - %.1f°C  %.1fmm\n",
                                label,
                                data.forecast[i].condition,
                                data.forecast[i].tempLow,
                                data.forecast[i].tempHigh,
                                data.forecast[i].precipitation);
                }
              }
              else
              {
                Serial.println(" [!] Forecast array not found or invalid");
              }

              /* ROOMS */
              logTitle(lang.title_indoor);

              data.lrT = jf(d["rooms"]["living"]["temp"]);
              data.lrH = jf(d["rooms"]["living"]["hum"]);
              logKVf(lang.room_living, data.lrT, "C");

              data.brT = jf(d["rooms"]["bedroom"]["temp"]);
              data.brH = jf(d["rooms"]["bedroom"]["hum"]);
              logKVf(lang.room_bedroom, data.brT, "C");

              data.krT = jf(d["rooms"]["kids"]["temp"]);
              data.krH = jf(d["rooms"]["kids"]["hum"]);
              logKVf(lang.room_kids, data.krT, "C");

              data.baT = jf(d["rooms"]["bath"]["temp"]);
              data.baH = jf(d["rooms"]["bath"]["hum"]);
              logKVf(lang.room_bathroom, data.baT, "C");

              data.ofT = jf(d["rooms"]["office"]["temp"]);
              data.ofH = jf(d["rooms"]["office"]["hum"]);
              logKVf(lang.room_office, data.ofT, "C");

              /* INTERNET */
              logTitle(lang.title_network);

              data.down = jf(d["internet"]["down"]);
              data.up = jf(d["internet"]["up"]);
              data.ping = jf(d["internet"]["ping"]);

              logKVf(lang.label_download, data.down, "Mbps");
              logKVf(lang.label_upload, data.up, "Mbps");
              logKVf(lang.label_ping, data.ping, "ms");

              /* POWER */
              logTitle(lang.title_electricity);

              data.powerPrice     = jf(d["power"]["price"]);
              data.powerTotal     = jf(d["power"]["total_power"]);
              data.powerVoltage   = jf(d["power"]["voltage"]);
              data.powerFrequency = jf(d["power"]["frequency"]);
              data.powerFactor    = jf(d["power"]["power_factor"]);
              data.powerMonth     = jf(d["power"]["monthly_consumption"]);

              logKVf(lang.label_price, data.powerPrice, "EUR/MWh");
              logKVf(lang.label_power, data.powerTotal, "W");
              logKVf(lang.label_voltage, data.powerVoltage, "V");
              logKVf(lang.label_frequency, data.powerFrequency, "Hz");
              logKVf(lang.label_power_factor, data.powerFactor, "");
              logKVf(lang.label_consumption, data.powerMonth, "kWh");

              /* FAMILY */
              logTitle(lang.title_location);

              for (int i = 0; i < 4; i++)
              {
                strlcpy(data.family[i].name, d["family"][i]["name"] | "?", sizeof(data.family[i].name));
                strlcpy(data.family[i].state, d["family"][i]["state"] | "?", sizeof(data.family[i].state));

                Serial.printf(" %-12s : %s\n",
                              data.family[i].name,
                              data.family[i].state);
              }

              /* FINAL STATUS */
              logTitle("UPDATE STATUS");
              Serial.printf(" Payload bytes     : %u\n", json.length());
              Serial.printf(" Free heap         : %u\n", ESP.getFreeHeap());
              Serial.printf(" Max alloc heap    : %u\n", ESP.getMaxAllocHeap());

              last_update_ms = millis();
              redraw_requested = true;
              r->send(200, "text/plain", "OK");

              /* allow TCP flush */
              delay(500);

              redraw_requested = true;
              update_received = true; });

  /* -------- FULL HTML DASHBOARD -------- */
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *r)
            {

  unsigned long age = last_update_ms ? (millis()-last_update_ms)/1000 : 0;

  String h;
  h.reserve(6000);

  h += "<!DOCTYPE html><html><head>";
  h += "<meta charset='UTF-8'>";
  h += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  h += "<style>";

  h += "body{margin:0;padding:24px;background:#04040e;"
       "color:#ece8ff;font-family:system-ui,-apple-system,BlinkMacSystemFont;"
       "background-image:radial-gradient(ellipse at 20% 50%,rgba(109,40,217,.06) 0%,transparent 60%),"
       "radial-gradient(ellipse at 80% 20%,rgba(139,92,246,.04) 0%,transparent 50%)}";

  h += "h1{font-size:1.6rem;margin:0 0 8px;"
       "background:linear-gradient(130deg,#ede9fe 0%,#a78bfa 45%,#8b5cf6 100%);"
       "-webkit-background-clip:text;-webkit-text-fill-color:transparent}";
  h += "h2{font-size:1.1rem;margin:0 0 12px;padding:0 0 10px;"
       "font-weight:700;color:#c4b5fd;"
       "border-bottom:1px solid rgba(139,92,246,.15);"
       "background:linear-gradient(90deg,rgba(109,40,217,.09) 0%,transparent 70%);"
       "margin:-16px -16px 12px;padding:12px 16px 10px;border-radius:24px 24px 0 0}";

  h += ".meta{color:#635d80;font-size:.9rem;margin-bottom:20px}";

  h += ".grid{display:grid;"
       "grid-template-columns:repeat(auto-fit,minmax(280px,1fr));"
       "gap:18px}";

  h += ".card{background:#0c0c1e;border-radius:24px;"
       "padding:16px;border:1px solid rgba(139,92,246,.10);"
       "box-shadow:0 4px 28px rgba(0,0,0,.65),0 1px 4px rgba(0,0,0,.5);"
       "transition:border-color .18s ease}";
  h += ".card:hover{border-color:rgba(139,92,246,.28);"
       "box-shadow:0 4px 28px rgba(0,0,0,.65),0 0 22px rgba(139,92,246,.12)}";

  h += ".row{display:flex;justify-content:space-between;"
       "margin:4px 0;padding:4px 0;font-size:.95rem;"
       "border-bottom:1px solid rgba(139,92,246,.05)}";
  h += ".row:last-child{border-bottom:none}";

  h += ".label{color:#a89ec8}";
  h += ".value{font-weight:500;color:#ece8ff}";

  h += ".big{font-size:1.8rem;font-weight:600;margin-bottom:6px;color:#c4b5fd}";
  h += ".unit{font-size:.9rem;color:#635d80}";

  h += ".bad{color:#f87171}";
  h += ".good{color:#4ade80}";

  h += "</style></head><body>";

  /* HEADER */
  h += "<h1>🏠 ";
  h += lang.house_name;
  h += "</h1>";
  h += "<div class='meta'>";
  h += getForecastWeekdayAbbr(0);
  h += ", "; h += data.date; h += " "; h += data.time;
  h += " · "; h += lang.web_updated_ago; h += " "; h += String(age); h += " s";
  h += "</div>";

  // Helper: value span with conditional red alert class
  auto val = [&h](float v, const char* fmt, const char* unit, bool alert) {
    h += "<span class='value";
    if (alert) h += " bad";
    h += "'>";
    h += String(v, strchr(fmt,'f') ? (fmt[strlen(fmt)-2]-'0') : 0);
    h += " "; h += unit; h += "</span></div>";
  };

  auto row = [&h](const char* label) {
    h += "<div class='row'><span class='label'>";
    h += label;
    h += "</span>";
  };

  h += "<div class='grid'>";

  /* BOX 1: OUTDOOR WEATHER */
  h += "<div class='card'><h2>🌤 ";
  h += lang.title_outdoor;
  h += "</h2>";

  h += "<div class='big";
  if (data.outTemp < -10 || data.outTemp > 30) h += " bad";
  h += "'>";
  h += String(data.outTemp,1);
  h += "<span class='unit'> °C</span></div>";

  row(lang.web_state);
  h += "<span class='value'>";
  h += translateWeatherState(data.weatherState);
  h += "</span></div>";

  row(lang.label_humidity); val(data.outHum, "%.0f", "%", false);
  row(lang.label_pressure); val(data.outPress, "%.0f", "hPa", false);
  row(lang.label_wind); val(data.outWind, "%.1f", "km/h", data.outWind > 20);
  row(lang.label_gusts); val(data.outGust, "%.1f", "km/h", data.outGust > 25);
  row(lang.label_clouds); val(data.clouds, "%.0f", "%", false);

  h += "</div>";

  /* BOX 2: INDOOR ROOMS */
  h += "<div class='card'><h2>🏠 ";
  h += lang.title_indoor;
  h += "</h2>";

  auto roomRow = [&](const char* name, float t, float hu) {
    bool alert = badTemp(t) || badHum(hu);
    h += "<div class='row'><span class='label'>";
    h += name;
    h += "</span><span class='value";
    if (alert) h += " bad";
    h += "'>";
    h += String(t,1); h += " °C / "; h += String(hu,0); h += " %</span></div>";
  };
  roomRow(lang.room_living, data.lrT, data.lrH);
  roomRow(lang.room_bedroom, data.brT, data.brH);
  roomRow(lang.room_kids, data.krT, data.krH);
  roomRow(lang.room_bathroom, data.baT, data.baH);
  roomRow(lang.room_office, data.ofT, data.ofH);
  h += "</div>";

  /* BOX 3: FORECAST */
  h += "<div class='card'><h2>📅 ";
  h += lang.title_forecast;
  h += "</h2>";
  for (int i = 0; i < 5; i++) {
    h += "<div class='row'><span class='label'><strong style='color:#c4b5fd'>";
    h += getForecastWeekdayAbbr(i + 1);
    h += "</strong> <span style='color:#635d80'>";
    h += translateWeatherState(data.forecast[i].condition);
    h += "</span></span><span class='value'>";
    h += String(data.forecast[i].tempLow, 0);
    h += "–";
    h += String(data.forecast[i].tempHigh, 0);
    h += " °C · ";
    h += String(data.forecast[i].precipitation, 1);
    h += " mm</span></div>";
  }
  h += "</div>";

  /* BOX 4: LOCATION / FAMILY */
  h += "<div class='card'><h2>📍 ";
  h += lang.title_location;
  h += "</h2>";
  for (int i = 0; i < 4; i++) {
    bool home = strcmp(data.family[i].state, "home") == 0;
    h += "<div class='row'><span class='label'>";
    h += data.family[i].name;
    h += "</span><span class='value";
    h += home ? " good" : " bad";
    h += "'>";
    h += translateFamilyState(data.family[i].state);
    h += "</span></div>";
  }
  h += "</div>";

  /* BOX 5: NETWORK */
  h += "<div class='card'><h2>📶 ";
  h += lang.title_network;
  h += "</h2>";
  h += "<div class='big";
  if (data.down < 10) h += " bad";
  h += "'>";
  h += String(data.down,1);
  h += "<span class='unit'> Mbps</span></div>";

  row(lang.web_upload); val(data.up, "%.1f", "Mbps", data.up < 5);
  row(lang.label_ping); val(data.ping, "%.0f", "ms", data.ping > 50);
  h += "</div>";

  /* BOX 6: ELECTRICITY */
  h += "<div class='card'><h2>⚡ ";
  h += lang.title_electricity;
  h += "</h2>";

  h += "<div class='big";
  if (data.powerTotal > 5000) h += " bad";
  h += "'>";
  h += String(data.powerTotal,0);
  h += "<span class='unit'> W</span></div>";

  row(lang.label_price); val(data.powerPrice, "%.2f", "€/MWh", data.powerPrice > 20);
  row(lang.label_voltage); val(data.powerVoltage, "%.0f", "V", data.powerVoltage < 220 || data.powerVoltage > 240);
  row(lang.label_frequency); val(data.powerFrequency, "%.2f", "Hz", data.powerFrequency < 48 || data.powerFrequency > 52);

  row("cosφ");
  h += "<span class='value";
  if (data.powerFactor < 70) h += " bad";
  h += "'>";
  h += String(data.powerFactor,2);
  h += "</span></div>";

  h += "<div class='row'><span class='label'>";
  h += lang.web_monthly;
  h += "</span><span class='value'>";
  h += String(data.powerMonth,1);
  h += " kWh</span></div>";

  h += "</div>";

  /* BOX 7: ESP32 SYSTEM STATS */
  float espTemp = (temprature_sens_read() - 32) / 1.8;
  uint32_t cpuFreq = getCpuFrequencyMhz();
  size_t heapFree = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  size_t heapLargest = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  float heapFrag = heapFree
      ? 100.0f * (1.0f - (float)heapLargest / heapFree)
      : 0;
  size_t psramFree = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

  wifi_ap_record_t ap;
  int wifiChan = 0;
  if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK)
    wifiChan = ap.primary;

  h += "<div class='card'><h2>🧠 ";
  h += lang.web_system;
  h += "</h2>";

  h += "<div class='row'><span class='label'>CPU temp</span><span class='value'>";
  h += String(espTemp,1);
  h += " °C</span></div>";

  h += "<div class='row'><span class='label'>CPU freq</span><span class='value'>";
  h += String(cpuFreq);
  h += " MHz</span></div>";

  h += "<div class='row'><span class='label'>Heap free</span><span class='value'>";
  h += String(heapFree/1024);
  h += " KB</span></div>";

  h += "<div class='row'><span class='label'>Heap frag</span><span class='value'>";
  h += String(heapFrag,1);
  h += " %</span></div>";

  h += "<div class='row'><span class='label'>PSRAM free</span><span class='value'>";
  h += String(psramFree/1024);
  h += " KB</span></div>";

  h += "<div class='row'><span class='label'>WiFi SSID</span><span class='value'>";
  h += WiFi.SSID();
  h += "</span></div>";

  h += "<div class='row'><span class='label'>RSSI</span><span class='value";
  if (WiFi.RSSI() < -75) h += " bad";
  h += "'>";
  h += String(WiFi.RSSI());
  h += " dBm</span></div>";

  h += "<div class='row'><span class='label'>WiFi ch</span><span class='value'>";
  h += String(wifiChan);
  h += "</span></div>";

  h += "<div class='row'><span class='label'>Uptime</span><span class='value'>";
  h += String(millis()/1000);
  h += " s</span></div>";

  h += "</div>";

  h += "</div></body></html>";

  r->send(200, "text/html", h); });

  server.begin();
}
