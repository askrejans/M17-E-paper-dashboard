#include "display/epd_spi.h"
#include "display/epd_display.h"
#include <SPI.h>

// Init sequence from GoodDisplay / GxEPD2. The fast waveform (spec: ~12 s
// instead of ~22 s) is selected by forcing the LUT temperature (E0/E6/A5)
// *before* the booster is powered on with 0x04.
static void EPD_init_common(bool fast)
{
  delay(100);
  EPD_W21_RST_0;
  delay(10);
  EPD_W21_RST_1;
  delay(10);
  lcd_chkstatus();

  EPD_W21_WriteCMD(0x00);
  EPD_W21_WriteDATA(0x0F);
  EPD_W21_WriteDATA(0x29);

  EPD_W21_WriteCMD(0x06);
  EPD_W21_WriteDATA(0x0F);
  EPD_W21_WriteDATA(0x8B);
  EPD_W21_WriteDATA(0x93);
  EPD_W21_WriteDATA(0xa1);

  EPD_W21_WriteCMD(0x41);
  EPD_W21_WriteDATA(0x00);

  EPD_W21_WriteCMD(0x50);
  EPD_W21_WriteDATA(0x37);

  EPD_W21_WriteCMD(0x60);
  EPD_W21_WriteDATA(0x02);
  EPD_W21_WriteDATA(0x02);

  EPD_W21_WriteCMD(0x61);
  EPD_W21_WriteDATA(Source_BITS / 256);
  EPD_W21_WriteDATA(Source_BITS % 256);
  EPD_W21_WriteDATA(Gate_BITS / 256);
  EPD_W21_WriteDATA(Gate_BITS % 256);

  EPD_W21_WriteCMD(0x62);
  EPD_W21_WriteDATA(0x98);
  EPD_W21_WriteDATA(0x98);
  EPD_W21_WriteDATA(0x98);
  EPD_W21_WriteDATA(0x75);
  EPD_W21_WriteDATA(0xCA);
  EPD_W21_WriteDATA(0xB2);
  EPD_W21_WriteDATA(0x98);
  EPD_W21_WriteDATA(0x7E);

  EPD_W21_WriteCMD(0x65);
  EPD_W21_WriteDATA(0x00);
  EPD_W21_WriteDATA(0x00);
  EPD_W21_WriteDATA(0x00);
  EPD_W21_WriteDATA(0x00);

  EPD_W21_WriteCMD(0xE7);
  EPD_W21_WriteDATA(0x1C);

  EPD_W21_WriteCMD(0xE3);
  EPD_W21_WriteDATA(0x00);

  EPD_W21_WriteCMD(0xE9);
  EPD_W21_WriteDATA(0x01);

  EPD_W21_WriteCMD(0x30);
  EPD_W21_WriteDATA(0x08);

  if (fast)
  {
    EPD_W21_WriteCMD(0xE0); // cascade setting: use forced temperature
    EPD_W21_WriteDATA(0x02);
    EPD_W21_WriteCMD(0xE6); // forced temperature value -> fast LUT
    EPD_W21_WriteDATA(0x5A);
    EPD_W21_WriteCMD(0xA5); // load LUT for that temperature
    EPD_W21_WriteDATA(0x00);
    lcd_chkstatus();
  }

  EPD_W21_WriteCMD(0x04); // power on
  lcd_chkstatus();
}

void EPD_init(void)
{
  EPD_init_common(false);
}

void EPD_init_Fast(void)
{
  EPD_init_common(true);
}

void EPD_sleep(void)
{   
	EPD_W21_WriteCMD(0X02);  	//power off
	EPD_W21_WriteDATA(0x00);
	lcd_chkstatus();          //waiting for the electronic paper IC to release the idle signal

	EPD_W21_WriteCMD(0X07);  	//deep sleep
	EPD_W21_WriteDATA(0xA5);
}

void EPD_refresh(void)
{   
  EPD_W21_WriteCMD(0x12); //Display Update Control
  EPD_W21_WriteDATA(0x00);
  lcd_chkstatus();   
}

// BUSY is low while the controller works. A full ACeP refresh takes 15-25 s;
// give up after 60 s so a disconnected panel cannot hang the firmware.
bool lcd_chkstatus(void)
{
  unsigned long start = millis();
  while (isEPD_W21_BUSY == 0)
  {
    if (millis() - start > 60000)
      return false;
    delay(2);
  }
  return true;
}

unsigned char Color_get(unsigned char color)
{
  switch (color)
  {
  case 0x01: return EPD_WHITE;
  case 0x02: return EPD_YELLOW;
  case 0x03: return EPD_RED;
  default:   return EPD_BLACK;
  }
}

uint32_t epdLastTransferMs = 0;
uint32_t epdLastRefreshMs = 0;

static uint8_t lut[256];
static uint32_t frameStart;

// Stream pixel rows in bursts: one CS/DC frame per chunk instead of toggling
// CS for every byte. Codes pass through a 256-entry LUT built from Color_get()
// so drivers with a different colour encoding still work.
void EPD_beginFrame(void)
{
  static bool lutReady = false;
  if (!lutReady)
  {
    for (int v = 0; v < 256; v++)
      lut[v] = (Color_get((v >> 6) & 3) << 6) | (Color_get((v >> 4) & 3) << 4) |
               (Color_get((v >> 2) & 3) << 2) | Color_get(v & 3);
    lutReady = true;
  }
  frameStart = millis();
  EPD_W21_WriteCMD(0x10);
}

void EPD_writeRows(const unsigned char *rows, unsigned count)
{
  uint8_t row[Source_BITS / 4];
  EPD_W21_CS_0;
  EPD_W21_DC_1;
  for (unsigned i = 0; i < count; i++)
  {
    const unsigned char *src = rows + i * (Source_BITS / 4);
    for (unsigned j = 0; j < sizeof(row); j++)
      row[j] = lut[src[j]];
    SPI.writeBytes(row, sizeof(row));
  }
  EPD_W21_CS_1;
}

void EPD_endFrame(void)
{
  epdLastTransferMs = millis() - frameStart;
}

void EPD_writeFrame(const unsigned char *picData)
{
  EPD_beginFrame();
  EPD_writeRows(picData, Gate_BITS);
  EPD_endFrame();
}

void PIC_display(const unsigned char *picData)
{
  EPD_writeFrame(picData);
  uint32_t t0 = millis();
  EPD_refresh();
  epdLastRefreshMs = millis() - t0;
}
