#pragma once
#include "Stream.h"

class HardwareSerial : public Stream {
public:
  void begin(unsigned long /*baud*/) {}
  void begin(unsigned long /*baud*/, uint32_t /*config*/) {}
  void end() {}
  void setTxTimeoutMs(uint32_t) {}
  bool availableForWrite() { return true; }
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  void flush() override {}
  size_t write(uint8_t c) override {
    std::fputc((int)c, stdout);
    return 1;
  }
  size_t write(const uint8_t *buf, size_t size) override {
    if (!buf || !size) return 0;
    return std::fwrite(buf, 1, size, stdout);
  }
};

static HardwareSerial Serial;
#ifndef Serial0
#define Serial0 Serial
#endif
