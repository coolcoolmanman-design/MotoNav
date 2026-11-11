#pragma once
// musicA.38 左划音乐页主题渲染（由 PageNavigation.h 在静态 helper 之后 include）。
// 样式 ID：0 原版·冷雾银 / 1 封面轮播 / 2 柔光立体 / 3 歌词主导 / 4 极简仪表 / 5 全屏转动封面（方案 B）。
// 共用 coverImg / imgs[5,6,12] / appleMedia 进度；播控命中见 musicButtonAt()。

static const uint16_t MS_ORANGE = 0xFBE4;     // #F97B20 近似
static const uint16_t MS_NEON   = 0x5E7F;     // 柔光青
static const uint16_t MS_WHITE  = 0xFFFF;
static const uint16_t MS_DIM    = 0x8410;

static inline uint16_t musicDim565(uint16_t c, uint32_t k) {   // k: 0~32，32 = 原样
  uint32_t r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
  return (uint16_t)(((r * k >> 5) << 11) | ((g * k >> 5) << 5) | (b * k >> 5));
}
static inline int musicAbs(int v) { return v < 0 ? -v : v; }

static inline bool musicLyricsReady() {
  return musicLyrics && hidMusicMode && appleMedia.available && imgs[12].showW > 0;
}

static inline double musicElapsedNow(double *outDur) {
  *outDur = 0;
  if (hidMusicMode && appleMedia.available) {
    double duration = AppleMediaData::duration(appleMedia.values[3].text.c_str());
    if (duration > 0 && appleMedia.playback.known) {
      *outDur = duration;
      return appleMedia.playback.elapsed + appleMedia.playback.rate * (millis() - appleMedia.playbackAt) / 1000.0;
    }
  } else if (musicDurMs > 0) {
    *outDur = musicDurMs / 1000.0;
    double elapsed = musicElapsedMs / 1000.0;
    if (musicPlaying && musicProgAt) elapsed += (millis() - musicProgAt) / 1000.0;
    return elapsed;
  }
  return 0;
}

// tintMode：0 原色 / 1 冷青偏色（上一首占位）/ 2 暖洋红偏色（下一首占位）
static inline uint16_t musicTint565(uint16_t c, int tintMode) {
  if (tintMode == 0) return c;
  uint32_t r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
  if (tintMode == 1) {          // 冷青：压红抬青
    r = (r * 10) / 32;
    g = (g * 22 + 20) / 32;
    b = (b * 18 + 40) / 32;
  } else {                      // 暖洋红：抬红压绿
    r = (r * 20 + 40) / 32;
    g = (g * 12) / 32;
    b = (b * 16 + 20) / 32;
  }
  if (r > 31) r = 31; if (g > 63) g = 63; if (b > 31) b = 31;
  return (uint16_t)((r << 11) | (g << 5) | b);
}

// 圆角方封：src 可指向当前封 / 历史封；tint+cropBias 保证邻封不会与中封长得一样。
static void drawMusicRoundedSquareSrc(const uint16_t *src, int sw, int sh,
                                      int cx, int cy, int half, int radius,
                                      int dimK, int tintMode, int cropBias) {
  if (!src || sw <= 0 || sh <= 0) return;
  musicEnsureChords();
  uint16_t *fb = gfx->getFramebuffer();
  // cropBias：邻封用偏裁，避免与中封同一构图
  const int biasU = (cropBias * sw) / 8;
  const int biasV = (cropBias * sh) / 10;
  const int scale = (sw << 16) / (2 * half);
  const int cx0 = (sw << 15) + (biasU << 16);
  const int cy0 = (sh << 15) + (biasV << 16);
  const int r2 = radius * radius;
  for (int y = cy - half; y < cy + half; y++) {
    if ((unsigned)y >= (unsigned)LCD_HEIGHT) continue;
    int h = chordHalf[y];
    if (!h) continue;
    int dy = y - cy;
    for (int x = cx - half; x < cx + half; x++) {
      if (x < CX - h || x >= CX + h) continue;
      int dx = x - cx;
      int ax = musicAbs(dx) - (half - radius);
      int ay = musicAbs(dy) - (half - radius);
      if (ax > 0 && ay > 0 && ax * ax + ay * ay > r2) continue;
      int ui = ((dx * scale) >> 16) + (cx0 >> 16);
      int vi = ((dy * scale) >> 16) + (cy0 >> 16);
      // 越界回绕，邻封图案更碎、更不像中封
      if (ui < 0) ui = (-ui) % sw; else if (ui >= sw) ui = ui % sw;
      if (vi < 0) vi = (-vi) % sh; else if (vi >= sh) vi = vi % sh;
      uint16_t c = src[(size_t)vi * sw + ui];
      c = musicTint565(c, tintMode);
      if (dimK < 32) c = musicDim565(c, (uint32_t)dimK);
      fb[(size_t)y * LCD_WIDTH + x] = c;
    }
  }
}

