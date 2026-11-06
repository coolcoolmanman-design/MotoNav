#pragma once
// HOST_SIM aggregate header matching Arduino_GFX_Library include used by DeviceContext.
#include "Arduino_G.h"
#include "Arduino_DataBus.h"
#include "Arduino_GFX.h"
#include "Arduino_Canvas.h"

#ifndef GFX_NOT_DEFINED
#define GFX_NOT_DEFINED -1
#endif

// CO5300 AMOLED panel driver — no-op under host; soft canvas owns pixels.
class Arduino_CO5300 : public Arduino_GFX {
public:
  Arduino_CO5300(Arduino_DataBus *bus, int8_t /*rst*/, uint8_t /*r*/,
                 int16_t w, int16_t h,
                 uint8_t /*col_offset1*/ = 0, uint8_t /*row_offset1*/ = 0,
                 uint8_t /*col_offset2*/ = 0, uint8_t /*row_offset2*/ = 0)
      : Arduino_GFX(w, h), _bus(bus) {}

  bool begin(int32_t speed = GFX_NOT_DEFINED) override {
    if (_bus) _bus->begin(speed);
    return true;
  }

  void setBrightness(uint8_t /*brightness*/) {}
  void displayOn() {}
  void displayOff() {}
  void draw16bitRGBBitmap(int16_t, int16_t, uint16_t *, int16_t, int16_t) override {}
  void draw16bitRGBBitmap(int16_t, int16_t, const uint16_t *, int16_t, int16_t) override {}
  void drawPixel(int16_t, int16_t, uint16_t) override {}
  void writeAddrWindow(int16_t, int16_t, uint16_t, uint16_t) override {}
  void startWrite() override { if (_bus) _bus->beginWrite(); }
  void endWrite() override { if (_bus) _bus->endWrite(); }

private:
  Arduino_DataBus *_bus;
};
