#pragma once
#include "Stream.h"

class TwoWire : public Stream {
public:
  void begin() {}
  void begin(int /*sda*/, int /*scl*/) {}
  void begin(int /*sda*/, int /*scl*/, uint32_t /*freq*/) {}
  void setClock(uint32_t) {}
  void end() {}
  void beginTransmission(uint8_t) {}
  // Default NACK — no PMU/IMU/touch silicon on host.
  uint8_t endTransmission(bool /*stop*/ = true) { return 2; }
  uint8_t requestFrom(uint8_t, uint8_t) { return 0; }
  uint8_t requestFrom(int, int) { return 0; }
  size_t write(uint8_t c) override { (void)c; return 1; }
  size_t write(const uint8_t *buf, size_t size) override { (void)buf; return size; }
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
};

static TwoWire Wire;
