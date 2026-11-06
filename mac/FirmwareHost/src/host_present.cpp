#include "host_present.h"
#include "host_touch.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <chrono>
#include <thread>

#if defined(HOST_HAS_SDL) && HOST_HAS_SDL
#include <SDL.h>
#endif

namespace {

int gW = HOST_LCD_W;
int gH = HOST_LCD_H;
bool gQuit = false;

#if defined(HOST_HAS_SDL) && HOST_HAS_SDL
SDL_Window *gWin = nullptr;
SDL_Renderer *gRen = nullptr;
SDL_Texture *gTex = nullptr;
#endif

inline uint8_t expand5(uint16_t v) { return (uint8_t)((v << 3) | (v >> 2)); }
inline uint8_t expand6(uint16_t v) { return (uint8_t)((v << 2) | (v >> 4)); }

void rgb565ToRgba(const uint16_t *src, int w, int h, std::vector<uint8_t> &rgba) {
  rgba.resize((size_t)w * (size_t)h * 4);
  const int cx = w / 2, cy = h / 2;
  const int r2 = (cx < cy ? cx : cy) * (cx < cy ? cx : cy);
  for (int y = 0; y < h; ++y) {
    const int dy = y - cy;
    for (int x = 0; x < w; ++x) {
      const int dx = x - cx;
      const size_t i = ((size_t)y * (size_t)w + (size_t)x) * 4;
      if (dx * dx + dy * dy > r2) {
        rgba[i + 0] = 0;
        rgba[i + 1] = 0;
        rgba[i + 2] = 0;
        rgba[i + 3] = 255;
        continue;
      }
      uint16_t c = src[(size_t)y * (size_t)w + (size_t)x];
      uint8_t r = expand5((c >> 11) & 0x1f);
      uint8_t g = expand6((c >> 5) & 0x3f);
      uint8_t b = expand5(c & 0x1f);
      rgba[i + 0] = r;
      rgba[i + 1] = g;
      rgba[i + 2] = b;
      rgba[i + 3] = 255;
    }
  }
}

bool writePpm(const char *path, const std::vector<uint8_t> &rgba, int w, int h) {
  FILE *f = std::fopen(path, "wb");
  if (!f) return false;
  std::fprintf(f, "P6\n%d %d\n255\n", w, h);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const size_t i = ((size_t)y * (size_t)w + (size_t)x) * 4;
      unsigned char px[3] = {rgba[i], rgba[i + 1], rgba[i + 2]};
      std::fwrite(px, 1, 3, f);
    }
  }
  std::fclose(f);
  return true;
}

} // namespace

bool hostPresentInit(int width, int height) {
  gW = width > 0 ? width : HOST_LCD_W;
  gH = height > 0 ? height : HOST_LCD_H;
  gQuit = false;

#if defined(HOST_HAS_SDL) && HOST_HAS_SDL
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
    std::fprintf(stderr, "HOST_SIM: SDL_Init failed (%s); falling back to PPM\n", SDL_GetError());
    return true; // still OK — PPM path works without window
  }
  gWin = SDL_CreateWindow("FirmwareHost", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                          gW, gH, SDL_WINDOW_SHOWN);
  if (!gWin) {
    std::fprintf(stderr, "HOST_SIM: SDL_CreateWindow failed (%s)\n", SDL_GetError());
    return true;
  }
  gRen = SDL_CreateRenderer(gWin, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!gRen) gRen = SDL_CreateRenderer(gWin, -1, 0);
  if (!gRen) {
    std::fprintf(stderr, "HOST_SIM: SDL_CreateRenderer failed (%s)\n", SDL_GetError());
    return true;
  }
  gTex = SDL_CreateTexture(gRen, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, gW, gH);
  std::printf("HOST_SIM: SDL present %dx%d\n", gW, gH);
#else
  std::printf("HOST_SIM: headless present → /tmp/motonav_host.ppm (%dx%d)\n", gW, gH);
#endif
  return true;
}

void hostPresentShutdown() {
#if defined(HOST_HAS_SDL) && HOST_HAS_SDL
  if (gTex) { SDL_DestroyTexture(gTex); gTex = nullptr; }
  if (gRen) { SDL_DestroyRenderer(gRen); gRen = nullptr; }
  if (gWin) { SDL_DestroyWindow(gWin); gWin = nullptr; }
  SDL_Quit();
#endif
}

void hostPresentFrame(const uint16_t *rgb565, int width, int height) {
  if (!rgb565 || width <= 0 || height <= 0) return;
  std::vector<uint8_t> rgba;
  rgb565ToRgba(rgb565, width, height, rgba);

#if defined(HOST_HAS_SDL) && HOST_HAS_SDL
  if (gTex && gRen && gWin) {
    void *pixels = nullptr;
    int pitch = 0;
    if (SDL_LockTexture(gTex, nullptr, &pixels, &pitch) == 0) {
      for (int y = 0; y < height; ++y) {
        std::memcpy((uint8_t *)pixels + y * pitch, rgba.data() + (size_t)y * (size_t)width * 4,
                    (size_t)width * 4);
      }
      SDL_UnlockTexture(gTex);
    }
    SDL_RenderClear(gRen);
    SDL_RenderCopy(gRen, gTex, nullptr, nullptr);
    SDL_RenderPresent(gRen);
    return;
  }
#endif
  writePpm("/tmp/motonav_host.ppm", rgba, width, height);
}

bool hostPresentPollQuit() {
#if defined(HOST_HAS_SDL) && HOST_HAS_SDL
  SDL_Event e;
  while (SDL_PollEvent(&e)) {
    if (e.type == SDL_QUIT) gQuit = true;
    if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) gQuit = true;
  }
#endif
  return gQuit;
}

void hostPresentPumpTouch() {
#if defined(HOST_HAS_SDL) && HOST_HAS_SDL
  int mx = 0, my = 0;
  uint32_t buttons = SDL_GetMouseState(&mx, &my);
  if (buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) {
    hostTouchFromScreen(mx, my, gW, gH);
  } else {
    hostTouchClear();
  }
#endif
}

void hostPresentDelay(uint32_t ms) {
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}
