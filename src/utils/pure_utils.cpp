#include "utils/utils.h"
#include <stdlib.h>
#include <ctype.h>

// ── Dates ──────────────────────────────────────────────────────

bool parseDate(const char *input, int &day, int &month, int &year)
{
  if (!input || strlen(input) < 10)
    return false;
  int a = 0, b = 0, c = 0;
  if (input[4] == '-')
  {
    if (sscanf(input, "%d-%d-%d", &year, &month, &day) != 3)
      return false;
  }
  else if (input[2] == '.')
  {
    if (sscanf(input, "%d.%d.%d", &day, &month, &year) != 3)
      return false;
  }
  else if (input[2] == '/' && input[5] == '/')
  {
    if (sscanf(input, "%d/%d/%d", &a, &b, &c) != 3)
      return false;
    // Ambiguous: prefer US (MM/DD) unless the first field cannot be a month.
    if (a > 12) { day = a; month = b; }
    else { month = a; day = b; }
    year = c;
  }
  else
    return false;
  return month >= 1 && month <= 12 && day >= 1 && day <= 31 && year > 1900;
}

void formatDate(const char *input, char *output, size_t outSize, const char *fmt)
{
  int day = 0, month = 0, year = 0;
  if (!parseDate(input, day, month, year))
  {
    snprintf(output, outSize, "%s", input ? input : "");
    return;
  }
  if (strcmp(fmt, "YYYY-MM-DD") == 0)
    snprintf(output, outSize, "%04d-%02d-%02d", year, month, day);
  else if (strcmp(fmt, "MM/DD/YYYY") == 0)
    snprintf(output, outSize, "%02d/%02d/%04d", month, day, year);
  else if (strcmp(fmt, "DD/MM/YYYY") == 0)
    snprintf(output, outSize, "%02d/%02d/%04d", day, month, year);
  else
    snprintf(output, outSize, "%02d.%02d.%04d", day, month, year);
}

int dayOfWeek(int y, int m, int d)
{
  // Sakamoto's algorithm
  static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (m < 3)
    y -= 1;
  return (y + y / 4 - y / 100 + y / 400 + t[(m - 1) % 12] + d) % 7;
}

bool extractHHMM(const char *in, char out[6])
{
  if (!in)
    return false;
  const char *p = strchr(in, 'T');
  p = p ? p + 1 : in;
  if (strlen(p) >= 5 && isdigit((unsigned char)p[0]) && isdigit((unsigned char)p[1]) && p[2] == ':' &&
      isdigit((unsigned char)p[3]) && isdigit((unsigned char)p[4]))
  {
    memcpy(out, p, 5);
    out[5] = 0;
    return true;
  }
  return false;
}

// ── Formatting ─────────────────────────────────────────────────

void fmtNum(char *out, size_t n, float v, int decimals, const char *unit)
{
  if (isnan(v))
    snprintf(out, n, "--%s%s", unit && unit[0] ? " " : "", unit ? unit : "");
  else
    snprintf(out, n, "%.*f%s%s", decimals, v, unit && unit[0] ? " " : "", unit ? unit : "");
}

static int utf8SeqLen(uint8_t c)
{
  if (c < 0x80) return 1;
  if ((c & 0xE0) == 0xC0) return 2;
  if ((c & 0xF0) == 0xE0) return 3;
  if ((c & 0xF8) == 0xF0) return 4;
  return 1;
}

int utf8Len(const char *s)
{
  int n = 0;
  while (*s)
  {
    int l = utf8SeqLen((uint8_t)*s);
    for (int i = 0; i < l && *s; i++)
      s++;
    n++;
  }
  return n;
}

void utf8Clip(char *dst, size_t n, const char *src, int maxGlyphs)
{
  size_t o = 0;
  int g = 0;
  while (*src && g < maxGlyphs)
  {
    int l = utf8SeqLen((uint8_t)*src);
    if (o + l >= n)
      break;
    for (int i = 0; i < l && *src; i++)
      dst[o++] = *src++;
    g++;
  }
  dst[o] = 0;
}

