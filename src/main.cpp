#include <Arduino.h>
#include <WiFi.h>
#include <SPI.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include "driver/temperature_sensor.h"
#include "esp_wifi.h"

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

#include "Display_EPD_W21.h"
#include "Display_EPD_W21_spi.h"
#include "font_8x16.h"

/* =====================================================
   CONFIG
   ===================================================== */
const char *ssid = "YOUR_WIFI_SSID";
const char *password = "YOUR_WIFI_PASSWORD";

AsyncWebServer server(80);
unsigned char *fb;
unsigned long last_update_ms = 0;

extern "C" uint8_t temprature_sens_read();

#define UPDATE_INTERVAL_MIN 15
#define WAKE_BEFORE_SEC 90    // wake 90s early
#define LISTEN_WINDOW_SEC 180 // stay awake max 3 min

#define US_PER_SEC 1000000ULL
#define US_PER_MIN (60ULL * US_PER_SEC)

RTC_DATA_ATTR uint32_t wake_count = 0;
unsigned long awake_since_ms = 0;

volatile bool update_received = false;
volatile bool sleep_after_draw = false;

/* =====================================================
   COLORS
   ===================================================== */
enum
{
  BLACK = 0,
  WHITE = 1,
  YELLOW = 2,
  RED = 3
};

/* =====================================================
   DATA MODEL
   ===================================================== */
struct Person
{
  char name[16];
  char state[12];
};

struct DashboardData
{
  char date[16];
  char time[8];
  char weatherState[24];

  float outTemp, outHum, outPress, outWind, outGust, clouds;
  float lrT, lrH, brT, brH, krT, krH, baT, baH, ofT, ofH;
  float down, up, ping;
  float powerPrice, powerTotal, powerVoltage, powerFrequency, powerFactor, powerMonth;

  Person family[4];
};

DashboardData data;
volatile bool redraw_requested = false;

/* =====================================================
   UTIL
   ===================================================== */
float jf(JsonVariant v)
{
  if (v.is<float>())
    return v.as<float>();
  if (v.is<const char *>())
    return atof(v.as<const char *>());
  return 0;
}

bool badTemp(float t) { return t < 19 || t > 25; }
bool badHum(float h) { return h < 35 || h > 60; }

static inline const char *familyStateLV(const char *s)
{
  if (strcmp(s, "home") == 0)
    return "Mājās";
  if (strcmp(s, "not_home") == 0)
    return "Prom";
  return s; // fallback, don’t hide unknown states
}

void goToSleepSeconds(uint32_t sec)
{
  Serial.println("[POWER] Deep sleep");
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(100);

  esp_sleep_enable_timer_wakeup((uint64_t)sec * US_PER_SEC);
  esp_deep_sleep_start();
}

/* =====================================================
   DRAW PRIMITIVES
   ===================================================== */

void text(int x, int y, const char *s, int c);

inline void px(int x, int y, int c)
{
  if (x < 0 || y < 0 || x >= 800 || y >= 480)
    return;
  uint32_t i = y * 800 + x;
  uint32_t b = i >> 2;
  uint8_t s = (3 - (i & 3)) * 2;
  fb[b] = (fb[b] & ~(3 << s)) | (c << s);
}

void fill(int c)
{
  uint8_t v = (c == WHITE) ? 0x55 : 0x00;
  memset(fb, v, ALLSCREEN_BYTES);
}

void fillRect(int x, int y, int w, int h, int c)
{
  for (int yy = y; yy < y + h; yy++)
    for (int xx = x; xx < x + w; xx++)
      px(xx, yy, c);
}

void rect(int x, int y, int w, int h)
{
  for (int i = x; i < x + w; i++)
  {
    px(i, y, BLACK);
    px(i, y + h - 1, BLACK);
  }
  for (int i = y; i < y + h; i++)
  {
    px(x, i, BLACK);
    px(x + w - 1, i, BLACK);
  }
}