static void drawMusicRoundedSquare(int cx, int cy, int half, int radius, int dimK, float /*angleRad*/) {
  if (!coverValid || !coverImg) return;
  const uint16_t *src = (coverHiReady && coverHi) ? coverHi : coverImg;
  const int sw = (coverHiReady && coverHi) ? COVER_HI : coverW;
  const int sh = (coverHiReady && coverHi) ? COVER_HI : coverH;
  drawMusicRoundedSquareSrc(src, sw, sh, cx, cy, half, radius, dimK, 0, 0);
}

// 无真图时的几何占位方封（左右各用不同底色/斜纹，绝不是同一张图）。
static void drawMusicPlaceholderSquare(int cx, int cy, int half, int radius, int variant) {
  musicEnsureChords();
  uint16_t *fb = gfx->getFramebuffer();
  const uint16_t base = (variant == 0) ? (uint16_t)0x29B2 : (uint16_t)0xA146;  // 冷青 / 暖橙
  const uint16_t stripe = (variant == 0) ? (uint16_t)0x3D17 : (uint16_t)0xFBE4;
  const int r2 = radius * radius;
  for (int y = cy - half; y < cy + half; y++) {
    if ((unsigned)y >= (unsigned)LCD_HEIGHT) continue;
    int h = chordHalf[y];
    if (!h) continue;
    int dy = y - cy;
    for (int x = cx - half; x < cx + half; x++) {
      if (x < CX - h || x >= CX + h) continue;
      int dx = x - cx;
      int ax = musicAbs(dx) - (half - radius);
      int ay = musicAbs(dy) - (half - radius);
      if (ax > 0 && ay > 0 && ax * ax + ay * ay > r2) continue;
      int band = ((dx + dy + (variant ? 20 : 0)) >> 3) & 3;
      uint16_t c = (band == 0) ? stripe : base;
      c = musicDim565(c, 14);
      fb[(size_t)y * LCD_WIDTH + x] = c;
    }
  }
}

// 小圆封面（歌词主导 / 极简仪表）。
static void drawMusicSmallDisc(int cx, int cy, int R) {
  if (!coverValid || !coverImg) return;
  const uint16_t *src = coverImg;
  const int sw = coverW, sh = coverH;
  if (!src || sw <= 0 || sh <= 0) return;
  musicEnsureChords();
  uint16_t *fb = gfx->getFramebuffer();
  const int R2 = R * R;
  for (int y = cy - R; y <= cy + R; y++) {
    if ((unsigned)y >= (unsigned)LCD_HEIGHT) continue;
    int h = chordHalf[y];
    if (!h) continue;
    int dy = y - cy;
    for (int x = cx - R; x <= cx + R; x++) {
      int dx = x - cx;
      if (dx * dx + dy * dy > R2) continue;
      if (x < CX - h || x >= CX + h) continue;
      int ui = (dx + R) * sw / (2 * R);
      int vi = (dy + R) * sh / (2 * R);
      if ((unsigned)ui >= (unsigned)sw || (unsigned)vi >= (unsigned)sh) continue;
      fb[(size_t)y * LCD_WIDTH + x] = src[(size_t)vi * sw + ui];
    }
  }
}

// 橙色进度沿方封外沿（从顶边中点顺时针）。
static void drawSquareProgress(int cx, int cy, int half, float t) {
  if (t < 0) t = 0; else if (t > 1) t = 1;
  const int peri = half * 8;
  const int steps = (int)(peri * t);
  musicEnsureChords();
  uint16_t *fb = gfx->getFramebuffer();
  auto put = [&](int x, int y) {
    if ((unsigned)x >= (unsigned)LCD_WIDTH || (unsigned)y >= (unsigned)LCD_HEIGHT) return;
    int h = chordHalf[y];
    if (!h || x < CX - h || x >= CX + h) return;
    fb[(size_t)y * LCD_WIDTH + x] = MS_ORANGE;
    if (x + 1 < LCD_WIDTH) fb[(size_t)y * LCD_WIDTH + x + 1] = MS_ORANGE;
  };
  for (int i = 0; i <= steps; i++) {
    int edge = i / (half * 2);
    int o = i % (half * 2);
    int x = cx, y = cy;
    if (edge == 0)      { x = cx - half + o; y = cy - half - 2; }       // 顶
    else if (edge == 1) { x = cx + half + 2; y = cy - half + o; }       // 右
    else if (edge == 2) { x = cx + half - o; y = cy + half + 2; }       // 底
    else                { x = cx - half - 2; y = cy + half - o; }       // 左
    put(x, y);
  }
}

