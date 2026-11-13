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
// FO-219：drawTextImgC 的 y0 是「顶部」，App 位图高约 39(歌名)/29(歌手)。
// 旧值 340/362/385/409 三层互相压叠；这里按最坏 s=1 高度重排，层间留 3~6px。
#define MS6_TITLE_Y  316
#define MS6_ARTIST_Y 358
#define MS6_CAP_Y    405
#define MS6_FOOT_Y   424
#define MS6_PIVOT_X  392
#define MS6_PIVOT_Y  236

// FO-219：旋帧只重绘变化区域，静态层存 PSRAM 局部快照（合计约 126 KiB）。
// 三块矩形互不重叠：label y99..217 / arm y218..298 / text y312..436。
#define MS6_LB_X (CX - MS6_LABEL_R - 3)
#define MS6_LB_Y (MS6_VINYL_CY - MS6_LABEL_R - 3)
#define MS6_LB_W (2 * MS6_LABEL_R + 7)
#define MS6_LB_H (2 * MS6_LABEL_R + 7)
#define MS6_AR_X 316
#define MS6_AR_Y 218
#define MS6_AR_W 96
#define MS6_AR_H 80
#define MS6_TX_X 50
#define MS6_TX_Y 312
#define MS6_TX_W 366
#define MS6_TX_H 124
static uint16_t *ms6LabelBg = nullptr;
static uint16_t *ms6ArmBg   = nullptr;
static uint16_t *ms6TextBg  = nullptr;
static bool ms6ArmDirty  = false;
static bool ms6TextDirty = false;

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
  printCentered(buf, CX, MS6_CAP_Y - 8, 2, MS_WHITE);   // size2 字高 16，居中于胶囊
  char foot[32];
  if (dur > 0) snprintf(foot, sizeof(foot), "%d:%02d  MOTONAV AUDIO", (int)dur / 60, (int)dur % 60);
  else snprintf(foot, sizeof(foot), "MOTONAV AUDIO");
  printCentered(foot, CX, MS6_FOOT_Y, 1, ms6RGB(206, 142, 132));
}

// FO-219：静态层局部快照 / 恢复。三块矩形互不重叠，恢复一块不会影响另两块。
static void ms6RectSave(uint16_t *dst, int X, int Y, int W, int H) {
  uint16_t *fb = gfx->getFramebuffer();
  if (!dst || !fb) return;
  for (int y = 0; y < H; y++) {
    int sy = Y + y;
    if ((unsigned)sy >= (unsigned)LCD_HEIGHT) break;
    memcpy(dst + (size_t)y * W, fb + (size_t)sy * LCD_WIDTH + X, (size_t)W * 2);
  }
}
static void ms6RectLoad(const uint16_t *src, int X, int Y, int W, int H) {
  uint16_t *fb = gfx->getFramebuffer();
  if (!src || !fb) return;
  for (int y = 0; y < H; y++) {
    int sy = Y + y;
    if ((unsigned)sy >= (unsigned)LCD_HEIGHT) break;
    memcpy(fb + (size_t)sy * LCD_WIDTH + X, src + (size_t)y * W, (size_t)W * 2);
  }
}
static void ms6CaptureStatic() {
  if (!gfx->getFramebuffer()) return;
  if (!ms6LabelBg) ms6LabelBg = (uint16_t *)ps_malloc(MS6_LB_W * MS6_LB_H * 2);
  if (!ms6ArmBg)   ms6ArmBg   = (uint16_t *)ps_malloc(MS6_AR_W * MS6_AR_H * 2);
  if (!ms6TextBg)  ms6TextBg  = (uint16_t *)ps_malloc(MS6_TX_W * MS6_TX_H * 2);
  if (ms6LabelBg) ms6RectSave(ms6LabelBg, MS6_LB_X, MS6_LB_Y, MS6_LB_W, MS6_LB_H);
  if (ms6ArmBg)   ms6RectSave(ms6ArmBg,   MS6_AR_X, MS6_AR_Y, MS6_AR_W, MS6_AR_H);
  if (ms6TextBg)  ms6RectSave(ms6TextBg,  MS6_TX_X, MS6_TX_Y, MS6_TX_W, MS6_TX_H);
}

static void drawMusicThemeVinyl() {
  ms6Base();
  ms6CaptureStatic();      // 必须在 label/arm/text 之前：存的是纯静态底
  ms6Label(coverAngle);
  ms6Arm();
  ms6Text();
  ms6ArmDirty = false;
  ms6TextDirty = false;
}

