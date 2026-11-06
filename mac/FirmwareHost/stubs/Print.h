#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include "WString.h"

class Print {
public:
  virtual ~Print() = default;
  virtual size_t write(uint8_t c) = 0;
  virtual size_t write(const uint8_t *buf, size_t size) {
    size_t n = 0;
    if (!buf) return 0;
    while (n < size) {
      if (write(buf[n]) == 0) break;
      ++n;
    }
    return n;
  }
  size_t write(const char *str) {
    if (!str) return 0;
    return write((const uint8_t *)str, std::strlen(str));
  }

  size_t print(const char *s) { return write(s ? s : ""); }
  size_t print(const String &s) { return write(s.c_str()); }
  size_t print(char c) { return write((uint8_t)c); }
  size_t print(int v, int /*base*/ = 10) {
    char b[32];
    std::snprintf(b, sizeof(b), "%d", v);
    return write(b);
  }
  size_t print(unsigned v, int /*base*/ = 10) {
    char b[32];
    std::snprintf(b, sizeof(b), "%u", v);
    return write(b);
  }
  size_t print(long v, int /*base*/ = 10) {
    char b[32];
    std::snprintf(b, sizeof(b), "%ld", v);
    return write(b);
  }
  size_t print(unsigned long v, int /*base*/ = 10) {
    char b[32];
    std::snprintf(b, sizeof(b), "%lu", v);
    return write(b);
  }
  size_t print(float v, int digits = 2) {
    char b[64];
    std::snprintf(b, sizeof(b), "%.*f", digits, (double)v);
    return write(b);
  }
  size_t print(double v, int digits = 2) {
    char b[64];
    std::snprintf(b, sizeof(b), "%.*f", digits, v);
    return write(b);
  }

  size_t println() { return write((uint8_t)'\n'); }
  size_t println(const char *s) { size_t n = print(s); return n + println(); }
  size_t println(const String &s) { size_t n = print(s); return n + println(); }
  size_t println(char c) { size_t n = print(c); return n + println(); }
  size_t println(int v, int base = 10) { size_t n = print(v, base); return n + println(); }
  size_t println(unsigned v, int base = 10) { size_t n = print(v, base); return n + println(); }
  size_t println(long v, int base = 10) { size_t n = print(v, base); return n + println(); }
  size_t println(unsigned long v, int base = 10) { size_t n = print(v, base); return n + println(); }
  size_t println(float v, int digits = 2) { size_t n = print(v, digits); return n + println(); }
  size_t println(double v, int digits = 2) { size_t n = print(v, digits); return n + println(); }

  size_t printf(const char *fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    int n = std::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n <= 0) return 0;
    if ((size_t)n >= sizeof(buf)) n = (int)sizeof(buf) - 1;
    return write((const uint8_t *)buf, (size_t)n);
  }
};
