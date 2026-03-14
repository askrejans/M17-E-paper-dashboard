#include <Arduino.h>
#include <WiFi.h>
#include <SPI.h>
#include <esp_heap_caps.h>
#include <esp_system.h>

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

#include "display/epd_driver.h"
#include "display/epd_spi.h"
#include "font_8x16.h"

#include "data.h"
#include "utils/utils.h"
#include "display/drawing.h"
#include "web/web.h"
#include "config.h"
#include "init/init.h"
#include "lang.h"

#ifndef PIO_UNIT_TESTING
AsyncWebServer server(80);
const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;
unsigned char *fb;
unsigned long last_update_ms = 0;

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
static void showError(const char *line1, const char *line2)
{
  fill(WHITE);
  fillRect(0, 0, 800, 40, BLACK);
  text(20, 12, "M17", WHITE);
  text(200, 12, "ERROR", RED);
  text(100, 200, line1, BLACK);
  if (line2)
    text(100, 230, line2, BLACK);
  EPD_init();
  PIC_display(fb);
  EPD_sleep();
}

void setup()
{
  initLanguage();
  initHardware();

  if (!initWiFi(ssid, password))
  {
    Serial.println("[ERROR] WiFi failed - showing error on display");
    showError("WiFi: nav savienojuma!",
              "Parbaudi .env failu (SSID/PASSWORD)");
    Serial.println("[POWER] Retrying in 60s...");
    delay(200);
    goToSleepSeconds(60);
    return; // never reached
  }

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