// FO-219 旋帧：中心封每帧重画（约 1.4 万像素），唱臂与文字带只在变化时重画。
// 不再每帧跑 ms6Base() 的两遍 sqrtf 全屏逐像素，帧率从 ~10fps 提到 24fps。
static void drawMusicThemeVinylSpin() {
  if (!ms6LabelBg || !ms6ArmBg || !ms6TextBg || !gfx->getFramebuffer()) {
    drawMusicThemeVinyl();                 // PSRAM 分配失败时退回全量
    return;
  }
  ms6RectLoad(ms6LabelBg, MS6_LB_X, MS6_LB_Y, MS6_LB_W, MS6_LB_H);
  ms6Label(coverAngle);
  if (ms6ArmDirty) {
    ms6RectLoad(ms6ArmBg, MS6_AR_X, MS6_AR_Y, MS6_AR_W, MS6_AR_H);
    ms6Arm();
    ms6ArmDirty = false;
  }
  if (ms6TextDirty) {
    ms6RectLoad(ms6TextBg, MS6_TX_X, MS6_TX_Y, MS6_TX_W, MS6_TX_H);
    ms6Text();
    ms6TextDirty = false;
  }
}

// ================= FO-225b / musicA.75：主题 7「封面流 Cover Flow」 =================
// 视觉参数逐像素沿用已确认的 motonav-coverflow-b.png / motonav-gesture-strip.png：
// 中央 164² @ (233,170)、右一 54×144 @339、左一 36×140 @131、倒影 34px。
// 3 张真封由 App 经 bulk type 12 预下发，7 个卡位循环复用（远→近绘制）。
// 逐列 16.16 定点梯形采样：每列只算一次 u / 顶底边 / v 步长，像素内部全是乘加，
// 无 GPU、无纹理、无逐像素除法；只写 318×220 卡带，静止零重绘。
#define CF_CY 170
struct CFGeom { int xL, xR; int32_t tl16, tr16, bl16, br16; float dim; };

// pos → (w, h, cx, tl, tr, bl, br, dim)；tl/tr/bl/br 是相对 cy±h/2 的收敛偏移，
// 左右边竖直 + 上下边向远侧收敛 = rotateY 的一阶近似。
static const float CF_KEYS[7][9] = {
  {-2.0f,  30, 126,  95,  20,  10, -20, -10, 0.40f},
  {-1.0f,  36, 140, 131,  14,   5, -14,  -5, 0.52f},
  { 0.0f, 164, 164, 233,   0,   0,   0,   0, 1.00f},
  { 1.0f,  54, 144, 339,   6,  14,  -6, -14, 0.68f},
  { 2.0f,  54, 130, 373,  13,  22, -13, -22, 0.57f},
  { 3.0f,  54, 116, 407,  20,  30, -20, -30, 0.46f},
  { 4.0f,  48, 102, 441,  27,  38, -27, -38, 0.35f},
};

static void cfGeom(float pos, CFGeom &g) {
  if (pos < -2.0f) pos = -2.0f;
  if (pos >  4.0f) pos =  4.0f;
  int k = 0;
  while (k < 5 && CF_KEYS[k + 1][0] < pos) k++;
  float t = (pos - CF_KEYS[k][0]) / (CF_KEYS[k + 1][0] - CF_KEYS[k][0]);
  float w   = CF_KEYS[k][1] + (CF_KEYS[k+1][1] - CF_KEYS[k][1]) * t;
  float h   = CF_KEYS[k][2] + (CF_KEYS[k+1][2] - CF_KEYS[k][2]) * t;
  float cx  = CF_KEYS[k][3] + (CF_KEYS[k+1][3] - CF_KEYS[k][3]) * t;
  float tl  = CF_KEYS[k][4] + (CF_KEYS[k+1][4] - CF_KEYS[k][4]) * t;
  float tr  = CF_KEYS[k][5] + (CF_KEYS[k+1][5] - CF_KEYS[k][5]) * t;
  float bl  = CF_KEYS[k][6] + (CF_KEYS[k+1][6] - CF_KEYS[k][6]) * t;
  float br  = CF_KEYS[k][7] + (CF_KEYS[k+1][7] - CF_KEYS[k][7]) * t;
  g.dim = CF_KEYS[k][8] + (CF_KEYS[k+1][8] - CF_KEYS[k][8]) * t;
  g.xL = (int)(cx - w / 2); g.xR = (int)(cx + w / 2);
  g.tl16 = (int32_t)((CF_CY - h / 2 + tl) * 65536.0f);
  g.tr16 = (int32_t)((CF_CY - h / 2 + tr) * 65536.0f);
  g.bl16 = (int32_t)((CF_CY + h / 2 + bl) * 65536.0f);
  g.br16 = (int32_t)((CF_CY + h / 2 + br) * 65536.0f);
}

