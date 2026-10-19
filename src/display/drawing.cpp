#include "display/drawing.h"
#include "font_8x16.h"
#include "data.h"
#include "config.h"
#include "icons.h"

// The framebuffer may hold the whole screen (host builds) or a horizontal band
// of it (firmware: the screen is rendered band by band and streamed to the
// panel, saving ~84 KB of RAM). All primitives clip to the current band.
int fbY0 = 0;
int fbRows = EPD_HEIGHT;

void fbSetBand(int y0, int rows)
{
  fbY0 = y0;
  fbRows = rows;
}

// Intersect rows [y, y+h) with the band; false when nothing is visible.
static inline bool bandRows(int y, int h, int &from, int &to)
{
  from = y < fbY0 ? fbY0 : y;
  to = y + h > fbY0 + fbRows ? fbY0 + fbRows : y + h;
  if (from < 0) from = 0;
  if (to > EPD_HEIGHT) to = EPD_HEIGHT;
  return from < to;
}

// ── UTF-8 decoding ─────────────────────────────────────────────

struct Utf8Map
{
  uint32_t utf8;
  uint8_t glyph;
};

// 2-byte sequences (Latvian letters, degree)
static const Utf8Map utf8_map2[] = {
    {0xC480, 0x80}, {0xC481, 0x81}, // Ā ā
    {0xC48C, 0x82}, {0xC48D, 0x83}, // Č č
    {0xC492, 0x84}, {0xC493, 0x85}, // Ē ē
    {0xC4A2, 0x86}, {0xC4A3, 0x87}, // Ģ ģ
    {0xC4AA, 0x88}, {0xC4AB, 0x89}, // Ī ī
    {0xC4B6, 0x95}, {0xC4B7, 0x97}, // Ķ ķ
    {0xC4BB, 0x8A}, {0xC4BC, 0x8B}, // Ļ ļ
    {0xC585, 0x8C}, {0xC586, 0x8D}, // Ņ ņ
    {0xC5A0, 0x8E}, {0xC5A1, 0x8F}, // Š š
    {0xC5AA, 0x90}, {0xC5AB, 0x91}, // Ū ū
    {0xC5BD, 0x92}, {0xC5BE, 0x93}, // Ž ž
    {0xC2B0, 0x94},                 // °
    {0xC2B7, 0x98},                 // · (middle dot -> small bullet)
};

// 3-byte sequences (symbols)
static const Utf8Map utf8_map3[] = {
    {0xE282AC, 0x96}, // €
    {0xE296AA, 0x98}, // ▪
    {0xE296B8, 0x99}, // ▸
    {0xE29480, 0x9A}, // ─
    {0xE29482, 0x9B}, // │
    {0xE28691, 0x9C}, // ↑
    {0xE28693, 0x9D}, // ↓
    {0xE28692, 0x9E}, // →
    {0xE28690, 0x9F}, // ←
    {0xE2978F, 0xA0}, // ●
    {0xE2978B, 0xA1}, // ○
    {0xE296A0, 0xA2}, // ■
    {0xE296B2, 0xA3}, // ▲
    {0xE296BC, 0xA4}, // ▼
    {0xE29C93, 0xA5}, // ✓
    {0xE29C97, 0xA6}, // ✗
    {0xE29786, 0xA7}, // ◆
    {0xE29AA1, 0xA8}, // ⚡
    {0xE29880, 0xA9}, // ☀
    {0xE29688, 0xB6}, // █
    {0xE29692, 0xB7}, // ▒
    // typographic punctuation folded to ASCII
    {0xE28093, '-'},  // – en dash
    {0xE28094, '-'},  // — em dash
    {0xE28098, '\''}, // ‘
    {0xE28099, '\''}, // ’
    {0xE2809C, '"'},  // “
    {0xE2809D, '"'},  // ”
    {0xE280A6, '.'},  // … (renders as a single dot)
};