void rectWithTitle(int x, int y, int w, int h, const char *title)
{
  const int title_h = 24;

  // outer border
  rect(x, y, w, h);

  // title bar
  fillRect(x + 1, y + 1, w - 2, title_h, BLACK);

  // title text (vertically centered in bar)
  text(x + 8, y + 5, title, YELLOW);
}

struct Utf8Map
{
  uint16_t utf8;
  uint8_t glyph;
};

static const Utf8Map utf8_map[] = {
    // Ā ā
    {0xC480, 0x80}, // Ā
    {0xC481, 0x81}, // ā

    // Č č
    {0xC48C, 0x82}, // Č
    {0xC48D, 0x83}, // č

    // Ē ē
    {0xC492, 0x84}, // Ē
    {0xC493, 0x85}, // ē

    // Ģ ģ
    {0xC4A2, 0x86}, // Ģ
    {0xC4A3, 0x87}, // ģ

    // Ī ī
    {0xC4AA, 0x88}, // Ī
    {0xC4AB, 0x89}, // ī

    // Ļ ļ
    {0xC4BB, 0x8A}, // Ļ
    {0xC4BC, 0x8B}, // ļ

    // Ņ ņ
    {0xC585, 0x8C}, // Ņ
    {0xC586, 0x8D}, // ņ

    // Š š
    {0xC5A0, 0x8E}, // Š
    {0xC5A1, 0x8F}, // š

    // Ū ū
    {0xC5AA, 0x90}, // Ū
    {0xC5AB, 0x91}, // ū

    // Ž ž
    {0xC5BD, 0x92}, // Ž
    {0xC5BE, 0x93}, // ž

    {0xC2B0, 0x94}, // °
};

static inline uint8_t next_char(const char **s)
{
  uint8_t c0 = (uint8_t)(*s)[0];

  // ASCII
  if (c0 < 0x80)
  {
    (*s)++;
    return c0;
  }

  // 2-byte UTF-8
  if ((c0 & 0xE0) == 0xC0)
  {
    uint16_t u = (c0 << 8) | (uint8_t)(*s)[1];
    (*s) += 2;

    for (unsigned i = 0; i < sizeof(utf8_map) / sizeof(utf8_map[0]); i++)
      if (utf8_map[i].utf8 == u)
        return utf8_map[i].glyph;

    return '?';
  }

  // 3-byte UTF-8 (EURO)
  if ((c0 & 0xF0) == 0xE0)
  {
    uint32_t u =
        ((uint32_t)c0 << 16) |
        ((uint32_t)(*s)[1] << 8) |
        (uint32_t)(*s)[2];

    (*s) += 3;

    // € = E2 82 AC
    if (u == 0xE282AC)
      return 0x96; // € glyph

    return '?';
  }

  // unsupported
  (*s)++;
  return '?';
}

void text(int x, int y, const char *s, int c)
{
  while (*s)
  {
    uint8_t ch = next_char(&s);
    const uint8_t *g = font8x16[ch];

    for (int r = 0; r < 16; r++)
    {
      uint8_t b = g[r];
      for (int i = 0; i < 8; i++)
        if (b & (0x80 >> i))
          px(x + i, y + r, c);
    }
    x += 8;
  }
}

/* =====================================================
   LOGGING HELPERS
   ===================================================== */
void logTitle(const char *t)
{
  Serial.println();
  Serial.println("=================================================");
  Serial.print(" ");
  Serial.println(t);
  Serial.println("=================================================");
}

void logKV(const char *k, const char *v)
{
  Serial.printf(" %-16s : %s\n", k, v);
}

void logKVf(const char *k, float v, const char *u = "")
{
  Serial.printf(" %-16s : %.2f %s\n", k, v, u);
}

/* =====================================================
   DASHBOARD DRAW
   ===================================================== */
