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

#include "display/epd_display.h"
#include "display/epd_spi.h"
#include "font_8x16.h"

#include "data.h"
#include "utils/utils.h"
#include "display/drawing.h"
#include "web/web.h"
#include "config.h"
#include "init/init.h"
#include "lang.h"

// When running unit tests, the test harness provides its own
// `fb`, `data`, `redraw_requested`, and `setup()`/`loop()` symbols.
#ifndef PIO_UNIT_TESTING
AsyncWebServer server(80);
const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;
unsigned char *fb;
unsigned long last_update_ms = 0;

extern "C" uint8_t temprature_sens_read();

RTC_DATA_ATTR uint32_t wake_count = 0;
unsigned long awake_since_ms = 0;

volatile bool update_received = false;
volatile bool sleep_after_draw = false;

DashboardData data;
volatile bool redraw_requested = false;
#endif

/* =====================================================
   SETUP
   ===================================================== */
#ifndef PIO_UNIT_TESTING
void setup()
{
  initLanguage();
  initHardware();
  initWiFi(ssid, password);
  setupWebServer(server);

  wake_count++;
  awake_since_ms = millis();

  Serial.printf("[WAKE] #%u\n", wake_count);
}
#endif

/* =====================================================
   LOOP
   ===================================================== */
#ifndef PIO_UNIT_TESTING
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
#endif