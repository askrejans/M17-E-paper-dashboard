// Logging to Serial plus an in-memory ring buffer shown in the admin console.
#pragma once

#include <Arduino.h>

void logBegin();
void logf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void logTitle(const char *t);
// Copy the ring buffer (oldest first) into `out`.
void logDump(String &out);