void draw_dashboard()
{
  fill(WHITE);

  fillRect(0, 0, 800, 40, BLACK);

  char header[48];
  sprintf(header, "     %s               atjaunots @ %s", data.date, data.time);
  int header_x = (800 - strlen(header) * 8) / 2;

  text(20, 12, "M17", WHITE);
  text(header_x, 12, header, YELLOW);

  char b[64];

  rectWithTitle(10, 50, 260, 200, "Ārā");

  int y = 80;

  /* Temperatūra */
  sprintf(b, "Temperatūra  %.1f°C", data.outTemp);
  text(20, y, b, (data.outTemp < -10 || data.outTemp > 30) ? RED : BLACK);
  y += 18;

  /* Mitrums */
  sprintf(b, "Mitrums       %.0f%%", data.outHum);
  text(20, y, b, BLACK);
  y += 18;

  /* Gaisa spiediens */
  sprintf(b, "Gaisa spied.  %.0f hPa", data.outPress);
  text(20, y, b, BLACK);
  y += 18;

  /* Vēja stiprums */
  sprintf(b, "Vējš          %.1f km/h", data.outWind);
  text(20, y, b, (data.outWind > 20) ? RED : BLACK);
  y += 18;

  /* Brāzmās */
  sprintf(b, "Brāzmās       %.1f km/h", data.outGust);
  text(20, y, b, (data.outGust > 25) ? RED : BLACK);
  y += 18;

  /* Mākoņu daudzums */
  sprintf(b, "Mākoņi        %.0f %%", data.clouds);
  text(20, y, b, BLACK);
  y += 18;

  /* Stāvoklis */
  sprintf(b, "Apstākļi      %s", data.weatherState);
  text(20, y, b, BLACK);

  rectWithTitle(280, 50, 260, 200, "Telpās");

  y = 80;

  /* Header */
  text(290, y, "              Temp    Mitr", BLACK);
  y += 18;
  /* Dzīvojamā istaba */
  sprintf(b, "Viesistaba    %.1f°C  %.0f%%", data.lrT, data.lrH);
  text(290, y, b, (badTemp(data.lrT) || badHum(data.lrH)) ? RED : BLACK);
  y += 18;

  /* Guļamistaba */
  sprintf(b, "Guļamistaba   %.1f°C  %.0f%%", data.brT, data.brH);
  text(290, y, b, (badTemp(data.brT) || badHum(data.brH)) ? RED : BLACK);
  y += 18;

  /* Bērnu istaba */
  sprintf(b, "Bērnu istaba  %.1f°C  %.0f%%", data.krT, data.krH);
  text(290, y, b, (badTemp(data.krT) || badHum(data.krH)) ? RED : BLACK);
  y += 18;

  /* Vannas istaba */
  sprintf(b, "Vannasistaba  %.1f°C  %.0f%%", data.baT, data.baH);
  text(290, y, b, badHum(data.baH) ? RED : BLACK);
  y += 18;

  /* Ofiss */
  sprintf(b, "Ofiss         %.1f°C  %.0f%%", data.ofT, data.ofH);
  text(290, y, b, (badTemp(data.ofT) || badHum(data.ofH)) ? RED : BLACK);

  rectWithTitle(10, 270, 260, 180, "Atrašanās vieta");

  for (int i = 0; i < 4; i++)
  {
    const char *st = familyStateLV(data.family[i].state);

    sprintf(b, "%s: %s", data.family[i].name, st);
    text(20, 300 + i * 20, b,
         strcmp(data.family[i].state, "home") == 0 ? BLACK : RED);
  }

  rectWithTitle(280, 270, 260, 180, "Tīkls");

  sprintf(b, "Down %.2f Mbps", data.down);
  text(290, 300, b, (data.down < 10) ? RED : BLACK);
  sprintf(b, "Up   %.2f Mbps", data.up);
  text(290, 320, b, (data.up < 5) ? RED : BLACK);
  sprintf(b, "Ping %.0f ms", data.ping);
  text(290, 340, b, (data.ping > 50) ? RED : BLACK);

  rectWithTitle(560, 270, 230, 180, "Elektrība");

  int py = 300;

  sprintf(b, "Cena     %.2f €/MWh", data.powerPrice);
  text(570, py, b, (data.powerPrice > 20) ? RED : BLACK);
  py += 18;

  sprintf(b, "Jauda    %.0f W", data.powerTotal);
  text(570, py, b, (data.powerTotal > 5000) ? RED : BLACK);
  py += 18;

  sprintf(b, "Sprieg.  %.0f V", data.powerVoltage);
  text(570, py, b, (data.powerVoltage < 220 || data.powerVoltage > 240) ? RED : BLACK);
  py += 18;

  sprintf(b, "Frekv.   %.2f Hz", data.powerFrequency);
  text(570, py, b, (data.powerFrequency < 48 || data.powerFrequency > 52) ? RED : BLACK);
  py += 18;

  sprintf(b, "PF       %.2f", data.powerFactor);
  text(570, py, b, (data.powerFactor < 70) ? RED : BLACK);
  py += 18;

  sprintf(b, "Patēriņš %.1f kWh", data.powerMonth);
  text(570, py, b, (data.powerMonth > 320) ? RED : BLACK);

  EPD_init();
  PIC_display(fb);
  EPD_sleep();
}

