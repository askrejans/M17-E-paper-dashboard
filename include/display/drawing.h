#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>

extern unsigned char *fb;

void px(int x, int y, int c);
void fill(int c);
void fillRect(int x, int y, int w, int h, int c);
void rect(int x, int y, int w, int h);
void rectWithTitle(int x, int y, int w, int h, const char *title);
void text(int x, int y, const char *s, int c);
void draw_dashboard();

#endif