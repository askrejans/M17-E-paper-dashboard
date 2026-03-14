#ifndef UTILS_H
#define UTILS_H

#ifndef NATIVE
#include <ArduinoJson.h>
#endif

#ifndef NATIVE
float jf(JsonVariant v);
#endif
bool badTemp(float t);
bool badHum(float h);
void goToSleepSeconds(uint32_t sec);

void logTitle(const char *t);
void logKV(const char *k, const char *v);
void logKVf(const char *k, float v, const char *u = "");

// Reformat date string to match DATE_FORMAT config
// Parses DD.MM.YYYY, YYYY-MM-DD, MM/DD/YYYY and outputs in target format
void formatDate(const char *input, char *output, size_t outSize, const char *fmt);

#endif