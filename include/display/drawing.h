// Framebuffer drawing primitives (platform independent).
// Framebuffer: 800x480, 2 bits per pixel, 4 pixels per byte, MSB first.
// Colours: BLACK=0, WHITE=1, YELLOW=2, RED=3 (see data.h).
#ifndef DRAWING_H
#define DRAWING_H

#include "compat.h"

extern unsigned char *fb;

// Band rendering: fb holds rows [fbY0, fbY0 + fbRows). Full screen by default.
extern int fbY0, fbRows;
void fbSetBand(int y0, int rows);
// Row of the BMP stream that byte `index` belongs to (-1 for the header).
int bmpRowAt(size_t index);

// Pixel & fill primitives
void px(int x, int y, int c);
int pxGet(int x, int y);
void fill(int c);
void fillRect(int x, int y, int w, int h, int c);
void fillRectDither(int x, int y, int w, int h, int c1, int c2);       // 50/50 checkerboard
void fillRectDitherDense(int x, int y, int w, int h, int c1, int c2);  // 75/25
void fillRectShadeLight(int x, int y, int w, int h, int c1, int c2);   // 1/16 sparse dots
void fillRectGradientH(int x, int y, int w, int h, int c_left, int c_right);
// Ordered (Bayer 16x16) dither with an explicit level 0..256 of c2 over c1.
void fillRectLevel(int x, int y, int w, int h, int c1, int c2, int level);

// Lines & shapes
void hline(int x, int y, int w, int c);
void vline(int x, int y, int h, int c);
void hlineDotted(int x, int y, int w, int c, int on, int off);
void vlineDotted(int x, int y, int h, int c, int on, int off);
void line(int x0, int y0, int x1, int y1, int c);
void circle(int cx, int cy, int r, int c);
void fillCircle(int cx, int cy, int r, int c);
void fillRoundRect(int x, int y, int w, int h, int r, int c);

// Boxes
void rect(int x, int y, int w, int h);
void rectColored(int x, int y, int w, int h, int c);
void rectThick(int x, int y, int w, int h, int t, int c);
void rectWithTitle(int x, int y, int w, int h, const char *title);

// Text (8x16 bitmap font, UTF-8 with Latvian letters and glyph icons)
uint8_t fontNextChar(const char **s);
void text(int x, int y, const char *s, int c);
void textBold(int x, int y, const char *s, int c);
void textDim(int x, int y, const char *s, int c);   // 50 % checkerboard glyphs
int textWidth(const char *s, int scale = 1);
// Scaled text. Even scale factors use Scale2x edge smoothing, which gives the
// chunky-but-clean look of vintage character generators.
void textScaled(int x, int y, const char *s, int c, int scale);
void textRight(int xr, int y, const char *s, int c, int scale = 1);
void textCenter(int cx, int y, const char *s, int c, int scale = 1);

// Seven-segment numerals ("0-9 - . : ° space"). `ghost` >= 0 draws unlit
// segments in that colour (dithered over white) like a real VFD/LED module.
#define SEVENSEG_GHOST_MIN_H 36
int sevenSegWidth(const char *s, int h);
void sevenSeg(int x, int y, const char *s, int h, int c, int ghost = -1);

// Icons
void icon(int x, int y, const uint8_t *iconData, int c);       // 24x24 PROGMEM
void icon16(int x, int y, const uint8_t *data16, int c);       // 16x16
void iconScaled(int x, int y, const uint8_t *iconData, int c, int scale);
const uint8_t *getWeatherIcon(const char *state);

// BMP export of the framebuffer (4 bpp, palette approximating the panel).
size_t bmpSize();
// Fill `buf` with up to `maxLen` bytes of the BMP starting at byte `index`.
size_t bmpRead(uint8_t *buf, size_t maxLen, size_t index);

#endif
