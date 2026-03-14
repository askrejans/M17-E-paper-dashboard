#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>

extern unsigned char *fb;

// Pixel & fill primitives
void px(int x, int y, int c);
void fill(int c);
void fillRect(int x, int y, int w, int h, int c);
void fillRectDither(int x, int y, int w, int h, int c1, int c2);
void fillRectDitherDense(int x, int y, int w, int h, int c1, int c2);
void fillRectShadeLight(int x, int y, int w, int h, int c1, int c2);
void fillRectGradientH(int x, int y, int w, int h, int c_left, int c_right);

// Line primitives
void hline(int x, int y, int w, int c);
void vline(int x, int y, int h, int c);

// Box primitives
void rect(int x, int y, int w, int h);
void rectColored(int x, int y, int w, int h, int c);
void rectWithTitle(int x, int y, int w, int h, const char *title);

// Text & icon rendering
void text(int x, int y, const char *s, int c);
void icon(int x, int y, const uint8_t *iconData, int c);
const uint8_t* getWeatherIcon(const char *state);

// Forecast helpers
const char* getForecastWeekdayAbbr(int offset);

// Dashboard
void draw_dashboard();

#endif