#ifndef DISPLAY_EPD_W21_SPI_H
#define DISPLAY_EPD_W21_SPI_H

#include <Arduino.h>

// Pin definitions for ESP32
// SPI uses the ESP32 VSPI defaults the panel has always run on.
// (Older docs listed MOSI/CLK swapped; these are the pins actually driven.)
#define EPD_W21_CLK_PIN   18
#define EPD_W21_MOSI_PIN  23
#define EPD_W21_CS_PIN    27
#define EPD_W21_DC_PIN    14
#define EPD_W21_RST_PIN   12
#define EPD_W21_BUSY_PIN  13

// SPI clock for the panel (override with -DEPD_SPI_HZ=...)
#ifndef EPD_SPI_HZ
#define EPD_SPI_HZ 4000000
#endif

// Pin macros
#define EPD_W21_CS_0  digitalWrite(EPD_W21_CS_PIN, LOW)
#define EPD_W21_CS_1  digitalWrite(EPD_W21_CS_PIN, HIGH)
#define EPD_W21_DC_0  digitalWrite(EPD_W21_DC_PIN, LOW)
#define EPD_W21_DC_1  digitalWrite(EPD_W21_DC_PIN, HIGH)
#define EPD_W21_RST_0 digitalWrite(EPD_W21_RST_PIN, LOW)
#define EPD_W21_RST_1 digitalWrite(EPD_W21_RST_PIN, HIGH)
#define isEPD_W21_BUSY digitalRead(EPD_W21_BUSY_PIN)

void EPD_spiBegin(void);
void SPI_Write(unsigned char value);
void EPD_W21_WriteCMD(unsigned char command);
void EPD_W21_WriteDATA(unsigned char datas);

#endif
