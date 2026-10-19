#include "display/epd_spi.h"
#include <SPI.h>

// SPI write byte (the bus is configured once in EPD_spiBegin)
void SPI_Write(unsigned char value)
{
  SPI.transfer(value);
}

void EPD_spiBegin(void)
{
  SPI.begin(EPD_W21_CLK_PIN, -1, EPD_W21_MOSI_PIN, -1);
  SPI.setFrequency(EPD_SPI_HZ);
  pinMode(EPD_W21_BUSY_PIN, INPUT);
  pinMode(EPD_W21_RST_PIN, OUTPUT);
  pinMode(EPD_W21_DC_PIN, OUTPUT);
  pinMode(EPD_W21_CS_PIN, OUTPUT);
  digitalWrite(EPD_W21_CS_PIN, HIGH);
  digitalWrite(EPD_W21_RST_PIN, HIGH);
}

//SPI write command
void EPD_W21_WriteCMD(unsigned char command)
{
	EPD_W21_CS_0;
	EPD_W21_DC_0;  // D/C#   0:command  1:data  
	SPI_Write(command);
	EPD_W21_CS_1;
}

//SPI write data
void EPD_W21_WriteDATA(unsigned char datas)
{
	EPD_W21_CS_0;
	EPD_W21_DC_1;  // D/C#   0:command  1:data
	SPI_Write(datas);
	EPD_W21_CS_1;
}