// 霓虹播控球（柔光立体）。
static void drawNeonPlayOrb(int cx, int cy, int rad, bool playing, bool pressed) {
  musicEnsureChords();
  uint16_t *fb = gfx->getFramebuffer();
  const int r2 = rad * rad, rOut2 = (rad + 3) * (rad + 3);
  for (int dy = -rad - 3; dy <= rad + 3; dy++) {
    int y = cy + dy;
    if ((unsigned)y >= (unsigned)LCD_HEIGHT) continue;
    int h = chordHalf[y];
    if (!h) continue;
    for (int dx = -rad - 3; dx <= rad + 3; dx++) {
      int d2 = dx * dx + dy * dy;
      if (d2 > rOut2) continue;
      int x = cx + dx;
      if (x < CX - h || x >= CX + h) continue;
      int a = d2 <= r2 ? (pressed ? 110 : 80) : 160 * (rOut2 - d2) / (rOut2 - r2 + 1);
      uint16_t *px = &fb[(size_t)y * LCD_WIDTH + x];
      *px = blend565(*px, MS_NEON, a);
    }
  }
  // 内环
  for (int ang = 0; ang < 360; ang += 2) {
    float ar = ang * (float)PI / 180.0f;
    int x = cx + (int)lroundf((rad - 2) * cosf(ar));
    int y = cy + (int)lroundf((rad - 2) * sinf(ar));
    if ((unsigned)x >= (unsigned)LCD_WIDTH || (unsigned)y >= (unsigned)LCD_HEIGHT) continue;
    int h = chordHalf[y];
    if (!h || x < CX - h || x >= CX + h) continue;
    fb[(size_t)y * LCD_WIDTH + x] = blend565(fb[(size_t)y * LCD_WIDTH + x], MS_WHITE, 200);
  }
  drawMusicControlIcons(cx, cy, 1, false, playing, MS_WHITE);
}

// 高对比白底播控（极简仪表）。
static void drawWhiteTransport(int cx, int cy, int rad, int i, bool enabled, bool pressed, bool playing) {
  musicEnsureChords();
  uint16_t *fb = gfx->getFramebuffer();
  const int r2 = rad * rad;
  for (int dy = -rad; dy <= rad; dy++) {
    int y = cy + dy;
    if ((unsigned)y >= (unsigned)LCD_HEIGHT) continue;
    int h = chordHalf[y];
    if (!h) continue;
    for (int dx = -rad; dx <= rad; dx++) {
      if (dx * dx + dy * dy > r2) continue;
      int x = cx + dx;
      if (x < CX - h || x >= CX + h) continue;
      uint16_t c = pressed ? 0xC618 : MS_WHITE;
      if (!enabled) c = 0x7BEF;
      fb[(size_t)y * LCD_WIDTH + x] = c;
    }
  }
  drawMusicControlIcons(cx, cy, i, false, playing, enabled ? (uint16_t)0x0000 : (uint16_t)0x4208);
}

// 外圈进度弧（极简仪表，12 点起顺时针）。
static void drawRimProgressArc(float t) {
  if (t < 0) t = 0; else if (t > 1) t = 1;
  musicEnsureChords();
  uint16_t *fb = gfx->getFramebuffer();
  const int R = LCD_WIDTH / 2 - 6;
  const int steps = 360;
  const int lit = (int)(steps * t);
  for (int i = 0; i < steps; i++) {
    // 从 -90°（12 点）起
    float ar = (-90.0f + i) * (float)PI / 180.0f;
    int x = CX + (int)lroundf(R * cosf(ar));
    int y = CX + (int)lroundf(R * sinf(ar));
    if ((unsigned)x >= (unsigned)LCD_WIDTH || (unsigned)y >= (unsigned)LCD_HEIGHT) continue;
    int h = chordHalf[y];
    if (!h || x < CX - h || x >= CX + h) continue;
    uint16_t c = (i <= lit) ? MS_WHITE : (uint16_t)0x4208;
    fb[(size_t)y * LCD_WIDTH + x] = c;
    if ((unsigned)(y + 1) < (unsigned)LCD_HEIGHT)
      fb[(size_t)(y + 1) * LCD_WIDTH + x] = blend565(fb[(size_t)(y + 1) * LCD_WIDTH + x], c, 120);
  }
}

static void drawMusicMetaCentered(int titleY, int artistY) {
  bool linked = bleConnected && bleAuthed;
  if (hidMusicMode && linked && !appleMedia.available) {
    printCentered("PHONE MEDIA KEYS", CX, titleY, 2, MUSIC_COL_TITLE);
    printCentered("OPEN MUSIC APP ON PHONE", CX, artistY, 2, MUSIC_COL_ARTIST);
  } else if (linked) {
    drawTextImgC(5, titleY, 0xE8, MUSIC_COL_TITLE);
    drawTextImgC(6, artistY, 0xC8, MUSIC_COL_ARTIST);
  } else {
    printCentered("PHONE OFFLINE", CX, (titleY + artistY) / 2, 3, MUSIC_COL_ARTIST);
  }
}