uint8_t fontNextChar(const char **s)
{
  const uint8_t *p = (const uint8_t *)*s;
  uint8_t c0 = p[0];

  if (c0 < 0x80)
  {
    (*s)++;
    return c0;
  }
  if ((c0 & 0xE0) == 0xC0 && p[1])
  {
    uint32_t u = (c0 << 8) | p[1];
    (*s) += 2;
    for (const Utf8Map &m : utf8_map2)
      if (m.utf8 == u)
        return m.glyph;
    return '?';
  }
  if ((c0 & 0xF0) == 0xE0 && p[1] && p[2])
  {
    uint32_t u = ((uint32_t)c0 << 16) | ((uint32_t)p[1] << 8) | p[2];
    (*s) += 3;
    for (const Utf8Map &m : utf8_map3)
      if (m.utf8 == u)
        return m.glyph;
    return '?';
  }
  if ((c0 & 0xF8) == 0xF0 && p[1] && p[2] && p[3])
  {
    (*s) += 4; // emoji etc.: unsupported
    return '?';
  }
  // Raw single byte >= 0x80: direct glyph index (G_* macros)
  (*s)++;
  return c0;
}

// ── Pixel primitives ───────────────────────────────────────────

void px(int x, int y, int c)
{
  y -= fbY0;
  if ((unsigned)x >= EPD_WIDTH || (unsigned)y >= (unsigned)fbRows)
    return;
  uint32_t i = (uint32_t)y * EPD_WIDTH + x;
  uint32_t b = i >> 2;
  uint8_t s = (3 - (i & 3)) * 2;
  fb[b] = (fb[b] & ~(3 << s)) | ((c & 3) << s);
}

int pxGet(int x, int y)
{
  y -= fbY0;
  if ((unsigned)x >= EPD_WIDTH || (unsigned)y >= (unsigned)fbRows)
    return WHITE;
  uint32_t i = (uint32_t)y * EPD_WIDTH + x;
  return (fb[i >> 2] >> ((3 - (i & 3)) * 2)) & 3;
}

void fill(int c)
{
  memset(fb, (c & 3) * 0x55, (size_t)fbRows * EPD_WIDTH / 4);
}

void fillRect(int x, int y, int w, int h, int c)
{
  int y0, y1;
  if (!bandRows(y, h, y0, y1))
    return;
  for (int yy = y0; yy < y1; yy++)
    for (int xx = x; xx < x + w; xx++)
      px(xx, yy, c);
}

void fillRectDither(int x, int y, int w, int h, int c1, int c2)
{
  int y0, y1;
  if (!bandRows(y, h, y0, y1))
    return;
  for (int yy = y0; yy < y1; yy++)
    for (int xx = x; xx < x + w; xx++)
      px(xx, yy, ((xx + yy) & 1) ? c1 : c2);
}

void fillRectDitherDense(int x, int y, int w, int h, int c1, int c2)
{
  int y0, y1;
  if (!bandRows(y, h, y0, y1))
    return;
  for (int yy = y0; yy < y1; yy++)
    for (int xx = x; xx < x + w; xx++)
      px(xx, yy, ((xx & 1) == 0 && (yy & 1) == 0) ? c2 : c1);
}

void fillRectShadeLight(int x, int y, int w, int h, int c1, int c2)
{
  int y0, y1;
  if (!bandRows(y, h, y0, y1))
    return;
  for (int yy = y0; yy < y1; yy++)
    for (int xx = x; xx < x + w; xx++)
      px(xx, yy, ((xx & 3) == 0 && (yy & 3) == 0) ? c2 : c1);
}

