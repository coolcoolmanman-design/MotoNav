#pragma once
#include <cstdint>
#include "Wire.h"

// Declared in src/host_touch.cpp — CST raw-space inject queue.
void hostInjectTouch(int16_t x, int16_t y);
bool hostTouchPeek(int16_t *x, int16_t *y);
bool hostTouchTake(int16_t *x, int16_t *y);

// Minimal SensorLib-compatible CST92xx stub. Coordinates are CST raw space;
// host_touch injects points via hostInjectTouch.
class TouchDrvCST92xx {
public:
  void setPins(int8_t rst, int8_t irq) { _rst = rst; _irq = irq; }

  bool begin(TwoWire &wire, uint8_t addr, int sda, int scl) {
    (void)wire; (void)addr; (void)sda; (void)scl;
    _ok = true;
    return true;
  }
  bool begin(TwoWire *wire, uint8_t addr, int sda, int scl) {
    return begin(*wire, addr, sda, scl);
  }

  // Sticky contact while host mouse/finger is down (matches CST: repeated getPoint).
  uint8_t getPoint(int16_t *x, int16_t *y, uint8_t /*max*/ = 1) {
    if (!_ok || !x || !y) return 0;
    int16_t hx = 0, hy = 0;
    // Peek — do not clear; hostTouchClear() on mouse-up ends the contact.
    if (!hostTouchPeek(&hx, &hy)) return 0;
    x[0] = hx;
    y[0] = hy;
    return 1;
  }

  void reset() {}
  void sleep() {}
  void wakeup() {}
  void setMaxCoordinates(int16_t, int16_t) {}
  void setSwapXY(bool) {}
  void setMirrorXY(bool, bool) {}

  void hostSetPoint(int16_t x, int16_t y) { hostInjectTouch(x, y); }

private:
  int8_t _rst = -1, _irq = -1;
  bool _ok = false;
};

using TouchDrvCSTXXX = TouchDrvCST92xx;