// ---- 主题 1：封面轮播 ----
// 左 = 上一首真图（coverPrev）或冷青偏色占位；中 = 当前封；右 = 暖洋红偏色/几何占位。
// 三张绝不能是同一 bitmap 原样拷贝。
static void drawMusicThemeCarousel() {
  drawMusicAmbient();
  const int half = 78, rad = 18;
  const int cy = 130;
  const int nHalf = 54, nRad = 14;
  // 左邻：优先上一首历史封面；否则当前封冷青偏色 + 偏裁；再否则几何占位
  if (coverPrevValid && coverPrevImg && coverPrevW > 0) {
    drawMusicRoundedSquareSrc(coverPrevImg, coverPrevW, coverPrevH,
                              CX - 150, cy, nHalf, nRad, 14, 0, -1);
  } else if (coverValid && coverImg) {
    const uint16_t *src = coverImg;
    drawMusicRoundedSquareSrc(src, coverW, coverH, CX - 150, cy, nHalf, nRad, 12, 1, -2);
  } else {
    drawMusicPlaceholderSquare(CX - 150, cy, nHalf, nRad, 0);
  }
  // 右邻：AMS 无下一首封面 → 暖色偏裁变体或占位（刻意与中/左不同）
  if (coverValid && coverImg) {
    drawMusicRoundedSquareSrc(coverImg, coverW, coverH, CX + 150, cy, nHalf, nRad, 12, 2, 2);
  } else {
    drawMusicPlaceholderSquare(CX + 150, cy, nHalf, nRad, 1);
  }
  // 中封：当前原色
  drawMusicRoundedSquare(CX, cy, half, rad, 32, 0);
  double dur = 0, el = musicElapsedNow(&dur);
  if (dur > 0) drawSquareProgress(CX, cy, half + 4, (float)(el / dur));
  // 中心橙播控
  {
    int pr = 28;
    bool enabled = hidMusicMode ? (hidMediaReady() || appleMedia.controlReady(1)) : bleConnected && musicHave;
    musicEnsureChords();
    uint16_t *fb = gfx->getFramebuffer();
    for (int dy = -pr; dy <= pr; dy++) {
      int y = cy + dy;
      if ((unsigned)y >= (unsigned)LCD_HEIGHT) continue;
      int h = chordHalf[y];
      if (!h) continue;
      for (int dx = -pr; dx <= pr; dx++) {
        if (dx * dx + dy * dy > pr * pr) continue;
        int x = CX + dx;
        if (x < CX - h || x >= CX + h) continue;
        fb[(size_t)y * LCD_WIDTH + x] = (enabled && musicPressed == 1) ? (uint16_t)0xD2E0 : MS_ORANGE;
      }
    }
    drawMusicControlIcons(CX, cy, 1, false, musicPlaying, MS_WHITE);
  }
  // 左右切歌三角（装饰 + 命中区）
  gfx->fillTriangle(28, cy - 10, 28, cy + 10, 14, cy, MS_WHITE);
  gfx->fillTriangle(LCD_WIDTH - 28, cy - 10, LCD_WIDTH - 28, cy + 10, LCD_WIDTH - 14, cy, MS_WHITE);
  if (musicLyricsReady()) drawTextImgC(12, 250, 0xD0, MUSIC_COL_TITLE);
  drawMusicMetaCentered(290, 324);
}

// ---- 主题 2：柔光立体 ----
// musicA.39：歌词仍在封面下方中部；歌名+作者挪到底栏 prev/next 中间空隙。
static void drawMusicThemeSoftNeon() {
  drawMusicAmbient();
  const int half = 100, rad = 22, cy = 128;
  drawMusicRoundedSquare(CX, cy, half, rad, 32, 0);
  bool enabled = hidMusicMode ? (hidMediaReady() || appleMedia.controlReady(1)) : bleConnected && musicHave;
  drawNeonPlayOrb(CX, cy, 46, musicPlaying, enabled && musicPressed == 1);
  // 歌词在封面下（保持偏上）
  if (musicLyricsReady()) drawTextImgC(12, 268, 0xD0, MUSIC_COL_TITLE);
  // 底栏：左 prev / 中 歌名+作者 / 右 next
  for (int i = 0; i < 2; i++) {
    int bi = i == 0 ? 0 : 2;
    int x = CX + (i == 0 ? -90 : 90);
    int y = 400;
    bool en = hidMusicMode ? (hidMediaReady() || appleMedia.controlReady(bi)) : bleConnected && musicHave;
    drawMusicGlassButton(x, y, 26, en, en && musicPressed == bi);
    drawMusicControlIcons(x, y, bi, false, musicPlaying, en ? MS_NEON : (uint16_t)0x7BEF);
  }
  drawMusicMetaCentered(348, 378);
}

// ---- 主题 3：歌词主导 ----
static void drawMusicThemeLyrics() {
  drawMusicAmbient();
  drawMusicSmallDisc(78, 78, 36);
  bool linked = bleConnected && bleAuthed;
  if (linked && !(hidMusicMode && !appleMedia.available)) {
    drawTextImgAt(5, 128, 56, 0xE8, MUSIC_COL_TITLE);
    drawTextImgAt(6, 128, 88, 0xC8, MUSIC_COL_ARTIST);
  } else if (!linked) {
    printCentered("PHONE OFFLINE", CX, 80, 2, MUSIC_COL_ARTIST);
  }
  if (musicLyricsReady()) drawTextImgC(12, 160, 0xF0, MUSIC_COL_TITLE);
  else {
    printCentered("NO LYRICS", CX, 220, 2, MUSIC_COL_TIME);
    printCentered("ENABLE IN APP", CX, 248, 2, MUSIC_COL_TIME);
  }
  double dur = 0, el = musicElapsedNow(&dur);
  if (dur > 0) drawMusicProgressBar(318, el, dur);
  for (int i = 0; i < 3; i++) {
    int x = CX + (i - 1) * MUSIC_BTN_GAP;
    int r = (i == 1) ? MUSIC_BTN_R_MAIN : MUSIC_BTN_R_SIDE;
    bool enabled = hidMusicMode ? (hidMediaReady() || appleMedia.controlReady(i)) : bleConnected && musicHave;
    drawMusicGlassButton(x, MUSIC_BTN_Y, r, enabled, enabled && musicPressed == i);
    drawMusicControlIcons(x, MUSIC_BTN_Y, i, false, musicPlaying, enabled ? MUSIC_COL_ICON : (uint16_t)0x7BEF);
  }
}

