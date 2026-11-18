#pragma once
// Shared code-native artwork for both logical 466px canvases (1.85B scales to 360px).
// musicA.102: restore cartesian square grid (step=40) for dragonball radar v3; keep slim four-dot ball.
namespace DragonRadar {
static constexpr uint16_t DayBackground = 0x36C7; // RGB(48,216,56)
static constexpr uint16_t NightBackground = 0x1384;
template<class Graphics> void drawGrid(Graphics *g, int width, int height, bool night) {
  const uint16_t minor = night ? 0x1263 : 0x25A5;
  const uint16_t major = night ? 0x0941 : 0x1383;
  const int step = 40, cx = width / 2, cy = height / 2;
  for (int k = -6; k <= 6; ++k) {
    const uint16_t color = k % 5 == 0 ? major : minor;
    int x = cx + k * step, y = cy + k * step;
    if (x >= 0 && x < width) g->drawFastVLine(x, 0, height, color);
    if (y >= 0 && y < height) g->drawFastHLine(0, y, width, color);
  }
  g->drawFastVLine(cx + 1, 0, height, major);
  g->drawFastHLine(0, cy + 1, width, major);
}
template<class Graphics> void drawBall(Graphics *g, int x, int y) {
  g->fillCircle(x, y, 18, 0xFC60);
  g->fillCircle(x - 5, y - 7, 3, 0xFF75);
  g->fillCircle(x - 6, y - 5, 3, 0xD8A1);
  g->fillCircle(x + 6, y - 5, 3, 0xD8A1);
  g->fillCircle(x - 6, y + 6, 3, 0xD8A1);
  g->fillCircle(x + 6, y + 6, 3, 0xD8A1);
}
}
