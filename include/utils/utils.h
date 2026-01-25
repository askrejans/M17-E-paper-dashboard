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
const char *familyStateLV(const char *s);
void goToSleepSeconds(uint32_t sec);

void logTitle(const char *t);
void logKV(const char *k, const char *v);
void logKVf(const char *k, float v, const char *u = "");

#endif