// ---- 主题 4：极简仪表 ----
static void drawMusicThemeGauge() {
  // ui.6：不再硬编码 0x1082 深底，与其余 4 种样式统一吃封面风格化背景。
  drawMusicAmbient();
  double dur = 0, el = musicElapsedNow(&dur);
  if (dur > 0) drawRimProgressArc((float)(el / dur));
  else drawRimProgressArc(0);
  drawMusicSmallDisc(CX, 70, 28);
  drawMusicMetaCentered(120, 168);
  if (musicLyricsReady()) drawTextImgC(12, 210, 0xD0, MUSIC_COL_TITLE);
  for (int i = 0; i < 3; i++) {
    int x = CX + (i - 1) * MUSIC_BTN_GAP;
    int r = (i == 1) ? 36 : 28;
    bool enabled = hidMusicMode ? (hidMediaReady() || appleMedia.controlReady(i)) : bleConnected && musicHave;
    drawWhiteTransport(x, MUSIC_BTN_Y, r, i, enabled, enabled && musicPressed == i, musicPlaying);
  }
}

// ---- 主题 5：全屏转动封面 · 方案 B（慢旋满圆 + 细进度 + 底部半透胶囊播控）----
static const int MS5_CAP_Y = 400;
static const int MS5_CAP_W = 220;
static const int MS5_CAP_H = 44;
static const int MS5_PROG_Y = 348;
static const int MS5_TITLE_Y = 300;
static const int MS5_ARTIST_Y = 328;
static const int MS5_FULL_R = 226;   // 接近屏缘，封面主导

static void drawMusicFullscreenCover(float angleRad) {
  if (!coverValid) return;
  const uint16_t *src = (coverHiReady && coverHi) ? coverHi : coverImg;
  const int sw = (coverHiReady && coverHi) ? COVER_HI : coverW;
  const int sh = (coverHiReady && coverHi) ? COVER_HI : coverH;
  if (!src || sw <= 0 || sh <= 0) return;
  musicEnsureChords();
  uint16_t *fb = gfx->getFramebuffer();
  const int R = MS5_FULL_R, R2 = R * R, Rrim2 = (R + 1) * (R + 1);
  const int ca = (int)lroundf(cosf(angleRad) * 65536.0f);
  const int sa = (int)lroundf(sinf(angleRad) * 65536.0f);
  const int scale = (sw << 16) / (2 * R);
  const int cx0 = sw << 15, cy0 = sh << 15;
  const int y0 = CX - R - 1, y1 = CX + R + 1;
  for (int y = y0; y <= y1; y++) {
    if ((unsigned)y >= (unsigned)LCD_HEIGHT) continue;
    int h = chordHalf[y];
    if (!h) continue;
    int dy = y - CX;
    int span2 = Rrim2 - dy * dy;
    if (span2 < 0) continue;
    int span = (int)sqrtf((float)span2);
    int xL = CX - span, xR = CX + span;
    if (xL < CX - h) xL = CX - h;
    if (xR > CX + h) xR = CX + h;
    if (xL < 0) xL = 0;
    if (xR > LCD_WIDTH) xR = LCD_WIDTH;
    uint16_t *row = fb + (size_t)y * LCD_WIDTH;
    const int dyS = dy * scale;
    for (int x = xL; x < xR; x++) {
      int dx = x - CX;
      int d2 = dx * dx + dy * dy;
      if (d2 > Rrim2) continue;
      if (d2 > R2) {
        row[x] = blend565(row[x], 0x8410, 60);
        continue;
      }
      const int dxS = dx * scale;
      int u = ((dxS >> 8) * (ca >> 8) + (dyS >> 8) * (sa >> 8)) + cx0;
      int v = (-(dxS >> 8) * (sa >> 8) + (dyS >> 8) * (ca >> 8)) + cy0;
      int ui = u >> 16, vi = v >> 16;
      if ((unsigned)ui < (unsigned)sw && (unsigned)vi < (unsigned)sh)
        row[x] = src[(size_t)vi * sw + ui];
    }
  }
}

