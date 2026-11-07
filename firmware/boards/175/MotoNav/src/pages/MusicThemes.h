#pragma once
// musicA.38 左划音乐页主题渲染（由 PageNavigation.h 在静态 helper 之后 include）。
// 样式 ID：0 原版·冷雾银 / 1 封面轮播 / 2 柔光立体 / 3 歌词主导 / 4 极简仪表。
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

// 圆角方封：从 cover 采样到 fb 的圆角矩形区域（邻居可 dim）。
static void drawMusicRoundedSquare(int cx, int cy, int half, int radius, int dimK, float angleRad) {
  if (!coverValid || !coverImg) return;
  const uint16_t *src = (coverHiReady && coverHi) ? coverHi : coverImg;
  const int sw = (coverHiReady && coverHi) ? COVER_HI : coverW;
  const int sh = (coverHiReady && coverHi) ? COVER_HI : coverH;
  if (!src || sw <= 0 || sh <= 0) return;
  musicEnsureChords();
  uint16_t *fb = gfx->getFramebuffer();
  const int ca = (int)lroundf(cosf(angleRad) * 65536.0f);
  const int sa = (int)lroundf(sinf(angleRad) * 65536.0f);
  const int scale = (sw << 16) / (2 * half);
  const int cx0 = sw << 15, cy0 = sh << 15;
  const int r2 = radius * radius;
  for (int y = cy - half; y < cy + half; y++) {
    if ((unsigned)y >= (unsigned)LCD_HEIGHT) continue;
    int h = chordHalf[y];
    if (!h) continue;
    int dy = y - cy;
    for (int x = cx - half; x < cx + half; x++) {
      if (x < CX - h || x >= CX + h) continue;
      int dx = x - cx;
      // 圆角：四角外圆外丢弃
      int ax = abs(dx) - (half - radius);
      int ay = abs(dy) - (half - radius);
      if (ax > 0 && ay > 0 && ax * ax + ay * ay > r2) continue;
      const int dxS = dx * scale, dyS = dy * scale;
      int u = ((dxS >> 8) * (ca >> 8) + (dyS >> 8) * (sa >> 8)) + cx0;
      int v = (-(dxS >> 8) * (sa >> 8) + (dyS >> 8) * (ca >> 8)) + cy0;
      int ui = u >> 16, vi = v >> 16;
      if ((unsigned)ui >= (unsigned)sw || (unsigned)vi >= (unsigned)sh) continue;
      uint16_t c = src[(size_t)vi * sw + ui];
      if (dimK < 32) c = musicDim565(c, (uint32_t)dimK);
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
static void drawMusicThemeCarousel() {
  drawMusicAmbient();
  const int half = 78, rad = 18;
  const int cy = 130;
  // 邻封（同图变暗）
  drawMusicRoundedSquare(CX - 150, cy, 54, 14, 12, 0);
  drawMusicRoundedSquare(CX + 150, cy, 54, 14, 12, 0);
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
static void drawMusicThemeSoftNeon() {
  drawMusicAmbient();
  const int half = 100, rad = 22, cy = 128;
  drawMusicRoundedSquare(CX, cy, half, rad, 32, 0);
  bool enabled = hidMusicMode ? (hidMediaReady() || appleMedia.controlReady(1)) : bleConnected && musicHave;
  drawNeonPlayOrb(CX, cy, 46, musicPlaying, enabled && musicPressed == 1);
  drawMusicMetaCentered(250, 284);
  if (musicLyricsReady()) drawTextImgC(12, 318, 0xD0, MUSIC_COL_TITLE);
  // 底栏上/下一首（磨砂圆）
  for (int i = 0; i < 2; i++) {
    int bi = i == 0 ? 0 : 2;
    int x = CX + (i == 0 ? -90 : 90);
    int y = 400;
    bool en = hidMusicMode ? (hidMediaReady() || appleMedia.controlReady(bi)) : bleConnected && musicHave;
    drawMusicGlassButton(x, y, 26, en, en && musicPressed == bi);
    drawMusicControlIcons(x, y, bi, false, musicPlaying, en ? MS_NEON : (uint16_t)0x7BEF);
  }
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
  // 深底
  musicEnsureChords();
  uint16_t *fb = gfx->getFramebuffer();
  for (int y = 0; y < LCD_HEIGHT; y++) {
    int h = chordHalf[y];
    uint16_t *row = fb + (size_t)y * LCD_WIDTH;
    if (!h) { memset(row, 0, LCD_WIDTH * 2); continue; }
    int xa = CX - h, xb = CX + h;
    if (xa < 0) xa = 0;
    if (xb > LCD_WIDTH) xb = LCD_WIDTH;
    for (int x = 0; x < xa; x++) row[x] = 0;
    for (int x = xa; x < xb; x++) row[x] = 0x1082;
    for (int x = xb; x < LCD_WIDTH; x++) row[x] = 0;
  }
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

// 按样式命中 prev/play/next（0/1/2）；未命中 -1。
static int musicButtonAtStyle(int x, int y) {
  // 触摸坐标已在 musicButtonAt 里校正到画布系
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
