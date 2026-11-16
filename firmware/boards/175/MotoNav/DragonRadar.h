#pragma once
// Shared code-native artwork for both logical 466px canvases (1.85B scales to 360px).
// musicA.85: flash-trimmed after musicA.84 brick (grid/edge-search deferred; minimal beam + four-dot ball).
namespace DragonRadar {
static constexpr uint16_t DayBackground = 0x36C7; // RGB(48,216,56)
static constexpr uint16_t NightBackground = 0x1384;
template<class Graphics> void drawBall(Graphics *g, int x, int y) {
  g->fillCircle(x, y, 18, 0xFC60);
  g->fillCircle(x - 5, y - 7, 3, 0xFF75);
  g->fillCircle(x - 6, y - 5, 3, 0xD8A1);
  g->fillCircle(x + 6, y - 5, 3, 0xD8A1);
  g->fillCircle(x - 6, y + 6, 3, 0xD8A1);
  g->fillCircle(x + 6, y + 6, 3, 0xD8A1);
}
}