// 老 App（无 type 12）兜底：3 张不同色调的渐变占位，仍是 3 张不同卡，不崩不闪。
static const uint16_t *cfPlaceholder(int slot) {
  static uint16_t *buf[CFQ_SLOTS] = {nullptr, nullptr, nullptr};
  if (!buf[slot]) {
    buf[slot] = (uint16_t *)ps_malloc(CFQ_RAW);
    if (!buf[slot]) return nullptr;
    const uint16_t pal[CFQ_SLOTS] = {0x2A4A, 0xA145, 0x4B2D};
    for (int y = 0; y < CFQ_PX; y++) for (int x = 0; x < CFQ_PX; x++) {
      int k = 18 + (((x + y) >> 3) & 3) * 5;
      buf[slot][y * CFQ_PX + x] = musicDim565(pal[slot], (uint32_t)k);
    }
  }
  return buf[slot];
}

// FO-225c：type 12 未到位时别让 7 个卡位全 fallback 到同一张 coverImg
// （实机表现「所有封面都一样」）。按 slot 从当前封派生三张不同裁切 + 冷暖色调的变体。
static uint16_t *cfFallback[CFQ_SLOTS] = {nullptr, nullptr, nullptr};
static unsigned long cfFallbackAt = 0;
static void cfBuildFallback() {
  if (!coverValid || !coverImg || coverW <= 0 || coverH <= 0) return;
  for (int s = 0; s < CFQ_SLOTS; s++) {
    if (!cfFallback[s]) cfFallback[s] = (uint16_t *)ps_malloc(CFQ_RAW);
    if (!cfFallback[s]) return;                 // 分配不全就不更新时间戳，下次再试
  }
  for (int s = 0; s < CFQ_SLOTS; s++) {
    const float u0 = (s == 0) ? 0.02f : (s == 2) ? 0.46f : 0.0f;
    const float uw = (s == 1) ? 1.0f : 0.52f;
    for (int y = 0; y < CFQ_PX; y++) for (int x = 0; x < CFQ_PX; x++) {
      int sx = (int)((u0 + ((float)x + 0.5f) / CFQ_PX * uw) * coverW);
      int sy = (int)(((float)y + 0.5f) / CFQ_PX * coverH);
      if (sx >= coverW) sx = coverW - 1;
      if (sy >= coverH) sy = coverH - 1;
      uint16_t c = coverImg[(size_t)sy * coverW + sx];
      int r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
      if (s == 0) {                              // 冷青
        r = r * 10 / 32; g = min(63, g * 24 / 32 + 8); b = min(31, b * 20 / 32 + 4);
      } else if (s == 2) {                       // 暖紫
        r = min(31, r * 26 / 32 + 5); g = g * 16 / 32; b = min(31, b * 22 / 32 + 6);
      }
      cfFallback[s][y * CFQ_PX + x] = (uint16_t)((r << 11) | (g << 5) | b);
    }
  }
  cfFallbackAt = coverSpinAt;
}

static const uint16_t *cfSrcFor(int slot, int &sw, int &sh) {
  if (cfSlotOk[slot] && cfCovers[slot]) { sw = CFQ_PX; sh = CFQ_PX; return cfCovers[slot]; }
  if (coverValid && coverImg && coverW > 0 && coverH > 0) {
    if (cfFallbackAt != coverSpinAt || !cfFallback[slot]) cfBuildFallback();
    if (cfFallback[slot]) { sw = CFQ_PX; sh = CFQ_PX; return cfFallback[slot]; }
  }
  sw = CFQ_PX; sh = CFQ_PX;
  return cfPlaceholder(slot);
}

