#include "app/log.h"
#include <stdarg.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

static const size_t RING = 6144;
static char ring[RING];
static size_t head = 0;
static bool wrapped = false;
static SemaphoreHandle_t lock;

void logBegin()
{
  Serial.begin(115200);
  lock = xSemaphoreCreateMutex();
}

static void append(const char *s)
{
  if (lock)
    xSemaphoreTake(lock, portMAX_DELAY);
  for (; *s; s++)
  {
    ring[head++] = *s;
    if (head == RING)
    {
      head = 0;
      wrapped = true;
    }
  }
  if (lock)
    xSemaphoreGive(lock);
}

void logf(const char *fmt, ...)
{
  char buf[256];
  unsigned long ms = millis();
  int n = snprintf(buf, sizeof(buf), "[%5lu.%03lu] ", ms / 1000, ms % 1000);
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf + n, sizeof(buf) - n - 1, fmt, ap);
  va_end(ap);
  strlcat(buf, "\n", sizeof(buf));
  Serial.print(buf);
  append(buf);
}

void logTitle(const char *t)
{
  logf("==== %s ====", t);
}

void logDump(String &out)
{
  if (lock)
    xSemaphoreTake(lock, portMAX_DELAY);
  out.reserve(RING + 1);
  if (wrapped)
    out.concat(ring + head, RING - head);
  out.concat(ring, head);
  if (lock)
    xSemaphoreGive(lock);
}
