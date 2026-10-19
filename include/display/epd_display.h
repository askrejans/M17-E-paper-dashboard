#ifndef EPD_DISPLAY_H
#define EPD_DISPLAY_H

#include <Arduino.h>

#define Source_BITS 800
#define Gate_BITS   480
#define ALLSCREEN_BYTES (Source_BITS * Gate_BITS / 4)  // 96000 bytes

// Display functions
void EPD_init(void);
void EPD_init_Fast(void);
void EPD_sleep(void);
void EPD_refresh(void);
bool lcd_chkstatus(void);  // false on BUSY timeout

// Picture display
void EPD_writeFrame(const unsigned char *picData);  // transfer only
void EPD_beginFrame(void);
void EPD_writeRows(const unsigned char *rows, unsigned count);
void EPD_endFrame(void);
void PIC_display(const unsigned char* picData);     // transfer + refresh

// Panel colour codes for 2-bit colour mode
#define EPD_BLACK  0x00
#define EPD_WHITE  0x01
#define EPD_YELLOW 0x02
#define EPD_RED    0x03

unsigned char Color_get(unsigned char color);

// Timing of the last PIC_display() (SPI transfer, panel waveform)
extern uint32_t epdLastTransferMs;
extern uint32_t epdLastRefreshMs;

#endif