// 16x16 Bayer ordered dither matrix (values 0-255)
static const uint8_t bayer16[16][16] PROGMEM = {
    {0, 128, 32, 160, 8, 136, 40, 168, 2, 130, 34, 162, 10, 138, 42, 170},
    {192, 64, 224, 96, 200, 72, 232, 104, 194, 66, 226, 98, 202, 74, 234, 106},
    {48, 176, 16, 144, 56, 184, 24, 152, 50, 178, 18, 146, 58, 186, 26, 154},
    {240, 112, 208, 80, 248, 120, 216, 88, 242, 114, 210, 82, 250, 122, 218, 90},
    {12, 140, 44, 172, 4, 132, 36, 164, 14, 142, 46, 174, 6, 134, 38, 166},
    {204, 76, 236, 108, 196, 68, 228, 100, 206, 78, 238, 110, 198, 70, 230, 102},
    {60, 188, 28, 156, 52, 180, 20, 148, 62, 190, 30, 158, 54, 182, 22, 150},
    {252, 124, 220, 92, 244, 116, 212, 84, 254, 126, 222, 94, 246, 118, 214, 86},
    {3, 131, 35, 163, 11, 139, 43, 171, 1, 129, 33, 161, 9, 137, 41, 169},
    {195, 67, 227, 99, 203, 75, 235, 107, 193, 65, 225, 97, 201, 73, 233, 105},
    {51, 179, 19, 147, 59, 187, 27, 155, 49, 177, 17, 145, 57, 185, 25, 153},
    {243, 115, 211, 83, 251, 123, 219, 91, 241, 113, 209, 81, 249, 121, 217, 89},
    {15, 143, 47, 175, 7, 135, 39, 167, 13, 141, 45, 173, 5, 133, 37, 165},
    {207, 79, 239, 111, 199, 71, 231, 103, 205, 77, 237, 109, 197, 69, 229, 101},
    {63, 191, 31, 159, 55, 183, 23, 151, 61, 189, 29, 157, 53, 181, 21, 149},
    {255, 127, 223, 95, 247, 119, 215, 87, 253, 125, 221, 93, 245, 117, 213, 85}};

static inline int bayer(int x, int y) { return pgm_read_byte(&bayer16[y & 15][x & 15]); }

void fillRectGradientH(int x, int y, int w, int h, int c_left, int c_right)
{
  int start = w * 60 / 100;
  int y0, y1;
  if (!bandRows(y, h, y0, y1))
    return;
  for (int yy = y0; yy < y1; yy++)
    for (int xx = x; xx < x + w; xx++)
    {
      int dx = xx - x;
      int ratio = dx <= start ? 0 : (dx - start) * 256 / (w - start);
      if (ratio > 256)
        ratio = 256;
      px(xx, yy, (ratio > bayer(xx, yy)) ? c_right : c_left);
    }
}

void fillRectLevel(int x, int y, int w, int h, int c1, int c2, int level)
{
  int y0, y1;
  if (!bandRows(y, h, y0, y1))
    return;
  for (int yy = y0; yy < y1; yy++)
    for (int xx = x; xx < x + w; xx++)
      px(xx, yy, (level > bayer(xx, yy)) ? c2 : c1);
}

// ── Lines & shapes ─────────────────────────────────────────────

void hline(int x, int y, int w, int c)
{
  if (y < fbY0 || y >= fbY0 + fbRows)
    return;
  for (int xx = x; xx < x + w; xx++)
    px(xx, y, c);
}

void vline(int x, int y, int h, int c)
{
  int y0, y1;
  if (!bandRows(y, h, y0, y1))
    return;
  for (int yy = y0; yy < y1; yy++)
    px(x, yy, c);
}

void hlineDotted(int x, int y, int w, int c, int on, int off)
{
  for (int i = 0; i < w; i++)
    if (i % (on + off) < on)
      px(x + i, y, c);
}

void vlineDotted(int x, int y, int h, int c, int on, int off)
{
  for (int i = 0; i < h; i++)
    if (i % (on + off) < on)
      px(x, y + i, c);
}

