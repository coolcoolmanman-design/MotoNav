#include "host_touch.h"

#include <atomic>
#include <mutex>

namespace {
std::mutex gMu;
bool gHas = false;
int16_t gX = 0, gY = 0;
} // namespace

void hostInjectTouch(int16_t x, int16_t y) {
  std::lock_guard<std::mutex> lock(gMu);
  gX = x;
  gY = y;
  gHas = true;
}

void hostTouchClear() {
  std::lock_guard<std::mutex> lock(gMu);
  gHas = false;
}

bool hostTouchPeek(int16_t *x, int16_t *y) {
  std::lock_guard<std::mutex> lock(gMu);
  if (!gHas) return false;
  if (x) *x = gX;
  if (y) *y = gY;
  return true;
}

bool hostTouchTake(int16_t *x, int16_t *y) {
  std::lock_guard<std::mutex> lock(gMu);
  if (!gHas) return false;
  if (x) *x = gX;
  if (y) *y = gY;
  gHas = false;
  return true;
}

void hostTouchFromScreen(int screenX, int screenY, int lcdW, int lcdH) {
  if (lcdW <= 0) lcdW = 466;
  if (lcdH <= 0) lcdH = 466;
  // Mouse is in canvas/pixel space (what the window shows). Device hit-tests apply
  // TOUCH_FLIP_X/Y=1 at rot0: canvas = 2*CX - raw. Invert so OK/music hit-tests match.
  const int cx = lcdW / 2;
  const int cy = lcdH / 2;
  int rawX = 2 * cx - screenX;
  int rawY = 2 * cy - screenY;
  if (rawX < 0) rawX = 0;
  if (rawY < 0) rawY = 0;
  if (rawX >= lcdW) rawX = lcdW - 1;
  if (rawY >= lcdH) rawY = lcdH - 1;
  hostInjectTouch((int16_t)rawX, (int16_t)rawY);
}
