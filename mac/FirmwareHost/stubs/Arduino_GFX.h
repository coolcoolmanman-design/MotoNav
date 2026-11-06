#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>
#include "Arduino_G.h"
#include "Print.h"

#ifndef GFX_BLACK
#define GFX_BLACK 0x0000
#endif
#ifndef GFX_WHITE
#define GFX_WHITE 0xFFFF
#endif

class Arduino_GFX : public Print, public Arduino_G {
public:
  Arduino_GFX(int16_t w, int16_t h) : Arduino_G(w, h) {}
  ~Arduino_GFX() override = default;

  virtual void startWrite() {}
  virtual void endWrite() {}
  virtual void writePixelPreclipped(int16_t x, int16_t y, uint16_t color) { drawPixel(x, y, color); }
  virtual void writePixel(int16_t x, int16_t y, uint16_t color) {
    if (x < 0 || y < 0 || x >= _width || y >= _height) return;
    writePixelPreclipped(x, y, color);
  }
  virtual void writeFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
    drawFastVLine(x, y, h, color);
  }
  virtual void writeFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
    drawFastHLine(x, y, w, color);
  }
  virtual void writeFillRectPreclipped(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    fillRect(x, y, w, h, color);
  }
  virtual void writeAddrWindow(int16_t, int16_t, uint16_t, uint16_t) {}

  void drawPixel(int16_t x, int16_t y, uint16_t color) override {
    (void)x; (void)y; (void)color;
  }

  virtual void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
    if (w < 0) { x += w; w = -w; }
    for (int16_t i = 0; i < w; ++i) writePixel(x + i, y, color);
  }
  virtual void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
    if (h < 0) { y += h; h = -h; }
    for (int16_t i = 0; i < h; ++i) writePixel(x, y + i, color);
  }
  virtual void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
    int16_t dx = (int16_t)std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int16_t dy = (int16_t)-std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int16_t err = dx + dy;
    for (;;) {
      writePixel(x0, y0, color);
      if (x0 == x1 && y0 == y1) break;
      int16_t e2 = (int16_t)(2 * err);
      if (e2 >= dy) { err = (int16_t)(err + dy); x0 = (int16_t)(x0 + sx); }
      if (e2 <= dx) { err = (int16_t)(err + dx); y0 = (int16_t)(y0 + sy); }
    }
  }
  virtual void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    drawFastHLine(x, y, w, color);
    drawFastHLine(x, (int16_t)(y + h - 1), w, color);
    drawFastVLine(x, y, h, color);
    drawFastVLine((int16_t)(x + w - 1), y, h, color);
  }
  virtual void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (w < 0) { x = (int16_t)(x + w); w = (int16_t)(-w); }
    if (h < 0) { y = (int16_t)(y + h); h = (int16_t)(-h); }
    for (int16_t j = 0; j < h; ++j) drawFastHLine(x, (int16_t)(y + j), w, color);
  }
  void fillScreen(uint16_t color) override { fillRect(0, 0, _width, _height, color); }

  virtual void drawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) {
    int16_t f = (int16_t)(1 - r), ddF_x = 1, ddF_y = (int16_t)(-2 * r), x = 0, y = r;
    writePixel(x0, (int16_t)(y0 + r), color);
    writePixel(x0, (int16_t)(y0 - r), color);
    writePixel((int16_t)(x0 + r), y0, color);
    writePixel((int16_t)(x0 - r), y0, color);
    while (x < y) {
      if (f >= 0) { y--; ddF_y = (int16_t)(ddF_y + 2); f = (int16_t)(f + ddF_y); }
      x++; ddF_x = (int16_t)(ddF_x + 2); f = (int16_t)(f + ddF_x);
      writePixel((int16_t)(x0 + x), (int16_t)(y0 + y), color);
      writePixel((int16_t)(x0 - x), (int16_t)(y0 + y), color);
      writePixel((int16_t)(x0 + x), (int16_t)(y0 - y), color);
      writePixel((int16_t)(x0 - x), (int16_t)(y0 - y), color);
      writePixel((int16_t)(x0 + y), (int16_t)(y0 + x), color);
      writePixel((int16_t)(x0 - y), (int16_t)(y0 + x), color);
      writePixel((int16_t)(x0 + y), (int16_t)(y0 - x), color);
      writePixel((int16_t)(x0 - y), (int16_t)(y0 - x), color);
    }
  }
  virtual void fillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) {
    drawFastVLine(x0, (int16_t)(y0 - r), (int16_t)(2 * r + 1), color);
    fillCircleHelper(x0, y0, r, 3, 0, color);
  }
  virtual void fillCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t corners, int16_t delta, uint16_t color) {
    int16_t f = (int16_t)(1 - r), ddF_x = 1, ddF_y = (int16_t)(-2 * r), x = 0, y = r;
    while (x < y) {
      if (f >= 0) { y--; ddF_y = (int16_t)(ddF_y + 2); f = (int16_t)(f + ddF_y); }
      x++; ddF_x = (int16_t)(ddF_x + 2); f = (int16_t)(f + ddF_x);
      if (corners & 0x1) {
        drawFastVLine((int16_t)(x0 + x), (int16_t)(y0 - y), (int16_t)(2 * y + 1 + delta), color);
        drawFastVLine((int16_t)(x0 + y), (int16_t)(y0 - x), (int16_t)(2 * x + 1 + delta), color);
      }
      if (corners & 0x2) {
        drawFastVLine((int16_t)(x0 - x), (int16_t)(y0 - y), (int16_t)(2 * y + 1 + delta), color);
        drawFastVLine((int16_t)(x0 - y), (int16_t)(y0 - x), (int16_t)(2 * x + 1 + delta), color);
      }
    }
  }

  virtual void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color) {
    // Sort by Y
    if (y0 > y1) { std::swap(y0, y1); std::swap(x0, x1); }
    if (y1 > y2) { std::swap(y1, y2); std::swap(x1, x2); }
    if (y0 > y1) { std::swap(y0, y1); std::swap(x0, x1); }
    if (y0 == y2) {
      int16_t a = x0, b = x0;
      if (x1 < a) a = x1; else if (x1 > b) b = x1;
      if (x2 < a) a = x2; else if (x2 > b) b = x2;
      drawFastHLine(a, y0, (int16_t)(b - a + 1), color);
      return;
    }
    int32_t dx01 = x1 - x0, dy01 = y1 - y0;
    int32_t dx02 = x2 - x0, dy02 = y2 - y0;
    int32_t dx12 = x2 - x1, dy12 = y2 - y1;
    int32_t sa = 0, sb = 0;
    int16_t y;
    int16_t last = (y1 == y2) ? y1 : (int16_t)(y1 - 1);
    for (y = y0; y <= last; ++y) {
      int16_t a = (int16_t)(x0 + sa / (dy01 ? dy01 : 1));
      int16_t b = (int16_t)(x0 + sb / (dy02 ? dy02 : 1));
      sa += dx01; sb += dx02;
      if (a > b) std::swap(a, b);
      drawFastHLine(a, y, (int16_t)(b - a + 1), color);
    }
    sa = dx12 * (y - y1);
    sb = dx02 * (y - y0);
    for (; y <= y2; ++y) {
      int16_t a = (int16_t)(x1 + sa / (dy12 ? dy12 : 1));
      int16_t b = (int16_t)(x0 + sb / (dy02 ? dy02 : 1));
      sa += dx12; sb += dx02;
      if (a > b) std::swap(a, b);
      drawFastHLine(a, y, (int16_t)(b - a + 1), color);
    }
  }

  virtual void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color) {
    drawFastHLine((int16_t)(x + r), y, (int16_t)(w - 2 * r), color);
    drawFastHLine((int16_t)(x + r), (int16_t)(y + h - 1), (int16_t)(w - 2 * r), color);
    drawFastVLine(x, (int16_t)(y + r), (int16_t)(h - 2 * r), color);
    drawFastVLine((int16_t)(x + w - 1), (int16_t)(y + r), (int16_t)(h - 2 * r), color);
    // corner arcs approximated via circle quarters
    drawCircleHelper(x + r, y + r, r, 1, color);
    drawCircleHelper(x + w - r - 1, y + r, r, 2, color);
    drawCircleHelper(x + w - r - 1, y + h - r - 1, r, 4, color);
    drawCircleHelper(x + r, y + h - r - 1, r, 8, color);
  }
  virtual void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color) {
    fillRect((int16_t)(x + r), y, (int16_t)(w - 2 * r), h, color);
    fillCircleHelper((int16_t)(x + w - r - 1), (int16_t)(y + r), r, 1, (int16_t)(h - 2 * r - 1), color);
    fillCircleHelper((int16_t)(x + r), (int16_t)(y + r), r, 2, (int16_t)(h - 2 * r - 1), color);
  }
  virtual void drawCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t cornername, uint16_t color) {
    int16_t f = (int16_t)(1 - r), ddF_x = 1, ddF_y = (int16_t)(-2 * r), x = 0, y = r;
    while (x < y) {
      if (f >= 0) { y--; ddF_y = (int16_t)(ddF_y + 2); f = (int16_t)(f + ddF_y); }
      x++; ddF_x = (int16_t)(ddF_x + 2); f = (int16_t)(f + ddF_x);
      if (cornername & 0x4) {
        writePixel((int16_t)(x0 + x), (int16_t)(y0 + y), color);
        writePixel((int16_t)(x0 + y), (int16_t)(y0 + x), color);
      }
      if (cornername & 0x2) {
        writePixel((int16_t)(x0 + x), (int16_t)(y0 - y), color);
        writePixel((int16_t)(x0 + y), (int16_t)(y0 - x), color);
      }
      if (cornername & 0x8) {
        writePixel((int16_t)(x0 - y), (int16_t)(y0 + x), color);
        writePixel((int16_t)(x0 - x), (int16_t)(y0 + y), color);
      }
      if (cornername & 0x1) {
        writePixel((int16_t)(x0 - y), (int16_t)(y0 - x), color);
        writePixel((int16_t)(x0 - x), (int16_t)(y0 - y), color);
      }
    }
  }

  void draw16bitRGBBitmap(int16_t x, int16_t y, uint16_t *bitmap, int16_t w, int16_t h) override {
    if (!bitmap) return;
    for (int16_t j = 0; j < h; ++j)
      for (int16_t i = 0; i < w; ++i)
        writePixel((int16_t)(x + i), (int16_t)(y + j), bitmap[j * w + i]);
  }
  void draw16bitRGBBitmap(int16_t x, int16_t y, const uint16_t *bitmap, int16_t w, int16_t h) override {
    draw16bitRGBBitmap(x, y, const_cast<uint16_t *>(bitmap), w, h);
  }

  virtual void setTextSize(uint8_t s) { _textSize = s ? s : 1; }
  virtual void setTextSize(uint8_t sx, uint8_t sy) {
    _textSize = sx ? sx : 1;
    (void)sy;
  }
  virtual void setTextColor(uint16_t c) { _textColor = c; _textBg = c; }
  virtual void setTextColor(uint16_t c, uint16_t bg) { _textColor = c; _textBg = bg; }
  virtual void setCursor(int16_t x, int16_t y) { _cursorX = x; _cursorY = y; }
  virtual void setTextWrap(bool w) { _wrap = w; }
  int16_t getCursorX() const { return _cursorX; }
  int16_t getCursorY() const { return _cursorY; }

  size_t write(uint8_t c) override {
    // Minimal 5x7 glyph stub: advance cursor, draw a filled block for printable chars.
    if (c == '\n') {
      _cursorX = 0;
      _cursorY = (int16_t)(_cursorY + 8 * _textSize);
      return 1;
    }
    if (c == '\r') return 1;
    if (c >= 32) {
      fillRect(_cursorX, _cursorY, (int16_t)(5 * _textSize), (int16_t)(7 * _textSize), _textColor);
      _cursorX = (int16_t)(_cursorX + 6 * _textSize);
      if (_wrap && _cursorX > _width - 6 * _textSize) {
        _cursorX = 0;
        _cursorY = (int16_t)(_cursorY + 8 * _textSize);
      }
    }
    return 1;
  }

  virtual void flush(bool /*force_flush*/ = false) {}
  virtual uint16_t *getFramebuffer() { return nullptr; }

protected:
  uint8_t _textSize = 1;
  uint16_t _textColor = GFX_WHITE;
  uint16_t _textBg = GFX_BLACK;
  int16_t _cursorX = 0, _cursorY = 0;
  bool _wrap = true;
};
