#pragma once
#include <cstdint>
#include <cstddef>

#ifndef HOST_LCD_W
#define HOST_LCD_W 466
#endif
#ifndef HOST_LCD_H
#define HOST_LCD_H 466
#endif

// Present RGB565 framebuffer with circular clip (diameter = min(w,h)).
// SDL window when HOST_HAS_SDL; otherwise writes /tmp/motonav_host.ppm.
bool hostPresentInit(int width = HOST_LCD_W, int height = HOST_LCD_H);
void hostPresentShutdown();
void hostPresentFrame(const uint16_t *rgb565, int width, int height);
bool hostPresentPollQuit(); // true if user closed window / requested quit
void hostPresentPumpTouch(); // SDL mouse → CST raw inject (TOUCH_FLIP-aware)
void hostPresentDelay(uint32_t ms);