/* =====================================================
   SETUP
   ===================================================== */
void setup()
{
  Serial.begin(115200);
  delay(600);

  logTitle("BOOT");

  Serial.printf(" ESP32 Chip        : %s\n", ESP.getChipModel());
  Serial.printf(" Cores             : %d\n", ESP.getChipCores());
  Serial.printf(" Flash             : %u MB\n", ESP.getFlashChipSize() / 1024 / 1024);
  Serial.printf(" Heap free         : %u\n", ESP.getFreeHeap());
  Serial.printf(" Max alloc heap    : %u\n", ESP.getMaxAllocHeap());

  fb = (unsigned char *)heap_caps_malloc(ALLSCREEN_BYTES, MALLOC_CAP_SPIRAM);
  if (!fb)
  {
    Serial.println(" [MEM] PSRAM failed → using heap");
    fb = (unsigned char *)malloc(ALLSCREEN_BYTES);
  }

  font8x16_init();

  Serial.printf(" Framebuffer       : %p\n", fb);

  SPI.begin();
  Serial.println(" SPI               : OK");

  pinMode(EPD_W21_BUSY_PIN, INPUT);
  pinMode(EPD_W21_RST_PIN, OUTPUT);
  pinMode(EPD_W21_DC_PIN, OUTPUT);
  pinMode(EPD_W21_CS_PIN, OUTPUT);
  digitalWrite(EPD_W21_CS_PIN, HIGH);
  digitalWrite(EPD_W21_RST_PIN, HIGH);

  WiFi.begin(ssid, password);
  Serial.print(" WiFi              : connecting");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(300);
    Serial.print(".");
  }
  Serial.println(" OK");
  Serial.print(" IP                : ");
  Serial.println(WiFi.localIP());

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

  wake_count++;
  awake_since_ms = millis();

  Serial.printf("[WAKE] #%u\n", wake_count);
}

/* =====================================================
   LOOP
   ===================================================== */
void loop()
{
  if (redraw_requested)
  {
    redraw_requested = false;

    Serial.println("[DRAW] Updating display");
    draw_dashboard();

    if (update_received)
    {
      update_received = false;

      uint32_t sleep_sec =
          UPDATE_INTERVAL_MIN * 60 - WAKE_BEFORE_SEC;

      Serial.printf("[POWER] Sleeping for %u sec\n", sleep_sec);
      delay(200); // let UART flush
      goToSleepSeconds(sleep_sec);
    }
  }

  // allow background tasks (AsyncTCP, WiFi, etc.)
  delay(1);
}