// 一张透视卡：按列扫描，u 决定该列顶/底边，v 逐行累加；圆角与圆屏都在这里裁掉。
static void cfCard(const uint16_t *src, int sw, int sh, const CFGeom &g) {
  if (!src || sw <= 0 || sh <= 0 || g.xR <= g.xL) return;
  uint16_t *fb = gfx->getFramebuffer();
  if (!fb) return;
  musicEnsureChords();
  const int xa = max(CF_BAND_X, g.xL), xb = min(CF_BAND_X + CF_BAND_W, g.xR);
  const float invW = 1.0f / (float)(g.xR - g.xL);
  const int32_t dtr = g.tr16 - g.tl16, dbr = g.br16 - g.bl16;
  const int32_t RAD = (int32_t)(0.045f * 65536.0f);
  for (int x = xa; x < xb; x++) {
    float uF = (x - g.xL + 0.5f) * invW;
    if (uF < 0.0f) uF = 0.0f; else if (uF > 1.0f) uF = 1.0f;
    const int32_t u16 = (int32_t)(uF * 65536.0f);
    const int32_t yt16 = g.tl16 + (int32_t)(((int64_t)dtr * u16) >> 16);
    const int32_t yb16 = g.bl16 + (int32_t)(((int64_t)dbr * u16) >> 16);
    const int32_t h16 = yb16 - yt16;
    if (h16 < 65536) continue;
    int y0 = max(CF_BAND_Y, (int)((yt16 + 65535) >> 16));
    int y1 = min(CF_BAND_Y + CF_BAND_H - 1, (int)(yb16 >> 16));
    if (y1 < y0) continue;
    int32_t v16 = (int32_t)(((int64_t)((int64_t)y0 * 65536 - yt16) << 16) / h16);
    const int32_t dv = (int32_t)(((int64_t)65536 << 16) / h16);
    int ui = (int)(((int64_t)u16 * sw) >> 16); if (ui >= sw) ui = sw - 1;
    const int32_t au = min(u16, 65536 - u16);
    float k = g.dim * (uF < 0.12f ? 1.12f : (0.88f + 0.12f * (1.0f - uF)));
    int dimK = (int)(k * 32.0f);
    if (dimK > 32) dimK = 32; else if (dimK < 4) dimK = 4;
    const bool leftEdge = (uF < 0.025f);
    for (int y = y0; y <= y1; y++, v16 += dv) {
      int hh = chordHalf[y];
      if (!hh || x < CX - hh || x >= CX + hh) continue;
      int32_t v = v16;
      if (v < 0) v = 0; else if (v > 65536) v = 65536;
      const int32_t av = min(v, 65536 - v);
      if (au < RAD && av < RAD) {
        int64_t dx = RAD - au, dy = RAD - av;
        if (dx * dx + dy * dy > (int64_t)RAD * RAD) continue;
      }
      int vi = (int)(((int64_t)v * sh) >> 16); if (vi >= sh) vi = sh - 1;
      uint16_t c = musicDim565(src[(size_t)vi * sw + ui], (uint32_t)dimK);
      if (leftEdge) c = blend565(c, 0xCE79, 140);
      fb[(size_t)y * LCD_WIDTH + x] = c;
    }
  }
}

// 倒影：取卡底上方若干行，按 j 二次衰减混到卡底下方（无模糊核，零额外缓冲）。
static void cfMirror(const CFGeom &g, int mh, int strength) {
  uint16_t *fb = gfx->getFramebuffer();
  if (!fb || mh <= 0) return;
  const int y1 = (int)(min(g.bl16, g.br16) >> 16);
  const int xa = max(CF_BAND_X, g.xL), xb = min(CF_BAND_X + CF_BAND_W, g.xR);
  for (int j = 0; j < mh; j++) {
    const int sy = y1 - 1 - j, dy = y1 + j;
    if (sy < CF_BAND_Y || sy >= CF_BAND_Y + CF_BAND_H) continue;
    if (dy < CF_BAND_Y || dy >= CF_BAND_Y + CF_BAND_H) continue;
    const int hh = chordHalf[dy];
    if (!hh) continue;
    int fade = mh - j;
    const int a = strength * fade * fade / (mh * mh);
    if (a <= 0) continue;
    const int x0 = max(xa, CX - hh), x1 = min(xb, CX + hh);
    for (int x = x0; x < x1; x++) {
      uint16_t &d = fb[(size_t)dy * LCD_WIDTH + x];
      uint16_t s = musicDim565(fb[(size_t)sy * LCD_WIDTH + x], 20);
      d = blend565(d, s, a);
    }
  }
}

