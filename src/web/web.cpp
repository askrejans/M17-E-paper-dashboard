#include "web/web.h"
#include <ArduinoJson.h>
#include "data.h"
#include "utils/utils.h"
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include "driver/temperature_sensor.h"
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
              strcpy(data.date, d["date"] | "--");
              strcpy(data.time, d["time"] | "--");

              logKV("Date", data.date);
              logKV("Time", data.time);

              /* WEATHER */
              logTitle("WEATHER");

              data.outTemp = jf(d["weather"]["temp"]);
              data.outHum = jf(d["weather"]["humidity"]);
              data.outPress = jf(d["weather"]["pressure"]);
              data.outWind = jf(d["weather"]["wind"]);
              data.outGust = jf(d["weather"]["gust"]);
              strcpy(data.weatherState, d["weather"]["state"] | "--");
              data.clouds = jf(d["weather"]["clouds"]);

              logKVf("Temp", data.outTemp, "C");
              logKVf("Humidity", data.outHum, "%");
              logKVf("Pressure", data.outPress, "hPa");
              logKVf("Wind", data.outWind, "km/h");
              logKVf("Gust", data.outGust, "km/h");

              /* ROOMS */
              logTitle("ROOMS");

              data.lrT = jf(d["rooms"]["living"]["temp"]);
              data.lrH = jf(d["rooms"]["living"]["hum"]);
              logKVf("Livingroom Temp", data.lrT, "C");
              logKVf("Livingroom Hum", data.lrH, "%");

              data.brT = jf(d["rooms"]["bedroom"]["temp"]);
              data.brH = jf(d["rooms"]["bedroom"]["hum"]);
              logKVf("Bedroom Temp", data.brT, "C");
              logKVf("Bedroom Hum", data.brH, "%");

              data.krT = jf(d["rooms"]["kids"]["temp"]);
              data.krH = jf(d["rooms"]["kids"]["hum"]);
              logKVf("Kids room Temp", data.krT, "C");
              logKVf("Kids room Hum", data.krH, "%");

              data.baT = jf(d["rooms"]["bath"]["temp"]);
              data.baH = jf(d["rooms"]["bath"]["hum"]);
              logKVf("Bathroom Temp", data.baT, "C");
              logKVf("Bathroom Hum", data.baH, "%");

              data.ofT = jf(d["rooms"]["office"]["temp"]);
              data.ofH = jf(d["rooms"]["office"]["hum"]);
              logKVf("Office Temp", data.ofT, "C");
              logKVf("Office Hum", data.ofH, "%");

              /* INTERNET */
              logTitle("INTERNET");

              data.down = jf(d["internet"]["down"]);
              data.up = jf(d["internet"]["up"]);
              data.ping = jf(d["internet"]["ping"]);

              logKVf("Download", data.down, "Mbps");
              logKVf("Upload", data.up, "Mbps");
              logKVf("Ping", data.ping, "ms");

              /* POWER */
              logTitle("ELECTRICITY");

              data.powerPrice     = jf(d["power"]["price"]);
              data.powerTotal     = jf(d["power"]["total_power"]);
              data.powerVoltage   = jf(d["power"]["voltage"]);
              data.powerFrequency = jf(d["power"]["frequency"]);
              data.powerFactor    = jf(d["power"]["power_factor"]);
              data.powerMonth     = jf(d["power"]["monthly_consumption"]);

              logKVf("Price", data.powerPrice, "EUR/MWh");
              logKVf("Total power", data.powerTotal, "W");
              logKVf("Voltage", data.powerVoltage, "V");
              logKVf("Frequency", data.powerFrequency, "Hz");
              logKVf("Power factor", data.powerFactor, "");
              logKVf("Month usage", data.powerMonth, "kWh");

              /* FAMILY */
              logTitle("FAMILY");

              for (int i = 0; i < 4; i++)
              {
                strcpy(data.family[i].name, d["family"][i]["name"] | "?");
                strcpy(data.family[i].state, d["family"][i]["state"] | "?");

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
  h.reserve(5000);   // prevent truncation / heap fragmentation

  h += "<!DOCTYPE html><html><head>";
  h += "<meta charset='UTF-8'>";
  h += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  h += "<style>";

  h += "body{margin:0;padding:16px;background:#0f1115;"
       "color:#e6e6e6;font-family:system-ui,-apple-system,BlinkMacSystemFont}";

  h += "h1{font-size:1.6rem;margin:0 0 8px}";
  h += "h2{font-size:1.1rem;margin:0 0 12px;font-weight:500;color:#cfd3ff}";

  h += ".meta{color:#8b8f9c;font-size:.9rem;margin-bottom:16px}";

  h += ".grid{display:grid;"
       "grid-template-columns:repeat(auto-fit,minmax(260px,1fr));"
       "gap:16px}";

  h += ".card{background:#181b22;border-radius:14px;"
       "padding:16px;box-shadow:0 6px 18px rgba(0,0,0,.45)}";

  h += ".row{display:flex;justify-content:space-between;"
       "margin:6px 0;font-size:.95rem}";

  h += ".label{color:#9aa0ad}";
  h += ".value{font-weight:500}";

  h += ".big{font-size:1.8rem;font-weight:600;margin-bottom:6px}";
  h += ".unit{font-size:.9rem;color:#9aa0ad}";

  h += ".bad{color:#ff6b6b}";
  h += ".good{color:#51cf66}";

  h += "</style></head><body>";

  /* HEADER */
  h += "<h1>🏠 M17</h1>";
  h += "<div class='meta'>";
  h += data.date; h += " "; h += data.time;
  h += " · updated "; h += String(age); h += " s ago";
  h += "</div>";

  h += "<div class='grid'>";

  /* WEATHER */
  h += "<div class='card'><h2>🌤 Laikapstākļi</h2>";

  h += "<div class='big'>";
  h += String(data.outTemp,1);
  h += "<span class='unit'> °C</span></div>";

  h += "<div class='row'><span class='label'>Stāvoklis</span><span class='value'>";
  h += data.weatherState;
  h += "</span></div>";

  h += "<div class='row'><span class='label'>Mitrums</span><span class='value'>";
  h += String(data.outHum,0);
  h += " %</span></div>";

  h += "<div class='row'><span class='label'>Gaisa spiediens</span><span class='value'>";
  h += String(data.outPress,0);
  h += " hPa</span></div>";

  h += "<div class='row'><span class='label'>Vēja stiprums</span><span class='value'>";
  h += String(data.outWind,1);
  h += " km/h</span></div>";

  h += "<div class='row'><span class='label'>Brāzmās</span><span class='value'>";
  h += String(data.outGust,1);
  h += " km/h</span></div>";

  h += "<div class='row'><span class='label'>Mākoņu daudzums</span><span class='value'>";
  h += String(data.clouds,0);
  h += " %</span></div>";

  h += "</div>";

  /* ROOMS */
  h += "<div class='card'><h2>🏠 Telpas</h2>";
  h += "<div class='row'><span class='label'>Viesistaba</span><span class='value'>";
  h += String(data.lrT,1); h += " °C / "; h += String(data.lrH,0); h += " %</span></div>";
  h += "<div class='row'><span class='label'>Guļamistaba</span><span class='value'>";
  h += String(data.brT,1); h += " °C / "; h += String(data.brH,0); h += " %</span></div>";
  h += "<div class='row'><span class='label'>Bērnu istaba</span><span class='value'>";
  h += String(data.krT,1); h += " °C / "; h += String(data.krH,0); h += " %</span></div>";
  h += "<div class='row'><span class='label'>Vannas istaba</span><span class='value'>";
  h += String(data.baT,1); h += " °C / "; h += String(data.baH,0); h += " %</span></div>";
  h += "<div class='row'><span class='label'>Ofiss</span><span class='value'>";
  h += String(data.ofT,1); h += " °C / "; h += String(data.ofH,0); h += " %</span></div>";
  h += "</div>";

  /* INTERNET */
  h += "<div class='card'><h2>📶 Tīkls</h2>";
  h += "<div class='big'>";
  h += String(data.down,1);
  h += "<span class='unit'> Mbps</span></div>";
  h += "<div class='row'><span class='label'>Upload</span><span class='value'>";
  h += String(data.up,1); h += " Mbps</span></div>";
  h += "<div class='row'><span class='label'>Ping</span><span class='value'>";
  h += String(data.ping,0); h += " ms</span></div>";
  h += "</div>";

  /* ELECTRICITY */
  h += "<div class='card'><h2>⚡ Elektrība</h2>";

  h += "<div class='big'>";
  h += String(data.powerTotal,0);
  h += "<span class='unit'> W</span></div>";

  h += "<div class='row'><span class='label'>Cena</span><span class='value'>";
  h += String(data.powerPrice,2);
  h += " €/MWh</span></div>";

  h += "<div class='row'><span class='label'>Spriegums</span><span class='value'>";
  h += String(data.powerVoltage,0);
  h += " V</span></div>";

  h += "<div class='row'><span class='label'>Frekvence</span><span class='value'>";
  h += String(data.powerFrequency,2);
  h += " Hz</span></div>";

  h += "<div class='row'><span class='label'>Cosφ</span><span class='value'>";
  h += String(data.powerFactor,2);
  h += "</span></div>";

  h += "<div class='row'><span class='label'>Mēnesī</span><span class='value'>";
  h += String(data.powerMonth,1);
  h += " kWh</span></div>";

  h += "</div>";

  /* ESP32 */
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

  h += "<div class='card'><h2>🧠 ESP32</h2>";

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

  h += "<div class='row'><span class='label'>RSSI</span><span class='value'>";
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