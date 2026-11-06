#pragma once
#include <cstdint>
#include <cstddef>

// Minimal Arduino_G base used by Arduino_Canvas / panel drivers.
class Arduino_G {
public:
  Arduino_G(int16_t w, int16_t h) : WIDTH(w), HEIGHT(h), _width(w), _height(h) {}
  virtual ~Arduino_G() = default;

  virtual bool begin(int32_t /*speed*/ = 0) { return true; }
  virtual void drawPixel(int16_t, int16_t, uint16_t) {}
  virtual void fillScreen(uint16_t) {}
  virtual void draw16bitRGBBitmap(int16_t, int16_t, uint16_t *, int16_t, int16_t) {}
  virtual void draw16bitRGBBitmap(int16_t, int16_t, const uint16_t *, int16_t, int16_t) {}
  virtual void draw16bitBeRGBBitmap(int16_t, int16_t, uint16_t *, int16_t, int16_t) {}

  int16_t width() const { return _width; }
  int16_t height() const { return _height; }

  const int16_t WIDTH;
  const int16_t HEIGHT;

protected:
  int16_t _width;
  int16_t _height;
};
