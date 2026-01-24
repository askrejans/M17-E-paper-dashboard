#ifndef DISPLAY_EPD_W21_H
#define DISPLAY_EPD_W21_H

#include <Arduino.h>

#define Source_BITS 800
#define Gate_BITS   480
#define ALLSCREEN_BYTES (Source_BITS * Gate_BITS / 4)  // 96000 bytes

// Display functions
void EPD_init(void);
void EPD_init_Fast(void);
void EPD_sleep(void);
void EPD_refresh(void);
void lcd_chkstatus(void);

// Display patterns
void Display_All_Black(void);
void Display_All_White(void);
void Display_All_Yellow(void);
void Display_All_Red(void);

// Picture display
void PIC_display(const unsigned char* picData);

// Color definitions for 2-bit color mode
#define white  0x01
#define yellow 0x02
#define red    0x03
#define black  0x00

unsigned char Color_get(unsigned char color);

#endif
