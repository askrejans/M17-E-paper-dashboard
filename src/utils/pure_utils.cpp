#include "utils/utils.h"

bool badTemp(float t) { return t < 19 || t > 25; }
bool badHum(float h) { return h < 35 || h > 60; }

const char *familyStateLV(const char *s)
{
  if (strcmp(s, "home") == 0)
    return "Mājās";
  if (strcmp(s, "not_home") == 0)
    return "Prom";
  return s; // fallback, don't hide unknown states
}