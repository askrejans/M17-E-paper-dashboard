#include "utils/utils.h"
#include <string.h>
#include <stdio.h>

bool badTemp(float t) { return t < 19 || t > 25; }
bool badHum(float h) { return h < 35 || h > 60; }

// Parse any common date format into day/month/year components
static bool parseDate(const char *input, int &day, int &month, int &year)
{
  // YYYY-MM-DD (ISO)
  if (strlen(input) >= 10 && input[4] == '-') {
    return sscanf(input, "%d-%d-%d", &year, &month, &day) == 3;
  }
  // MM/DD/YYYY (US)
  if (strlen(input) >= 10 && input[2] == '/') {
    return sscanf(input, "%d/%d/%d", &month, &day, &year) == 3;
  }
  // DD.MM.YYYY (EU)
  if (strlen(input) >= 10 && input[2] == '.') {
    return sscanf(input, "%d.%d.%d", &day, &month, &year) == 3;
  }
  // DD/MM/YYYY (UK)
  if (strlen(input) >= 10 && input[2] == '/' && input[5] == '/') {
    return sscanf(input, "%d/%d/%d", &day, &month, &year) == 3;
  }
  return false;
}

void formatDate(const char *input, char *output, size_t outSize, const char *fmt)
{
  int day = 0, month = 0, year = 0;
  if (!parseDate(input, day, month, year)) {
    // Can't parse — pass through unchanged
    snprintf(output, outSize, "%s", input);
    return;
  }

  if (strcmp(fmt, "YYYY-MM-DD") == 0) {
    snprintf(output, outSize, "%04d-%02d-%02d", year, month, day);
  } else if (strcmp(fmt, "MM/DD/YYYY") == 0) {
    snprintf(output, outSize, "%02d/%02d/%04d", month, day, year);
  } else if (strcmp(fmt, "DD/MM/YYYY") == 0) {
    snprintf(output, outSize, "%02d/%02d/%04d", day, month, year);
  } else {
    // Default: DD.MM.YYYY
    snprintf(output, outSize, "%02d.%02d.%04d", day, month, year);
  }
}