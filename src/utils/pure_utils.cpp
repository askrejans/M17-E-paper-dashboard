#include "utils/utils.h"

bool badTemp(float t) { return t < 19 || t > 25; }
bool badHum(float h) { return h < 35 || h > 60; }