// 中央卡 1px 亮边（效果图 B 的干净描边），只写卡带内圆内像素。
static void cfOutline(const CFGeom &g) {
  uint16_t *fb = gfx->getFramebuffer();
  if (!fb) return;
  const int xa = max(CF_BAND_X, g.xL), xb = min(CF_BAND_X + CF_BAND_W, g.xR) - 1;
  const int yt = (int)(min(g.tl16, g.tr16) >> 16), yb = (int)(max(g.bl16, g.br16) >> 16);
  const uint16_t c = 0xDEFB;
  auto put = [&](int x, int y) {
    if (y < CF_BAND_Y || y >= CF_BAND_Y + CF_BAND_H) return;
    int hh = chordHalf[y];
    if (!hh || x < CX - hh || x >= CX + hh) return;
    fb[(size_t)y * LCD_WIDTH + x] = c;
  };
  for (int x = xa; x <= xb; x++) { put(x, yt); put(x, yb); }
  for (int y = yt; y <= yb; y++) { put(xa, y); put(xb, y); }
}

// 远→近铺 7 个卡位；每张都按 phase 重算几何，不是位图平移。
static void cfDrawDeck(float phase) {
  musicEnsureChords();
  for (int idx = 4; idx >= -2; idx--) {
    const float pos = (float)idx - phase;
    if (pos < -2.0f || pos > 4.0f) continue;
    CFGeom g; cfGeom(pos, g);
    // FO-225e：App 协议 slot 0=上一首 / 1=当前 / 2=下一首候选。静止（phase=0、shift=0）
    // 时中央卡 idx=0 必须落在 slot 1；旧写法 idx=0→slot 0 会让中央显示「上一首」、
    // 而背景用真·当前封，实机看成「背景是下一张」。+1 偏移后左右卡也回到正确语义。
    int slot = ((idx + 1 + cfShift) % CFQ_SLOTS + CFQ_SLOTS) % CFQ_SLOTS;
    int sw = 0, sh = 0;
    const uint16_t *src = cfSrcFor(slot, sw, sh);
    if (!src) continue;
    cfCard(src, sw, sh, g);
    float a = fabsf(pos); if (a > 1.0f) a = 1.0f;
    int mh = (int)(34.0f - 18.0f * a);
    if (mh < 6) mh = 6;
    cfMirror(g, mh, a < 0.05f ? 82 : 38);
    if (a < 0.52f) cfOutline(g);
  }
}

// FO-225e：底栏播控重做。原来左右键只是 12×18 的裸三角 + 3px 竖条，既小又没边界；
// 现在用一枚淡玻璃胶囊把三键收成一组，左右放大为 r19 磨砂圆钮（亮灰外环 + 双三角
// 图标），主播放键 r22 加白环 / 内环 / 顶部高光弧，按下时对应键提亮。
static void cfCtrlPill(int by) {
  uint16_t *fb = gfx->getFramebuffer();
  if (!fb) return;
  musicEnsureChords();
  const int x0 = CX - 112, x1 = CX + 112;
  const int y0 = by - 27, y1 = by + 27, rr = 27;
  for (int y = y0; y <= y1; y++) {
    if ((unsigned)y >= (unsigned)LCD_HEIGHT) continue;
    const int h = chordHalf[y];
    if (!h) continue;
    const int xa = max(x0, CX - h), xb = min(x1, CX + h);
    for (int x = xa; x < xb; x++) {
      int dx = 0, dy = 0;
      if (x < x0 + rr) dx = x0 + rr - x; else if (x > x1 - rr) dx = x - (x1 - rr);
      if (y < y0 + rr) dy = y0 + rr - y; else if (y > y1 - rr) dy = y - (y1 - rr);
      if (dx * dx + dy * dy > rr * rr) continue;      // 胶囊圆角
      uint16_t *px = &fb[(size_t)y * LCD_WIDTH + x];
      if (y == y0 + 1 && x > x0 + rr && x < x1 - rr)
        *px = blend565(*px, 0xBDF8, 70);              // 顶沿一道细高光
      else
        *px = blend565(*px, 0x0000, 52);              // 极淡深色玻璃，不盖住封面底
    }
  }
}

