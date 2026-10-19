// Pure helpers (no Arduino dependencies) shared by firmware, preview and tests.
#ifndef UTILS_H
#define UTILS_H

#include "compat.h"
#include "settings.h"

// Dates ---------------------------------------------------------
// Parse DD.MM.YYYY, YYYY-MM-DD[...], MM/DD/YYYY or DD/MM/YYYY
// (slash dates are read as US when the first field > 12 is impossible).
bool parseDate(const char *input, int &day, int &month, int &year);
// Reformat any supported input date into `fmt`; unparseable input passes through.
void formatDate(const char *input, char *output, size_t outSize, const char *fmt);
// Day of week (0=Sun..6=Sat) for a Gregorian date.
int dayOfWeek(int year, int month, int day);
// Extract "HH:MM" from "HH:MM[:SS]" or an ISO timestamp "YYYY-MM-DDTHH:MM...".
bool extractHHMM(const char *in, char out[6]);

// Formatting ----------------------------------------------------
// printf-style float formatting that renders NAN as "--".
void fmtNum(char *out, size_t n, float v, int decimals, const char *unit = "");
// Number of glyphs a UTF-8 string occupies with the 8x16 font.
int utf8Len(const char *s);
// Copy at most `maxGlyphs` glyphs of a UTF-8 string (never splits a sequence).
void utf8Clip(char *dst, size_t n, const char *src, int maxGlyphs);
// Uppercase ASCII + Latvian letters in place (UTF-8 aware).
void utf8Upper(char *s);

// Parse a JSON-ish number value; returns NAN for null/"unknown"/"unavailable".
float parseNumber(const char *s);

// Alerts (thresholds from settings) ------------------------------
bool alertOutTemp(const Thresholds &t, float v);
bool alertWind(const Thresholds &t, float v);
bool alertGust(const Thresholds &t, float v);
bool alertInTemp(const Thresholds &t, float v);
bool alertInHum(const Thresholds &t, float v);
bool alertDown(const Thresholds &t, float v);
bool alertUp(const Thresholds &t, float v);
bool alertPing(const Thresholds &t, float v);
bool alertPrice(const Thresholds &t, float v);
bool alertPower(const Thresholds &t, float v);
bool alertVoltage(const Thresholds &t, float v);
bool alertFrequency(const Thresholds &t, float v);
bool alertFactor(const Thresholds &t, float v);
bool alertMonth(const Thresholds &t, float v);
bool alertRssi(const Thresholds &t, int v);
// Power factor sensors report either 0..1 or percent; normalise to 0..1.
float normFactor(float pf);

// Quiet hours: true when local `hour`:`minute` is inside [start, end)
// (window may wrap midnight; start < 0 disables). secondsUntilQuietEnd returns
// the remaining seconds of the window, 0 when outside.
bool inQuietHours(int start, int end, int hour);
uint32_t secondsUntilQuietEnd(int start, int end, int hour, int minute, int second);

// Seconds from `now` (Unix time) to the next multiple of `intervalSec`,
// shifted earlier by `leadSec` (push wake-up) — never less than `minSec`.
uint32_t secondsToSlot(int64_t now, uint32_t intervalSec, int32_t leadSec, uint32_t minSec);

// Legacy API kept for tests/back-compat (uses global settings thresholds).
bool badTemp(float t);
bool badHum(float h);

#endif
