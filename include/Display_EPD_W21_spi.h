#ifndef DISPLAY_EPD_W21_SPI_H
#define DISPLAY_EPD_W21_SPI_H

#include <Arduino.h>

// Pin definitions for ESP32
#define EPD_W21_MOSI_PIN  18
#define EPD_W21_CLK_PIN   23
#define EPD_W21_CS_PIN    27
#define EPD_W21_DC_PIN    14
#define EPD_W21_RST_PIN   12
#define EPD_W21_BUSY_PIN  13

// Pin macros
#define EPD_W21_CS_0  digitalWrite(EPD_W21_CS_PIN, LOW)
#define EPD_W21_CS_1  digitalWrite(EPD_W21_CS_PIN, HIGH)
#define EPD_W21_DC_0  digitalWrite(EPD_W21_DC_PIN, LOW)
#define EPD_W21_DC_1  digitalWrite(EPD_W21_DC_PIN, HIGH)
#define EPD_W21_RST_0 digitalWrite(EPD_W21_RST_PIN, LOW)
#define EPD_W21_RST_1 digitalWrite(EPD_W21_RST_PIN, HIGH)
#define isEPD_W21_BUSY digitalRead(EPD_W21_BUSY_PIN)

void SPI_Write(unsigned char value);
void EPD_W21_WriteCMD(unsigned char command);
void EPD_W21_WriteDATA(unsigned char datas);

#endif