void line(int x0, int y0, int x1, int y1, int c)
{
  int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
  int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  for (;;)
  {
    px(x0, y0, c);
    if (x0 == x1 && y0 == y1)
      break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

void circle(int cx, int cy, int r, int c)
{
  int x = r, y = 0, err = 1 - r;
  while (x >= y)
  {
    px(cx + x, cy + y, c); px(cx + y, cy + x, c);
    px(cx - y, cy + x, c); px(cx - x, cy + y, c);
    px(cx - x, cy - y, c); px(cx - y, cy - x, c);
    px(cx + y, cy - x, c); px(cx + x, cy - y, c);
    y++;
    if (err < 0)
      err += 2 * y + 1;
    else
    {
      x--;
      err += 2 * (y - x) + 1;
    }
  }
}

void fillCircle(int cx, int cy, int r, int c)
{
  for (int y = -r; y <= r; y++)
    for (int x = -r; x <= r; x++)
      if (x * x + y * y <= r * r + r)
        px(cx + x, cy + y, c);
}

void fillRoundRect(int x, int y, int w, int h, int r, int c)
{
  for (int yy = 0; yy < h; yy++)
    for (int xx = 0; xx < w; xx++)
    {
      int dx = xx < r ? r - xx : (xx >= w - r ? xx - (w - r - 1) : 0);
      int dy = yy < r ? r - yy : (yy >= h - r ? yy - (h - r - 1) : 0);
      if (dx * dx + dy * dy <= r * r)
        px(x + xx, y + yy, c);
    }
}

// ── Boxes ──────────────────────────────────────────────────────

void rectColored(int x, int y, int w, int h, int c)
{
  hline(x, y, w, c);
  hline(x, y + h - 1, w, c);
  vline(x, y, h, c);
  vline(x + w - 1, y, h, c);
}

void rect(int x, int y, int w, int h) { rectColored(x, y, w, h, BLACK); }

void rectThick(int x, int y, int w, int h, int t, int c)
{
  for (int i = 0; i < t; i++)
    rectColored(x + i, y + i, w - 2 * i, h - 2 * i, c);
}

void rectWithTitle(int x, int y, int w, int h, const char *title)
{
  const int TITLE_H = 22;
  rect(x, y, w, h);
  fillRect(x + 1, y + 1, w - 2, TITLE_H, BLACK);
  hline(x + 1, y + TITLE_H + 1, w - 2, YELLOW);
  text(x + 8, y + 3, title, WHITE);
}

// ── Text ───────────────────────────────────────────────────────

static void glyph(int x, int y, uint8_t ch, int c)
{
  if (y + 16 <= fbY0 || y >= fbY0 + fbRows)
    return;
  const uint8_t *g = font8x16[ch];
  for (int r = 0; r < 16; r++)
  {
    uint8_t b = g[r];
    if (!b)
      continue;
    for (int i = 0; i < 8; i++)
      if (b & (0x80 >> i))
        px(x + i, y + r, c);
  }
}

void text(int x, int y, const char *s, int c)
{
  while (*s)
  {
    glyph(x, y, fontNextChar(&s), c);
    x += 8;
  }
}

void textDim(int x, int y, const char *s, int c)
{
  while (*s)
  {
    const uint8_t *g = font8x16[fontNextChar(&s)];
    for (int r = 0; r < 16; r++)
      for (int i = 0; i < 8; i++)
        if ((g[r] & (0x80 >> i)) && ((x + i + y + r) & 1))
          px(x + i, y + r, c);
    x += 8;
  }
}

void textBold(int x, int y, const char *s, int c)
{
  text(x, y, s, c);
  text(x + 1, y, s, c);
}

int textWidth(const char *s, int scale)
{
  int n = 0;
  while (*s)
  {
    fontNextChar(&s);
    n++;
  }
  return n * 8 * scale;
}

// Scale2x (EPX) on a 1-bit bitmap: src w x h -> dst 2w x 2h
static void scale2x(const uint8_t *src, int w, int h, uint8_t *dst)
{
  auto at = [&](int x, int y) -> uint8_t {
    if (x < 0 || y < 0 || x >= w || y >= h)
      return 0;
    return src[y * w + x];
  };
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++)
    {
      uint8_t P = at(x, y), A = at(x, y - 1), B = at(x + 1, y), C = at(x - 1, y), D = at(x, y + 1);
      uint8_t e0 = P, e1 = P, e2 = P, e3 = P;
      if (C == A && C != D && A != B) e0 = A;
      if (A == B && A != C && B != D) e1 = B;
      if (D == C && D != B && C != A) e2 = C;
      if (B == D && B != A && D != C) e3 = D;
      int W = w * 2;
      dst[(2 * y) * W + 2 * x] = e0;
      dst[(2 * y) * W + 2 * x + 1] = e1;
      dst[(2 * y + 1) * W + 2 * x] = e2;
      dst[(2 * y + 1) * W + 2 * x + 1] = e3;
    }
}

static void glyphScaled(int x, int y, uint8_t ch, int c, int scale)
{
  if (y + 16 * scale <= fbY0 || y >= fbY0 + fbRows)
    return;
  // Up to two Scale2x passes (8x16 -> 32x64), remaining factor is nearest.
  static uint8_t a[8 * 16], b[16 * 32], d[32 * 64];
  const uint8_t *g = font8x16[ch];
  for (int r = 0; r < 16; r++)
    for (int i = 0; i < 8; i++)
      a[r * 8 + i] = (g[r] & (0x80 >> i)) ? 1 : 0;

  const uint8_t *bm = a;
  int w = 8, h = 16, rest = scale;
  if (rest % 2 == 0)
  {
    scale2x(a, 8, 16, b);
    bm = b; w = 16; h = 32; rest /= 2;
    if (rest % 2 == 0)
    {
      scale2x(b, 16, 32, d);
      bm = d; w = 32; h = 64; rest /= 2;
    }
  }
  for (int yy = 0; yy < h; yy++)
    for (int xx = 0; xx < w; xx++)
      if (bm[yy * w + xx])
      {
        if (rest == 1)
          px(x + xx, y + yy, c);
        else
          fillRect(x + xx * rest, y + yy * rest, rest, rest, c);
      }
}

void textScaled(int x, int y, const char *s, int c, int scale)
{
  if (scale <= 1)
  {
    text(x, y, s, c);
    return;
  }
  while (*s)
  {
    glyphScaled(x, y, fontNextChar(&s), c, scale);
    x += 8 * scale;
  }
}

void textRight(int xr, int y, const char *s, int c, int scale)
{
  textScaled(xr - textWidth(s, scale), y, s, c, scale);
}

void textCenter(int cx, int y, const char *s, int c, int scale)
{
  textScaled(cx - textWidth(s, scale) / 2, y, s, c, scale);
}

// ── Seven segment ──────────────────────────────────────────────
//  segments: a=top b=top-right c=bottom-right d=bottom e=bottom-left f=top-left g=middle

static const uint8_t SEG_DIGITS[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};

static void segH(int x, int y, int len, int t, int c, bool ghost)
{
  for (int i = 0; i < t; i++)
  {
    int inset = i < t / 2 ? t / 2 - i : i - t / 2;
    for (int xx = x + inset; xx < x + len - inset; xx++)
      if (!ghost || ((xx & 3) == 0 && ((y + i) & 1) == 0))
        px(xx, y + i, c);
  }
}

static void segV(int x, int y, int len, int t, int c, bool ghost)
{
  for (int i = 0; i < t; i++)
  {
    int inset = i < t / 2 ? t / 2 - i : i - t / 2;
    for (int yy = y + inset; yy < y + len - inset; yy++)
      if (!ghost || (((x + i) & 1) == 0 && (yy & 3) == 0))
        px(x + i, yy, c);
  }
}

static int segCharWidth(char ch, int h)
{
  int w = h / 2;
  if (ch == '.' || ch == ':')
    return h / 5;
  if (ch == ' ')
    return w / 2;
  if (ch == '\x94')
    return h / 3;
  return w + h / 8;
}

int sevenSegWidth(const char *s, int h)
{
  int w = 0;
  while (*s)
  {
    uint8_t ch = fontNextChar(&s);
    w += segCharWidth((char)ch, h);
  }
  return w;
}

void sevenSeg(int x, int y, const char *s, int h, int c, int ghost)
{
  // Ghost (unlit) segments only behind large numerals; on small digits they
  // read as dirt rather than as a display.
  if (h < SEVENSEG_GHOST_MIN_H)
    ghost = -1;
  int t = h / 8 < 3 ? 3 : h / 8;
  int w = h / 2;
  int half = h / 2;
  while (*s)
  {
    uint8_t ch = fontNextChar(&s);
    uint8_t mask = 0;
    bool seg = true;
    if (ch >= '0' && ch <= '9') mask = SEG_DIGITS[ch - '0'];
    else if (ch == '-') mask = 0x40;
    else if (ch == 'E') mask = 0x79;
    else if (ch == 'C') mask = 0x39;
    else if (ch == 'H') mask = 0x76;
    else if (ch == 'L') mask = 0x38;
    else if (ch == 'P') mask = 0x73;
    else if (ch == 'o') mask = 0x5C;
    else seg = false;

    if (seg)
    {
      for (int pass = 0; pass < 2; pass++)
      {
        bool g = pass == 0;
        if (g && ghost < 0)
          continue;
        uint8_t m = g ? (uint8_t)~mask : mask;
        int col = g ? ghost : c;
        if (m & 0x01) segH(x + 1, y, w - 2, t, col, g);
        if (m & 0x02) segV(x + w - t, y + 1, half - 1, t, col, g);
        if (m & 0x04) segV(x + w - t, y + half, half - 1, t, col, g);
        if (m & 0x08) segH(x + 1, y + h - t, w - 2, t, col, g);
        if (m & 0x10) segV(x, y + half, half - 1, t, col, g);
        if (m & 0x20) segV(x, y + 1, half - 1, t, col, g);
        if (m & 0x40) segH(x + 1, y + half - t / 2, w - 2, t, col, g);
      }
    }
    else if (ch == '.')
      fillRect(x + (h / 5 - t) / 2, y + h - t, t, t, c);
    else if (ch == ':')
    {
      fillRect(x + (h / 5 - t) / 2, y + h / 4, t, t, c);
      fillRect(x + (h / 5 - t) / 2, y + 3 * h / 4 - t, t, t, c);
    }
    else if (ch == 0x94) // degree
    {
      int r = h / 8;
      for (int i = 0; i < (t + 1) / 2; i++)
        circle(x + r + 1, y + r + 1, r - i, c);
    }
    x += segCharWidth((char)ch, h);
  }
}

// ── Icons ──────────────────────────────────────────────────────

void icon(int x, int y, const uint8_t *iconData, int c)
{
  iconScaled(x, y, iconData, c, 1);
}

void iconScaled(int x, int y, const uint8_t *iconData, int c, int scale)
{
  for (int row = 0; row < 24; row++)
    for (int col = 0; col < 24; col++)
      if (pgm_read_byte(&iconData[row * 3 + (col / 8)]) & (0x80 >> (col % 8)))
      {
        if (scale == 1)
          px(x + col, y + row, c);
        else
          fillRect(x + col * scale, y + row * scale, scale, scale, c);
      }
}

void icon16(int x, int y, const uint8_t *data16, int c)
{
  for (int r = 0; r < 16; r++)
    for (int i = 0; i < 16; i++)
      if (data16[r * 2 + i / 8] & (0x80 >> (i % 8)))
        px(x + i, y + r, c);
}

const uint8_t *getWeatherIcon(const char *state)
{
  struct M
  {
    const char *k;
    const uint8_t *i;
  };
  static const M map[] = {
      {"sunny", ICON_SUNNY}, {"clear-night", ICON_CLEAR_NIGHT}, {"cloudy", ICON_CLOUDY},
      {"partlycloudy", ICON_PARTLYCLOUDY}, {"rainy", ICON_RAINY}, {"pouring", ICON_POURING},
      {"snowy", ICON_SNOWY}, {"snowy-rainy", ICON_SNOWY_RAINY}, {"fog", ICON_FOG}, {"hail", ICON_HAIL},
      {"windy", ICON_WINDY}, {"windy-variant", ICON_WINDY_VARIANT},
      {"lightning", ICON_THUNDERSTORM}, {"lightning-rainy", ICON_LIGHTNING_RAINY},
      {"exceptional", ICON_EXCEPTIONAL}};
  for (const M &m : map)
    if (strcmp(state, m.k) == 0)
      return m.i;
  return ICON_CLOUDY;
}

// ── BMP export ─────────────────────────────────────────────────

static const uint32_t BMP_HDR = 14 + 40 + 16 * 4;
static const uint32_t BMP_ROW = EPD_WIDTH / 2;

size_t bmpSize() { return BMP_HDR + BMP_ROW * EPD_HEIGHT; }

static uint8_t bmpHeaderByte(uint32_t i)
{
  static const uint8_t pal[4][4] = {
      // B, G, R, 0 — approximations of the GDEM075F52 inks on paper
      {0x26, 0x22, 0x22, 0}, // black
      {0xE0, 0xEC, 0xF0, 0}, // white (paper)
      {0x10, 0xC4, 0xE8, 0}, // yellow
      {0x34, 0x2A, 0xC4, 0}, // red
  };
  uint32_t fileSize = bmpSize();
  uint8_t h[54] = {'B', 'M'};
  auto put32 = [&](int o, uint32_t v) { h[o] = v; h[o + 1] = v >> 8; h[o + 2] = v >> 16; h[o + 3] = v >> 24; };
  put32(2, fileSize);
  put32(10, BMP_HDR);
  put32(14, 40);
  put32(18, EPD_WIDTH);
  put32(22, EPD_HEIGHT); // bottom-up
  h[26] = 1;             // planes
  h[28] = 4;             // bpp
  put32(34, BMP_ROW * EPD_HEIGHT);
  put32(38, 2835);
  put32(42, 2835);
  put32(46, 16);
  put32(50, 4);
  if (i < 54)
    return h[i];
  uint32_t p = i - 54;
  return p < 16 ? pal[p / 4][p % 4] : 0;
}

int bmpRowAt(size_t index)
{
  if (index < BMP_HDR)
    return -1;
  return EPD_HEIGHT - 1 - (int)((index - BMP_HDR) / BMP_ROW);
}

size_t bmpRead(uint8_t *buf, size_t maxLen, size_t index)
{
  size_t total = bmpSize(), n = 0;
  while (n < maxLen && index < total)
  {
    if (index < BMP_HDR)
      buf[n] = bmpHeaderByte(index);
    else
    {
      uint32_t p = index - BMP_HDR;
      int row = EPD_HEIGHT - 1 - p / BMP_ROW;
      uint32_t x = (p % BMP_ROW) * 2;
      if (row < fbY0 || row >= fbY0 + fbRows)
      {
        buf[n++] = (WHITE << 4) | WHITE; // outside the rendered band
        index++;
        continue;
      }
      uint32_t i = (uint32_t)(row - fbY0) * EPD_WIDTH + x;
      // two 2-bit pixels from the same fb byte -> one 4-bit pair
      uint8_t byte = fb[i >> 2];
      uint8_t s = (3 - (i & 3)) * 2;
      uint8_t p0 = (byte >> s) & 3, p1 = (byte >> (s - 2)) & 3;
      buf[n] = (p0 << 4) | p1;
    }
    n++;
    index++;
  }
  return n;
}