static void drawMusicCapsuleTransport() {
  musicEnsureChords();
  uint16_t *fb = gfx->getFramebuffer();
  const int x0 = CX - MS5_CAP_W / 2, y0 = MS5_CAP_Y - MS5_CAP_H / 2;
  const int x1 = x0 + MS5_CAP_W, y1 = y0 + MS5_CAP_H;
  const int rad = MS5_CAP_H / 2;
  for (int y = y0; y < y1; y++) {
    if ((unsigned)y >= (unsigned)LCD_HEIGHT) continue;
    int h = chordHalf[y];
    if (!h) continue;
    int dy = y - MS5_CAP_Y;
    for (int x = x0; x < x1; x++) {
      if (x < CX - h || x >= CX + h) continue;
      // 圆角胶囊：两端半圆
      bool in = true;
      if (x < x0 + rad) {
        int dx = x - (x0 + rad);
        if (dx * dx + dy * dy > rad * rad) in = false;
      } else if (x >= x1 - rad) {
        int dx = x - (x1 - rad);
        if (dx * dx + dy * dy > rad * rad) in = false;
      }
      if (!in) continue;
      uint16_t &px = fb[(size_t)y * LCD_WIDTH + x];
      px = blend565(px, 0xDEFB, musicPressed >= 0 ? 110 : 90);
    }
  }
  // 三键图标落在胶囊内
  for (int i = 0; i < 3; i++) {
    int x = CX + (i - 1) * 64;
    bool enabled = hidMusicMode ? (hidMediaReady() || appleMedia.controlReady(i)) : bleConnected && musicHave;
    drawMusicControlIcons(x, MS5_CAP_Y, i, false, musicPlaying, enabled ? MUSIC_COL_ICON : (uint16_t)0x7BEF);
  }
}

static void drawMusicThemeFullscreenSpin() {
  // 封面即背景：先黑再满圆慢旋封面；底轻 scrim + 标题/进度/胶囊（静止 overlay）
  gfx->fillScreen(0x0000);
  if (coverValid && coverImg) drawMusicFullscreenCover(coverAngle);
  else drawMusicAmbient();
  drawMusicBottomScrim();
  drawMusicMetaCentered(MS5_TITLE_Y, MS5_ARTIST_Y);
  double dur = 0, el = musicElapsedNow(&dur);
  if (dur > 0) drawMusicProgressBar(MS5_PROG_Y, el, dur);
  drawMusicCapsuleTransport();
}

// ================= FO-218 / musicA.71：主题 6「黑胶唱片」 =================
// 参考 AudioPod 封面：红色底盘（下移）+ 同心沟槽黑胶 + 中心圆封（随 coverAngle 慢旋）
// + 唱臂（tonearmT 落槽 / 抬起）+ 红盘文字四层。全部逐像素程序化生成，
// 零资源、零新增 bulk；复用 coverImg / coverAngle / tonearmT / musicElapsedNow。
#define MS6_VINYL_CY 158
#define MS6_VINYL_R  152
#define MS6_LABEL_R  56
#define MS6_RED_CY   258
#define MS6_RED_R    178
#define MS6_TITLE_Y  340
#define MS6_ARTIST_Y 362
#define MS6_CAP_Y    385
#define MS6_FOOT_Y   409
#define MS6_PIVOT_X  392
#define MS6_PIVOT_Y  236

