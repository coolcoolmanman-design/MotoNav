#pragma once
#include <cstdint>

// CST raw-space touch inject (same coordinate space as TouchDrvCST92xx::getPoint).
void hostInjectTouch(int16_t x, int16_t y);
void hostTouchClear();
bool hostTouchPeek(int16_t *x, int16_t *y); // sticky while pressed
bool hostTouchTake(int16_t *x, int16_t *y); // consume one pending point

// SDL/mouse → CST mapping helpers (origin top-left of 466x466).
void hostTouchFromScreen(int screenX, int screenY, int lcdW = 466, int lcdH = 466);
