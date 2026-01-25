#include "init/init.h"
#include <Arduino.h>
#include <WiFi.h>
#include <SPI.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include "display/epd_display.h"
#include "display/epd_spi.h"
#include "font_8x16.h"
#include "utils/utils.h"

extern unsigned char *fb;

void initHardware()
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
}

void initWiFi(const char* ssid, const char* password)
{
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
}