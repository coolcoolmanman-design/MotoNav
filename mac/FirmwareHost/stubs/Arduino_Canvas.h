#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include "Arduino_GFX.h"

#ifndef GFX_NOT_DEFINED
#define GFX_NOT_DEFINED -1
#endif

// Soft RGB565 canvas. SwapCanvas in firmware subclasses this and assigns _framebuffer.
class Arduino_Canvas : public Arduino_GFX {
public:
  Arduino_Canvas(int16_t w, int16_t h, Arduino_G *output, int16_t /*rotation*/ = 0)
      : Arduino_GFX(w, h), _output(output), _framebuffer(nullptr), _ownsFb(false) {}

  ~Arduino_Canvas() override {
    if (_ownsFb && _framebuffer) {
      std::free(_framebuffer);
      _framebuffer = nullptr;
    }
  }

  bool begin(int32_t speed = GFX_NOT_DEFINED) override {
    (void)speed;
    if (!_framebuffer) {
      size_t bytes = (size_t)_width * (size_t)_height * sizeof(uint16_t);
      _framebuffer = (uint16_t *)std::malloc(bytes);
      if (!_framebuffer) return false;
      _ownsFb = true;
      std::memset(_framebuffer, 0, bytes);
    }
    if (_output) _output->begin(speed);
    return true;
  }

  // Allow firmware SwapCanvas::setFramebuffer to re-point without freeing.
  void setFramebuffer(uint16_t *f) {
    if (_ownsFb && _framebuffer && _framebuffer != f) {
      std::free(_framebuffer);
      _ownsFb = false;
    }
    _framebuffer = f;
    _ownsFb = false;
  }

  uint16_t *getFramebuffer() override { return _framebuffer; }

  void writePixelPreclipped(int16_t x, int16_t y, uint16_t color) override {
    if (!_framebuffer) return;
    _framebuffer[(size_t)y * (size_t)_width + (size_t)x] = color;
  }

  void drawPixel(int16_t x, int16_t y, uint16_t color) override {
    if (!_framebuffer) return;
    if (x < 0 || y < 0 || x >= _width || y >= _height) return;
    writePixelPreclipped(x, y, color);
  }

  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) override {
    if (!_framebuffer || y < 0 || y >= _height || w == 0) return;
    if (w < 0) { x = (int16_t)(x + w); w = (int16_t)(-w); }
    if (x < 0) { w = (int16_t)(w + x); x = 0; }
    if (x + w > _width) w = (int16_t)(_width - x);
    if (w <= 0) return;
    uint16_t *row = _framebuffer + (size_t)y * (size_t)_width + (size_t)x;
    for (int16_t i = 0; i < w; ++i) row[i] = color;
  }

  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) override {
    if (!_framebuffer || x < 0 || x >= _width || h == 0) return;
    if (h < 0) { y = (int16_t)(y + h); h = (int16_t)(-h); }
    if (y < 0) { h = (int16_t)(h + y); y = 0; }
    if (y + h > _height) h = (int16_t)(_height - y);
    if (h <= 0) return;
    for (int16_t i = 0; i < h; ++i)
      _framebuffer[(size_t)(y + i) * (size_t)_width + (size_t)x] = color;
  }

  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override {
    if (!_framebuffer) return;
    if (w < 0) { x = (int16_t)(x + w); w = (int16_t)(-w); }
    if (h < 0) { y = (int16_t)(y + h); h = (int16_t)(-h); }
    if (x < 0) { w = (int16_t)(w + x); x = 0; }
    if (y < 0) { h = (int16_t)(h + y); y = 0; }
    if (x + w > _width) w = (int16_t)(_width - x);
    if (y + h > _height) h = (int16_t)(_height - y);
    if (w <= 0 || h <= 0) return;
    for (int16_t j = 0; j < h; ++j) {
      uint16_t *row = _framebuffer + (size_t)(y + j) * (size_t)_width + (size_t)x;
      for (int16_t i = 0; i < w; ++i) row[i] = color;
    }
  }

  void fillScreen(uint16_t color) override {
    if (!_framebuffer) return;
    size_t n = (size_t)_width * (size_t)_height;
    for (size_t i = 0; i < n; ++i) _framebuffer[i] = color;
  }

  void draw16bitRGBBitmap(int16_t x, int16_t y, uint16_t *bitmap, int16_t w, int16_t h) override {
    if (!_framebuffer || !bitmap) return;
    for (int16_t j = 0; j < h; ++j) {
      int16_t yy = (int16_t)(y + j);
      if (yy < 0 || yy >= _height) continue;
      for (int16_t i = 0; i < w; ++i) {
        int16_t xx = (int16_t)(x + i);
        if (xx < 0 || xx >= _width) continue;
        _framebuffer[(size_t)yy * (size_t)_width + (size_t)xx] = bitmap[(size_t)j * (size_t)w + (size_t)i];
      }
    }
  }

  void flush(bool force_flush = false) override {
    (void)force_flush;
    // Host present layer reads getFramebuffer(); panel draw is a no-op under HOST_SIM.
    if (_output && _framebuffer) {
      _output->draw16bitRGBBitmap(0, 0, _framebuffer, _width, _height);
    }
  }

  // Public so SwapCanvas can assign directly (firmware: `_framebuffer = f`).
  uint16_t *_framebuffer;

protected:
  Arduino_G *_output;
  bool _ownsFb;
};
