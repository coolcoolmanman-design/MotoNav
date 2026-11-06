#pragma once
#include <cstdint>

class Arduino_DataBus {
public:
  virtual ~Arduino_DataBus() = default;
  virtual bool begin(int32_t /*speed*/ = 0, int8_t /*dataMode*/ = -1) { return true; }
  virtual void beginWrite() {}
  virtual void endWrite() {}
  virtual void writeCommand(uint8_t) {}
  virtual void writeCommand16(uint16_t) {}
  virtual void write(uint8_t) {}
  virtual void write16(uint16_t) {}
  virtual void writeRepeat(uint16_t, uint32_t) {}
  virtual void writePixels(uint16_t *, uint32_t) {}
  virtual void writeBytes(uint8_t *, uint32_t) {}
  virtual void writeC8D8(uint8_t, uint8_t) {}
  virtual void writeC8D16(uint8_t, uint16_t) {}
  virtual void writeC8D16D16(uint8_t, uint16_t, uint16_t) {}
  virtual void sendCommand(uint8_t) {}
  virtual void batchOperation(const uint8_t *, uint8_t) {}
};

class Arduino_ESP32QSPI : public Arduino_DataBus {
public:
  Arduino_ESP32QSPI(int8_t /*cs*/, int8_t /*sck*/, int8_t /*d0*/, int8_t /*d1*/,
                    int8_t /*d2*/, int8_t /*d3*/, bool /*shared*/ = false) {}
  ~Arduino_ESP32QSPI() override = default;
};
