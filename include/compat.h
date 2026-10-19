// Portability shim so rendering, parsing and settings code builds both for the
// ESP32 (Arduino) and for the host (`pio test -e native`, preview renderer).
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#if defined(ARDUINO)
#  include <Arduino.h>
#else
#  ifndef PROGMEM
#    define PROGMEM
#  endif
#  ifndef pgm_read_byte
#    define pgm_read_byte(addr) (*(const uint8_t *)(addr))
#  endif
// glibc gained strlcpy only recently; provide a portable fallback.
#  if !defined(__APPLE__) && !defined(__FreeBSD__) && !defined(strlcpy)
static inline size_t m17_strlcpy(char *dst, const char *src, size_t size)
{
  size_t len = strlen(src);
  if (size) {
    size_t n = len >= size ? size - 1 : len;
    memcpy(dst, src, n);
    dst[n] = 0;
  }
  return len;
}
#    define strlcpy m17_strlcpy
static inline size_t m17_strlcat(char *dst, const char *src, size_t size)
{
  size_t d = strnlen(dst, size);
  if (d == size)
    return size + strlen(src);
  return d + m17_strlcpy(dst + d, src, size - d);
}
#    define strlcat m17_strlcat
#  endif
#endif