// 现代播放器式切歌图标：竖条 + 两枚三角，比原来单三角更饱满、r19 圆钮里不显空。
static void cfSkipIcon(int cx, int cy, int sign, uint16_t c) {
  for (int k = 0; k < 2; k++) {
    const int ox = sign * (k * 9 - 8);
    gfx->fillTriangle(cx + ox - sign * 5, cy - 8,
                      cx + ox - sign * 5, cy + 8,
                      cx + ox + sign * 5, cy, c);
  }
  gfx->fillRect(sign > 0 ? cx + 9 : cx - 12, cy - 8, 3, 16, c);
}

static void cfCtrlButtons(int by, uint16_t *fb) {
  const int xs[2] = { CX - 64, CX + 64 };
  for (int i = 0; i < 2; i++) {
    const int idx = i == 0 ? 0 : 2;
    const bool pressed = musicPressed == idx;
    drawMusicGlassButton(xs[i], by, 19, true, pressed);
    gfx->drawCircle(xs[i], by, 20, pressed ? 0xFFFF : 0xBDF8);
    cfSkipIcon(xs[i], by, i == 0 ? -1 : 1, 0x0000);
  }
  const bool mainPressed = musicPressed == 1;
  gfx->fillCircle(CX, by, 22, mainPressed ? 0xFFFF : 0xEF7D);
  gfx->drawCircle(CX, by, 23, 0xFFFF);
  gfx->drawCircle(CX, by, 21, 0xBDF8);
  for (int ang = 205; ang <= 335; ang += 4) {        // 上半圈柔和高光
    const float ar = ang * (float)PI / 180.0f;
    const int hx = CX + (int)lroundf(20.0f * cosf(ar));
    const int hy = by + (int)lroundf(20.0f * sinf(ar));
    if ((unsigned)hx >= (unsigned)LCD_WIDTH || (unsigned)hy >= (unsigned)LCD_HEIGHT) continue;
    uint16_t *px = &fb[(size_t)hy * LCD_WIDTH + hx];
    *px = blend565(*px, 0xFFFF, 110);
  }
  drawMusicControlIcons(CX, by, 1, false, musicPlaying, 0x0000);
}

// 卡带外的静态 chrome：标题 / 歌名 / 作者 / 进度 / 播控，只在整页重绘时画一次。
static void cfChrome() {
  printCentered("N O W   P L A Y I N G", CX, 58, 1, 0x73AE);
  drawMusicMetaCentered(294, 340);
  double dur = 0, el = musicElapsedNow(&dur);
  const int x0 = 104, barW = 258, py = 380;
  gfx->fillRect(x0, py, barW, 3, 0x528A);
  int fill = 0;
  if (dur > 0) {
    fill = (int)(barW * constrain((float)(el / dur), 0.0f, 1.0f));
    if (fill > 0) gfx->fillRect(x0, py, fill, 3, 0xEF7D);
  }
  gfx->fillCircle(x0 + fill, py + 1, 4, 0xFFFF);
  char lb[8], rb[8];
  int sec = (int)el, total = (int)dur;
  snprintf(lb, sizeof(lb), "%d:%02d", sec / 60, sec % 60);
  snprintf(rb, sizeof(rb), "%d:%02d", total / 60, total % 60);
  gfx->setTextSize(1);
  gfx->setTextColor(0x73AE);
  gfx->setCursor(x0 - 8 - (int)strlen(lb) * 6, py - 4);
  gfx->print(lb);
  gfx->setCursor(x0 + barW + 8, py - 4);
  gfx->print(rb);
  const int by = 414;
  cfCtrlPill(by);
  cfCtrlButtons(by, gfx->getFramebuffer());
}

// 卡带快照：整页重绘后保存「没有卡」的干净背景，拖动帧反复恢复它再画卡组。
static void cfCaptureBand() {
  uint16_t *fb = gfx->getFramebuffer();
  if (!fb) return;
  if (!cfBandBg) cfBandBg = (uint16_t *)ps_malloc((size_t)CF_BAND_W * CF_BAND_H * 2);
  if (!cfBandBg) return;
  for (int y = 0; y < CF_BAND_H; y++)
    memcpy(cfBandBg + (size_t)y * CF_BAND_W,
           fb + (size_t)(CF_BAND_Y + y) * LCD_WIDTH + CF_BAND_X,
           (size_t)CF_BAND_W * 2);
}
static void cfRestoreBand() {
  uint16_t *fb = gfx->getFramebuffer();
  if (!fb || !cfBandBg) return;
  for (int y = 0; y < CF_BAND_H; y++)
    memcpy(fb + (size_t)(CF_BAND_Y + y) * LCD_WIDTH + CF_BAND_X,
           cfBandBg + (size_t)y * CF_BAND_W,
           (size_t)CF_BAND_W * 2);
}

