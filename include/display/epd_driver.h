#ifndef EPD_DRIVER_H
#define EPD_DRIVER_H

#include <Arduino.h>

/*
 * E-Paper Display Driver Interface
 *
 * All display drivers must provide implementations of these functions
 * and define the resolution/color constants below.
 *
 * To add a new driver:
 *   1. Create src/display/drivers/epd_<name>.cpp
 *   2. Create include/display/drivers/epd_<name>.h
 *   3. Implement all functions declared here
 *   4. Set EPD_DRIVER_<NAME> build flag in platformio.ini
 *
 * The drawing layer (drawing.cpp) and web server are display-agnostic —
 * they only use the framebuffer and these driver functions.
 */

// ── Driver selection via build flags ────────────────────────────
// Default: GDEM075F52 (GoodDisplay 7.5" 4-color ACeP)
// Override in platformio.ini with: build_flags = -DEPD_DRIVER_WAVESHARE75BW
//
// Each driver header must define:
//   Source_BITS, Gate_BITS, ALLSCREEN_BYTES
//   black, white, yellow, red (color byte values)

#if defined(EPD_DRIVER_WAVESHARE75BW)
  #include "display/drivers/epd_waveshare75bw.h"
#elif defined(EPD_DRIVER_CUSTOM)
  #include "display/drivers/epd_custom.h"
#else
  // Default: GDEM075F52
  #include "display/epd_display.h"
#endif

// ── Required driver functions ───────────────────────────────────
// These must be implemented by every driver:

// Initialize display controller (full quality mode)
void EPD_init(void);

// Initialize display controller (fast update mode, reduced quality)
void EPD_init_Fast(void);

// Enter low-power sleep mode
void EPD_sleep(void);

// Trigger display refresh (waveform cycle)
void EPD_refresh(void);

// Wait for display to become idle (BUSY pin)
void lcd_chkstatus(void);

// Push framebuffer to display via SPI
// The framebuffer uses 2 bits per pixel with Color_get() mapping
void PIC_display(const unsigned char* picData);

// Map internal 2-bit color code to driver-specific color byte
// Input: 0x00=black, 0x01=white, 0x02=yellow, 0x03=red
unsigned char Color_get(unsigned char color);

#endif