static inline uint16_t ms6RGB(int r, int g, int b) {
  if (r < 0) r = 0; else if (r > 255) r = 255;
  if (g < 0) g = 0; else if (g > 255) g = 255;
  if (b < 0) b = 0; else if (b > 255) b = 255;
  return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

// 背景 + 红盘 + 黑胶沟槽；圆屏外不写（chordHalf）。
static void ms6Base() {
  musicEnsureChords();
  uint16_t *fb = gfx->getFramebuffer();
  const int W = LCD_WIDTH, H = LCD_HEIGHT;
  for (int y = 0; y < H; y++) {
    int h = chordHalf[y];
    if (!h) continue;
    for (int x = CX - h; x < CX + h; x++) {
      int d2 = (x - CX) * (x - CX) + (y - CX) * (y - CX);
      int glow = d2 > 90000 ? 0 : (90000 - d2) * 8 / 90000;
      fb[(size_t)y * W + x] = ms6RGB(10 + glow, 10 + glow, 18 + glow);
    }
  }
  const int RR = MS6_RED_R, RR2 = RR * RR;
  for (int y = MS6_RED_CY - RR; y <= MS6_RED_CY + RR; y++) {
    if ((unsigned)y >= (unsigned)H) continue;
    int h = chordHalf[y];
    if (!h) continue;
    int dy = y - MS6_RED_CY;
    for (int x = CX - RR; x <= CX + RR; x++) {
      int dx = x - CX, d2 = dx * dx + dy * dy;
      if (d2 > RR2) continue;
      if (x < CX - h || x >= CX + h) continue;
      float rr = sqrtf((float)d2);
      float light = 1.0f - rr / RR;                                     // 中心亮、边缘暗
      if (light < 0) light = 0;
      float sheen = (-(float)dx * 0.35f - (float)dy * 0.9f) / RR + 0.52f;
      if (sheen < 0) sheen = 0; else if (sheen > 1.0f) sheen = 1.0f;
      int r = 120 + (int)(light * 76) + (int)(sheen * 22);
      int g = 26  + (int)(light * 34) + (int)(sheen * 12);
      int b = 23  + (int)(light * 28) + (int)(sheen * 10);
      float vig = (rr - RR * 0.72f) / (RR * 0.28f);                    // 边缘暗角
      if (vig < 0) vig = 0; else if (vig > 1.0f) vig = 1.0f;
      int vd = (int)(vig * 34);
      r -= vd; g -= vd; b -= vd;
      if (rr > RR - 3) { r = r * 72 / 100; g = g * 72 / 100; b = b * 72 / 100; }
      fb[(size_t)y * W + x] = ms6RGB(r, g, b);
    }
  }
  const int VR = MS6_VINYL_R, VR2 = VR * VR, LR2 = (MS6_LABEL_R + 2) * (MS6_LABEL_R + 2);
  for (int y = MS6_VINYL_CY - VR; y <= MS6_VINYL_CY + VR; y++) {
    if ((unsigned)y >= (unsigned)H) continue;
    int h = chordHalf[y];
    if (!h) continue;
    int dy = y - MS6_VINYL_CY;
    for (int x = CX - VR; x <= CX + VR; x++) {
      int dx = x - CX, d2 = dx * dx + dy * dy;
      if (d2 > VR2 || d2 < LR2) continue;
      if (x < CX - h || x >= CX + h) continue;
      float rr = sqrtf((float)d2);
      // cos(ang + 135°) 的点积等价式：左上最亮、右下最暗（与确认稿一致）
      float lit = (-(float)dx - (float)dy) * 0.7071f / rr * 0.5f + 0.5f;
      if (lit < 0) lit = 0; else if (lit > 1.0f) lit = 1.0f;
      int v = 13 + (int)(lit * 34);
      if (((int)rr % 4) == 0) v -= 7;                                    // 细沟槽
      if (((int)rr % 11) == 0) v -= 5;                                   // 粗沟槽
      float lift = (rr - 60) / 92.0f;
      if (lift < 0) lift = 0; else if (lift > 1.0f) lift = 1.0f;
      v += (int)(lift * 4);
      if (rr > VR - 2) v += 22;                                           // 外缘亮边
      fb[(size_t)y * W + x] = ms6RGB(v + 1, v + 1, v + 3);
    }
  }
}

// 中心圆封：反向旋转采样 coverImg；无封面时给占位盘。
static void ms6Label(float angle) {
  musicEnsureChords();
  uint16_t *fb = gfx->getFramebuffer();
  const int R = MS6_LABEL_R, R2 = R * R;
  if (!coverValid || !coverImg || coverW <= 0 || coverH <= 0) {
    gfx->fillCircle(CX, MS6_VINYL_CY, R, ms6RGB(30, 32, 44));
    printCentered("NO COVER", CX, MS6_VINYL_CY, 2, MS_WHITE);
  } else {
    float ca = cosf(angle), sa = sinf(angle);
    const int sw = coverW, sh = coverH;
    for (int y = MS6_VINYL_CY - R; y <= MS6_VINYL_CY + R; y++) {
      if ((unsigned)y >= (unsigned)LCD_HEIGHT) continue;
      int h = chordHalf[y];
      if (!h) continue;
      int dy = y - MS6_VINYL_CY;
      for (int x = CX - R; x <= CX + R; x++) {
        int dx = x - CX;
        if (dx * dx + dy * dy > R2) continue;
        if (x < CX - h || x >= CX + h) continue;
        int rx = (int)(dx * ca + dy * sa), ry = (int)(-dx * sa + dy * ca);
        int ui = (rx + R) * sw / (2 * R), vi = (ry + R) * sh / (2 * R);
        if ((unsigned)ui >= (unsigned)sw || (unsigned)vi >= (unsigned)sh) continue;
        fb[(size_t)y * LCD_WIDTH + x] = coverImg[(size_t)vi * sw + ui];
      }
    }
  }
  gfx->drawCircle(CX, MS6_VINYL_CY, R + 1, ms6RGB(226, 224, 216));
  gfx->drawCircle(CX, MS6_VINYL_CY, R,     ms6RGB(226, 224, 216));
  gfx->fillCircle(CX, MS6_VINYL_CY, 3, ms6RGB(8, 8, 12));
  gfx->fillCircle(CX, MS6_VINYL_CY, 1, ms6RGB(180, 178, 172));
}

static void ms6Thick(int x0, int y0, int x1, int y1, int w, uint16_t c) {
  int dx = x1 - x0, dy = y1 - y0;
  int n = (musicAbs(dx) > musicAbs(dy) ? musicAbs(dx) : musicAbs(dy)) + 1;
  int r = w / 2;
  uint16_t *fb = gfx->getFramebuffer();
  for (int i = 0; i <= n; i++) {
    int x = x0 + dx * i / n, y = y0 + dy * i / n;
    for (int oy = -r; oy <= r; oy++) for (int ox = -r; ox <= r; ox++) {
      if (ox * ox + oy * oy > r * r) continue;
      int X = x + ox, Y = y + oy;
      if ((unsigned)X < (unsigned)LCD_WIDTH && (unsigned)Y < (unsigned)LCD_HEIGHT)
        fb[(size_t)Y * LCD_WIDTH + X] = c;
    }
  }
}

// 唱臂：tonearmT 0=暂停抬离（针尖在盘外、杆更陡）/ 1=播放落进外圈沟槽
static void ms6Arm() {
  float tt = tonearmT;
  if (tt < 0) tt = 0; else if (tt > 1) tt = 1;
  int tipX = (int)(372 + (330 - 372) * tt);
  int tipY = (int)(282 + (268 - 282) * tt);
  int px = MS6_PIVOT_X - 4, py = MS6_PIVOT_Y + 4;
  ms6Thick(px, py, tipX, tipY, 9, ms6RGB(8, 10, 16));
  ms6Thick(px, py, tipX, tipY, 5, ms6RGB(178, 190, 204));
  ms6Thick(px - 2, py - 2, tipX - 2, tipY - 2, 2, ms6RGB(236, 242, 250));
  gfx->fillCircle(tipX, tipY, 4, ms6RGB(238, 190, 74));
  gfx->fillCircle(tipX, tipY, 2, ms6RGB(255, 232, 160));
  gfx->fillCircle(MS6_PIVOT_X, MS6_PIVOT_Y, 12, ms6RGB(12, 14, 22));
  gfx->fillCircle(MS6_PIVOT_X, MS6_PIVOT_Y, 10, ms6RGB(56, 68, 86));
  gfx->fillCircle(MS6_PIVOT_X, MS6_PIVOT_Y, 6,  ms6RGB(108, 128, 152));
  gfx->fillCircle(MS6_PIVOT_X, MS6_PIVOT_Y - 1, 2, ms6RGB(196, 212, 230));
}

// 红盘文字层：歌名 / 歌手 / 深色时间胶囊 / 状态小字
static void ms6Text() {
  drawMusicMetaCentered(MS6_TITLE_Y, MS6_ARTIST_Y);
  gfx->fillRoundRect(CX - 38, MS6_CAP_Y - 13, 76, 26, 13, ms6RGB(58, 16, 16));
  gfx->drawRoundRect(CX - 38, MS6_CAP_Y - 13, 76, 26, 13, ms6RGB(150, 58, 48));
  double dur = 0, el = musicElapsedNow(&dur);
  char buf[16];
  if (dur > 0) snprintf(buf, sizeof(buf), "%d:%02d", (int)el / 60, (int)el % 60);
  else snprintf(buf, sizeof(buf), "--:--");
  printCentered(buf, CX, MS6_CAP_Y - 1, 2, MS_WHITE);
  char foot[32];
  if (dur > 0) snprintf(foot, sizeof(foot), "%d:%02d  MOTONAV AUDIO", (int)dur / 60, (int)dur % 60);
  else snprintf(foot, sizeof(foot), "MOTONAV AUDIO");
  printCentered(foot, CX, MS6_FOOT_Y, 1, ms6RGB(206, 142, 132));
}

static void drawMusicThemeVinyl() {
  ms6Base();
  ms6Label(coverAngle);
  ms6Arm();
  ms6Text();
}

// 按样式命中 prev/play/next（0/1/2）；未命中 -1。
static int musicButtonAtStyle(int x, int y) {
  // 黑胶：点中心封面播放/暂停，左右红耳与文字带切歌（无可见按键，保持参考图纯净）
  if (musicStyle == 6) {
    int dx = x - CX, dy = y - MS6_VINYL_CY;
    if (dx * dx + dy * dy <= 62 * 62) return 1;
    if (y > 180 && y < MS6_FOOT_Y + 18) {
      if (x < CX - 62) return 0;
      if (x > CX + 62) return 2;
      return 1;
    }
    return -1;
  }
  // 触摸坐标已在 musicButtonAt 里校正到画布系
  if (musicStyle == 5) {
    const int x0 = CX - MS5_CAP_W / 2, y0 = MS5_CAP_Y - MS5_CAP_H / 2;
    if (x >= x0 && x < x0 + MS5_CAP_W && y >= y0 && y < y0 + MS5_CAP_H) {
      int slot = (x - x0) * 3 / MS5_CAP_W;
      if (slot < 0) slot = 0;
      if (slot > 2) slot = 2;
      return slot;
    }
    return -1;
  }
  if (musicStyle == 1) {
    // 封面轮播：中心播控 + 左右三角
    int cy = 130, pr = 40;
    if ((x - CX) * (x - CX) + (y - cy) * (y - cy) <= pr * pr) return 1;
    if (x < 70 && musicAbs(y - cy) < 40) return 0;
    if (x > LCD_WIDTH - 70 && musicAbs(y - cy) < 40) return 2;
    return -1;
  }
  if (musicStyle == 2) {
    int cy = 128;
    if ((x - CX) * (x - CX) + (y - cy) * (y - cy) <= 54 * 54) return 1;
    if ((x - (CX - 90)) * (x - (CX - 90)) + (y - 400) * (y - 400) <= 40 * 40) return 0;
    if ((x - (CX + 90)) * (x - (CX + 90)) + (y - 400) * (y - 400) <= 40 * 40) return 2;
    return -1;
  }
  // 0 / 3 / 4：底栏三键
  for (int i = 0; i < 3; i++) {
    int bx = CX + (i - 1) * MUSIC_BTN_GAP;
    int br = (i == 1) ? MUSIC_BTN_R_MAIN + 12 : MUSIC_BTN_R_SIDE + 12;
    if (musicStyle == 4 && i == 1) br = 48;
    int by = MUSIC_BTN_Y;
    if ((x - bx) * (x - bx) + (y - by) * (y - by) <= br * br) return i;
  }
  return -1;
}