// 只推 318×220 局部脏矩形；旋转状态下 fastPushRect 退化成整屏 flushSync（已知坑）。
static void cfDrawFrame() {
  if (!cfBandBg) return;
  cfRestoreBand();
  cfDrawDeck(cfPhase);
  flushRect(CF_BAND_X, CF_BAND_Y, CF_BAND_W, CF_BAND_H);
}

static void drawMusicThemeCoverFlow() {
  drawMusicAmbient();
  cfChrome();
  cfCaptureBand();                 // 必须在画卡组之前：存的是纯背景
  cfDrawDeck(cfPhase);             // 中途被 musicDirty 打断也保持当前相位，不闪回
  needFlush = true;
  musicChromeReady = false;
}

// 切歌命令：与 musicTap 的 HID / AMS / legacy 三条路径保持一致。
static void cfSendButton(int button) {
  if (hidMusicMode) {
    bool hidReady = hidMediaReady();
    if (!hidReady && !appleMedia.controlReady(button)) {
      musicAck = 7; musicAckUntil = millis() + 2500; musicDirty = true; return;
    }
    if (hidTapAt && millis() - hidTapAt < 120) return;
    hidTapAt = millis();
    if (hidReady) {
      if (!hidPulses.enqueue(button)) { musicAck = 8; musicAckUntil = millis() + 2500; musicDirty = true; }
    } else {
      musicAck = appleMedia.control(button) ? 11 : 10;
      musicAckUntil = millis() + 2500; musicDirty = true;
    }
    return;
  }
  if (!musicHave) return;
  pendingEvt = String("T,M,") + (button == 0 ? "P" : button == 1 ? "T" : "N") +
               ",233,414," + button;
  musicAck = 1; musicAckUntil = millis() + 2500;
  musicDirty = true;
}

// 松手吸附：7 帧 ease-out ≈315ms，不做惯性甩动；最后一帧滚动队列并真的发切歌。
static void cfSnapTick() {
  unsigned long now = millis();
  if (now - cfFrameAt < 40) return;                 // ≈22fps 节流
  cfFrameAt = now;
  float t = (now - cfSnapAt) / 315.0f;
  if (t >= 1.0f) t = 1.0f;
  const float e = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
  cfPhase = cfSnapFrom + (cfSnapTo - cfSnapFrom) * e;
  if (t < 1.0f) { cfDrawFrame(); return; }
  cfSnap = false;
  const int dir = cfSnapTo > 0.5f ? 1 : (cfSnapTo < -0.5f ? -1 : 0);
  cfPhase = 0.0f;
  if (dir) {
    cfShift += dir;                                // 本地队列先滚一格，手机重发后归零
    cfSendButton(dir > 0 ? 2 : 0);
    // 背景刻意不置 musicBgDirty：它固定绑队列 slot 1，等手机重发队列再重建，
    // 否则吸附瞬间会提前显示成「下一张」的背景。
    Serial.printf("[CFQ] snap dir=%d shift=%d\n", dir, cfShift);
  }
  musicDirty = true;                               // 整页重绘：文字/进度/卡组回到静止态
}

// 按样式命中 prev/play/next（0/1/2）；未命中 -1。
static int musicButtonAtStyle(int x, int y) {
  // 封面流：底栏三键 + 点左右侧卡也能切歌（点侧卡是最稳的一档，无需拖动）
  if (musicStyle == 7) {
    const int by = 414;
    if ((x - CX) * (x - CX) + (y - by) * (y - by) <= 26 * 26) return 1;
    if (musicAbs(x - (CX - 64)) <= 26 && musicAbs(y - by) <= 26) return 0;
    if (musicAbs(x - (CX + 64)) <= 26 && musicAbs(y - by) <= 26) return 2;
    if (y >= CF_HOT_Y0 && y <= CF_HOT_Y1) {
      if (x >= CF_BAND_X && x < CX - 90) return 0;
      if (x <= CF_BAND_X + CF_BAND_W && x > CX + 90) return 2;
    }
    return -1;
  }
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
