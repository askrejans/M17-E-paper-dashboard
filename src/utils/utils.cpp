#include "utils/utils.h"
#include <Arduino.h>
#include <WiFi.h>

float jf(JsonVariant v)
{
  if (v.is<float>())
    return v.as<float>();
  if (v.is<const char *>())
    return atof(v.as<const char *>());
  return 0;
}

void goToSleepSeconds(uint32_t sec)
{
  Serial.println("[POWER] Deep sleep");
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(100);

  esp_sleep_enable_timer_wakeup((uint64_t)sec * 1000000ULL);
  esp_deep_sleep_start();
}

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

void logKVf(const char *k, float v, const char *u)
{
  Serial.printf(" %-16s : %.2f %s\n", k, v, u);
}