void utf8Upper(char *s)
{
  while (*s)
  {
    uint8_t c = (uint8_t)*s;
    if (c < 0x80)
    {
      *s = (char)toupper(c);
      s++;
    }
    else if ((c == 0xC4 || c == 0xC5) && s[1])
    {
      // Latvian letters in U+0100..U+017F: lowercase is uppercase + 1 when odd
      // (Ā/ā, Č/č, Ē/ē, Ģ/ģ, Ī/ī, Ķ/ķ, Ļ/ļ, Ņ/ņ, Š/š, Ū/ū, Ž/ž)
      uint16_t cp = ((c & 0x1F) << 6) | ((uint8_t)s[1] & 0x3F);
      bool odd = cp & 1;
      // In U+0139..U+0148 and U+0179..U+017E the pairs are shifted (upper odd).
      bool shifted = (cp >= 0x139 && cp <= 0x148) || (cp >= 0x179 && cp <= 0x17E);
      if ((!shifted && odd) || (shifted && !odd))
      {
        cp -= 1;
        s[0] = (char)(0xC0 | (cp >> 6));
        s[1] = (char)(0x80 | (cp & 0x3F));
      }
      s += 2;
    }
    else
      s += utf8SeqLen(c);
  }
}

float parseNumber(const char *s)
{
  if (!s || !*s)
    return NAN;
  char *end = nullptr;
  float v = strtof(s, &end);
  if (end == s)
    return NAN;
  return v;
}

// ── Alerts ─────────────────────────────────────────────────────

#define OUT_OF(v, lo, hi) (!isnan(v) && ((v) < (lo) || (v) > (hi)))
#define ABOVE(v, hi) (!isnan(v) && (v) > (hi))
#define BELOW(v, lo) (!isnan(v) && (v) < (lo))

bool alertOutTemp(const Thresholds &t, float v) { return OUT_OF(v, t.outTempLow, t.outTempHigh); }
bool alertWind(const Thresholds &t, float v) { return ABOVE(v, t.wind); }
bool alertGust(const Thresholds &t, float v) { return ABOVE(v, t.gust); }
bool alertInTemp(const Thresholds &t, float v) { return OUT_OF(v, t.inTempLow, t.inTempHigh); }
bool alertInHum(const Thresholds &t, float v) { return OUT_OF(v, t.inHumLow, t.inHumHigh); }
bool alertDown(const Thresholds &t, float v) { return BELOW(v, t.downLow); }
bool alertUp(const Thresholds &t, float v) { return BELOW(v, t.upLow); }
bool alertPing(const Thresholds &t, float v) { return ABOVE(v, t.pingHigh); }
bool alertPrice(const Thresholds &t, float v) { return ABOVE(v, t.priceHigh); }
bool alertPower(const Thresholds &t, float v) { return ABOVE(v, t.powerHigh); }
bool alertVoltage(const Thresholds &t, float v) { return OUT_OF(v, t.voltLow, t.voltHigh); }
bool alertFrequency(const Thresholds &t, float v) { return OUT_OF(v, t.freqLow, t.freqHigh); }
bool alertFactor(const Thresholds &t, float v) { return BELOW(normFactor(v), t.pfLow); }
bool alertMonth(const Thresholds &t, float v) { return ABOVE(v, t.monthHigh); }
bool alertRssi(const Thresholds &t, int v) { return v != 0 && v < t.rssiLow; }

float normFactor(float pf)
{
  if (isnan(pf))
    return pf;
  return fabsf(pf) > 1.5f ? pf / 100.0f : pf;
}

bool inQuietHours(int start, int end, int hour)
{
  if (start < 0 || start == end)
    return false;
  if (start < end)
    return hour >= start && hour < end;
  return hour >= start || hour < end; // wraps midnight
}

uint32_t secondsUntilQuietEnd(int start, int end, int hour, int minute, int second)
{
  if (!inQuietHours(start, end, hour))
    return 0;
  int nowS = hour * 3600 + minute * 60 + second;
  int endS = end * 3600;
  if (endS <= nowS)
    endS += 24 * 3600;
  return (uint32_t)(endS - nowS);
}

uint32_t secondsToSlot(int64_t now, uint32_t intervalSec, int32_t leadSec, uint32_t minSec)
{
  if (intervalSec == 0)
    intervalSec = 60;
  int64_t sec = (int64_t)intervalSec - (now % intervalSec) - leadSec;
  while (sec < (int64_t)minSec)
    sec += intervalSec;
  return (uint32_t)sec;
}

bool badTemp(float t) { return alertInTemp(settings.th, t); }
bool badHum(float h) { return alertInHum(settings.th, h); }
