#pragma once
// 音乐页面、手势换页和数据中断提示。由 MotoNav.ino 按依赖顺序包含，仅编译一次。
// ---- v5.5 跟手换页 ----
// 触摸坐标和画面方向相反时改成 1（拖动时画面往手指反方向走就是反了）
#define TOUCH_FLIP_X 1      // v5.5.1 实测左右是反的
#define TOUCH_FLIP_Y 1      // 实机坐标确认：上下也反向；点按和手势使用同一校正
#define DRAG_START  16        // 手指移动超过这么多像素才开始拖
uint16_t *slideOld = nullptr, *slideNew = nullptr;
int16_t chordHalf[LCD_HEIGHT];   // 每一行在圆里的半宽（圆外不用算）
bool chordReady = false;
int lastSlideSx = -1, lastSlideSy = 0;      // 上次进天气页的方向（自动回去时反着滑）
void drawNavFrame();

// v7.8 音乐页：歌名/歌手图片栏 5/6；#Z,有会话,播放中,按钮位掩码。
// zelda40.9-musicA.5（方案 A 冷雾银）：旋帧定点最近邻；底区 chrome 静态叠上；无「MUSIC」标题。
// musicA.10：纯圆裁封面（无黑胶沟槽环）+ 封面取色氛围 + 动态唱针；方案 A 色/玻璃三键保留。
// 色值：标题 #E8EEF4 / 歌手 #BAC6D2 / 时长 #A0B2C0 / 进度 #91C3D7
static const uint16_t MUSIC_COL_TITLE  = 0xEF7E;   // #E8EEF4
static const uint16_t MUSIC_COL_ARTIST = 0xBE3A;   // #BAC6D2
static const uint16_t MUSIC_COL_TIME   = 0xA598;   // #A0B2C0
static const uint16_t MUSIC_COL_PROG   = 0x967A;   // #91C3D7
static const uint16_t MUSIC_COL_ICON   = 0xEF7E;
static const int MUSIC_BTN_Y = 382;
static const int MUSIC_BTN_GAP = 102;
static const int MUSIC_BTN_R_SIDE = 28;
static const int MUSIC_BTN_R_MAIN = 34;
static uint8_t musicScrimA[LCD_HEIGHT];    // 预计算底区 scrim alpha
static bool musicScrimReady = false;
// 唱针：枢轴右上；短臂 + 方头壳 + 黑针尖；play 落边，pause 抬离。
static const float TONEARM_PIVOT_X = CX + COVER_DISC_R * 0.78f;
static const float TONEARM_PIVOT_Y = COVER_DISC_CY - COVER_DISC_R * 0.72f;
static const float TONEARM_LEN = COVER_DISC_R * 0.58f;   // musicA.11：缩短，避免压过封面
static const float TONEARM_ANG_PLAY = 2.55f;   // ≈146°：针尖落在右缘
static const float TONEARM_ANG_REST = 1.95f;   // ≈112°：抬到盘外

static inline size_t musicChromeBytes() {
  return (size_t)(LCD_HEIGHT - MUSIC_CHROME_Y0) * LCD_WIDTH * 2;
}
static inline uint16_t *musicChromeRow(int y) {
  return musicChrome + (size_t)(y - MUSIC_CHROME_Y0) * LCD_WIDTH;
}

static void musicEnsureChords() {
  if (chordReady) return;
  for (int y = 0; y < LCD_HEIGHT; y++) {
    float d = y + 0.5f - LCD_HEIGHT / 2.0f, r = LCD_WIDTH / 2.0f + 1;
    chordHalf[y] = d * d < r * r ? (int16_t)ceilf(sqrtf(r * r - d * d)) : 0;
  }
  chordReady = true;
}

static void musicEnsureScrim() {
  if (musicScrimReady) return;
  const int y0 = (int)(LCD_HEIGHT * 0.50f);
  for (int y = 0; y < LCD_HEIGHT; y++) {
    if (y < y0) { musicScrimA[y] = 0; continue; }
    float t = (float)(y - y0) / (float)(LCD_HEIGHT - y0);
    t = t * t * (3.0f - 2.0f * t);
    musicScrimA[y] = (uint8_t)(t * 140.0f);
  }
  musicScrimReady = true;
}

// 氛围底：圆屏弦内填封面取色（无模糊核，低开销；封面一换 coverAmbient 跟着变）。
static void drawMusicAmbient() {
  musicEnsureChords();
  uint16_t *fb = gfx->getFramebuffer();
  const uint16_t amb = coverValid ? coverAmbient : (uint16_t)0x0841;
  for (int y = 0; y < LCD_HEIGHT; y++) {
    int h = chordHalf[y];
    uint16_t *row = fb + (size_t)y * LCD_WIDTH;
    if (!h) { memset(row, 0, LCD_WIDTH * 2); continue; }
    int xa = CX - h, xb = CX + h;
    if (xa < 0) xa = 0;
    if (xb > LCD_WIDTH) xb = LCD_WIDTH;
    for (int x = 0; x < xa; x++) row[x] = 0;
    for (int x = xa; x < xb; x++) row[x] = amb;
    for (int x = xb; x < LCD_WIDTH; x++) row[x] = 0;
  }
}

// 纯圆裁封面：COVER_HI 上 16.16 逆变换 + 最近邻（musicA.5 FPS 课：旋帧不做双线性）。
// 无黑胶沟槽环；仅极细浅描边便于从氛围底分离。
static void drawMusicCoverDisc(float angleRad) {
  if (!coverValid) return;
  const uint16_t *src = (coverHiReady && coverHi) ? coverHi : coverImg;
  const int sw = (coverHiReady && coverHi) ? COVER_HI : coverW;
  const int sh = (coverHiReady && coverHi) ? COVER_HI : coverH;
  if (!src || sw <= 0 || sh <= 0) return;
  musicEnsureChords();
  uint16_t *fb = gfx->getFramebuffer();
  const int R = COVER_DISC_R, R2 = R * R, Rrim2 = (R + 1) * (R + 1);
  const int ca = (int)lroundf(cosf(angleRad) * 65536.0f);
  const int sa = (int)lroundf(sinf(angleRad) * 65536.0f);
  const int scale = (sw << 16) / (2 * R);                 // 源方图 → 盘直径
  const int cx0 = sw << 15, cy0 = sh << 15;
  const int y0 = COVER_DISC_CY - R - 1, y1 = COVER_DISC_CY + R + 1;
  for (int y = y0; y <= y1; y++) {
    if ((unsigned)y >= (unsigned)LCD_HEIGHT) continue;
    int h = chordHalf[y];
    if (!h) continue;
    int dy = y - COVER_DISC_CY;
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
        row[x] = blend565(row[x], 0xC618, 90);           // 极细浅描边，非黑胶环
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

static inline void tonearmPut(int x, int y, uint16_t c, int a) {
  if ((unsigned)x >= (unsigned)LCD_WIDTH || (unsigned)y >= (unsigned)LCD_HEIGHT) return;
  int h = chordHalf[y];
  if (!h || x < CX - h || x >= CX + h) return;
  uint16_t *pxp = &gfx->getFramebuffer()[(size_t)y * LCD_WIDTH + x];
  *pxp = a >= 255 ? c : blend565(*pxp, c, a);
}

// 动态唱针：短银臂 + 矩形头壳 + 黑色针尖（musicA.11）。
static void drawMusicTonearm(float t) {
  if (t < 0) t = 0; else if (t > 1) t = 1;
  musicEnsureChords();
  const float ang = TONEARM_ANG_REST + (TONEARM_ANG_PLAY - TONEARM_ANG_REST) * t;
  const float ca = cosf(ang), sa = sinf(ang);
  const float px = TONEARM_PIVOT_X, py = TONEARM_PIVOT_Y;
  // 臂端（头壳中心略短于全长，留给黑针）
  const float armLen = TONEARM_LEN * 0.82f;
  const float hx = px + armLen * ca, hy = py + armLen * sa;
  const float nx = -sa, ny = ca;                         // 臂法线
  const uint16_t armCol = 0xC618, shellCol = 0xAD55, shellHi = 0xDEFB;
  const uint16_t tipBlack = 0x0000, piv = 0xEF7E;
  // 细臂身
  int steps = (int)(armLen * 1.8f);
  if (steps < 20) steps = 20;
  for (int i = 0; i <= steps; i++) {
    float u = (float)i / (float)steps;
    int x = (int)lroundf(px + (hx - px) * u);
    int y = (int)lroundf(py + (hy - py) * u);
    int rad = (i < 3) ? 3 : 1;
    for (int dy = -rad; dy <= rad; dy++)
      for (int dx = -rad; dx <= rad; dx++)
        if (dx * dx + dy * dy <= rad * rad) tonearmPut(x + dx, y + dy, armCol, 210);
  }
  // 矩形头壳（沿臂向 + 法向展开）
  const float shellL = 11.0f, shellW = 7.0f;
  for (float along = -shellL * 0.15f; along <= shellL; along += 0.7f) {
    for (float side = -shellW; side <= shellW; side += 0.7f) {
      int x = (int)lroundf(hx + along * ca + side * nx);
      int y = (int)lroundf(hy + along * sa + side * ny);
      uint16_t c = (side < -shellW * 0.35f) ? shellHi : shellCol;
      tonearmPut(x, y, c, 230);
    }
  }
  // 黑色针尖（从头壳前端沿臂向再伸出）
  const float tipExtra = TONEARM_LEN - armLen;
  for (float along = shellL * 0.55f; along <= tipExtra; along += 0.55f) {
    float taper = along / (tipExtra + 0.01f);
    int rad = taper > 0.6f ? 1 : 2;
    int x = (int)lroundf(hx + along * ca);
    int y = (int)lroundf(hy + along * sa);
    for (int dy = -rad; dy <= rad; dy++)
      for (int dx = -rad; dx <= rad; dx++)
        if (dx * dx + dy * dy <= rad * rad) tonearmPut(x + dx, y + dy, tipBlack, 255);
  }
  // 枢轴帽
  int pxi = (int)lroundf(px), pyi = (int)lroundf(py);
  for (int dy = -5; dy <= 5; dy++)
    for (int dx = -5; dx <= 5; dx++) {
      int d2 = dx * dx + dy * dy;
      if (d2 <= 25) tonearmPut(pxi + dx, pyi + dy, piv, d2 <= 9 ? 230 : 140);
    }
}

// musicA.10 合成：氛围 → 圆封面 → 唱针（替代旧满圆铺满）。
static void drawMusicCoverScene(float angleRad) {
  drawMusicAmbient();
  if (coverValid && coverImg) drawMusicCoverDisc(angleRad);
  drawMusicTonearm(tonearmT);
}

static void drawMusicBottomScrim() {
  musicEnsureChords();
  musicEnsureScrim();
  uint16_t *fb = gfx->getFramebuffer();
  for (int y = 0; y < LCD_HEIGHT; y++) {
    int a = musicScrimA[y];
    if (!a) continue;
    int h = chordHalf[y];
    if (!h) continue;
    int xa = CX - h, xb = CX + h;
    if (xa < 0) xa = 0;
    if (xb > LCD_WIDTH) xb = LCD_WIDTH;
    uint16_t *row = fb + (size_t)y * LCD_WIDTH;
    for (int x = xa; x < xb; x++) row[x] = blend565(row[x], 0x0841, a);
  }
}

static void drawMusicProgressBar(int y, double elapsedSec, double durationSec) {
  if (durationSec <= 0) return;
  double e = constrain(elapsedSec, 0.0, durationSec);
  const int barW = 260, barH = 3, x0 = CX - barW / 2;
  gfx->fillRect(x0, y, barW, barH, 0x3186);             // 半透感浅轨
  int fill = (int)lround(barW * (e / durationSec));
  if (fill > 0) gfx->fillRect(x0, y, fill, barH, MUSIC_COL_PROG);
  int sec = (int)e, total = (int)durationSec;
  char left[8], right[8];
  snprintf(left, sizeof(left), "%d:%02d", sec / 60, sec % 60);
  int nR = snprintf(right, sizeof(right), "%d:%02d", total / 60, total % 60);
  if (nR < 0) nR = 0;
  const int tSize = 2, tY = y + 10, inset = 4, cw = 6 * tSize;
  gfx->setTextSize(tSize);
  gfx->setTextColor(MUSIC_COL_TIME);
  gfx->setCursor(x0 + inset, tY);
  gfx->print(left);
  gfx->setCursor(x0 + barW - inset - nR * cw, tY);
  gfx->print(right);
}

// 透明液态/磨砂玻璃圆钮：半透冷白叠在封面上（无 sqrt；musicA.2 提速）。
static void drawMusicGlassButton(int cx, int cy, int rad, bool enabled, bool pressed) {
  musicEnsureChords();
  uint16_t *fb = gfx->getFramebuffer();
  const int rIn = rad > 1 ? rad - 1 : rad;
  const int rIn2 = rIn * rIn, rOut2 = (rad + 1) * (rad + 1);
  const uint16_t frost = pressed ? 0xCE79 : 0xDEFB;
  int baseA = enabled ? (pressed ? 95 : 120) : 55;
  for (int dy = -rad - 1; dy <= rad + 1; dy++) {
    int y = cy + dy;
    if ((unsigned)y >= (unsigned)LCD_HEIGHT) continue;
    int h = chordHalf[y];
    if (!h) continue;
    for (int dx = -rad - 1; dx <= rad + 1; dx++) {
      int dist2 = dx * dx + dy * dy;
      if (dist2 > rOut2) continue;
      int x = cx + dx;
      if (x < CX - h || x >= CX + h) continue;
      int a;
      if (dist2 <= rIn2) {
        // 上亮下暗磨砂；用 (rIn2-dist2) 做边缘衰减
        int shade = baseA + (baseA * (rIn2 - dist2)) / (rIn2 + 1) / 3;
        shade += ((rad - dy) * 18) / (2 * rad + 1);   // 顶亮
        a = shade;
      } else {
        a = 70 * (rOut2 - dist2) / (rOut2 - rIn2 + 1);  // 边缘环
      }
      if (a < 1) continue;
      if (a > 220) a = 220;
      uint16_t *px = &fb[(size_t)y * LCD_WIDTH + x];
      *px = blend565(*px, frost, a);
    }
  }
  for (int ang = 200; ang <= 340; ang += 3) {
    float ar = ang * (float)PI / 180.0f;
    int x = cx + (int)lroundf((rad - 3) * cosf(ar));
    int y = cy + (int)lroundf((rad - 3) * sinf(ar));
    if ((unsigned)x >= (unsigned)LCD_WIDTH || (unsigned)y >= (unsigned)LCD_HEIGHT) continue;
    int h = chordHalf[y];
    if (!h || x < CX - h || x >= CX + h) continue;
    uint16_t *px = &fb[(size_t)y * LCD_WIDTH + x];
    *px = blend565(*px, 0xFFFF, enabled ? 140 : 70);
  }
}

static void drawMusicControlIcons(int cx, int cy, int i, bool playingUnknown, bool playing, uint16_t c) {
  if (i == 1) {
    if (playingUnknown) {
      gfx->fillTriangle(cx - 14, cy - 12, cx - 14, cy + 12, cx + 2, cy, c);
      gfx->fillRect(cx + 6, cy - 12, 4, 24, c); gfx->fillRect(cx + 13, cy - 12, 4, 24, c);
    } else if (playing) {
      gfx->fillRect(cx - 10, cy - 12, 6, 24, c);
      gfx->fillRect(cx + 5, cy - 12, 6, 24, c);
    } else {
      gfx->fillTriangle(cx - 8, cy - 14, cx - 8, cy + 14, cx + 14, cy, c);
    }
  } else {
    int sign = i == 0 ? -1 : 1;
    gfx->fillTriangle(cx - sign * 10, cy - 12, cx - sign * 10, cy + 12, cx + sign * 8, cy, c);
    gfx->fillRect(cx + sign * 12 - 2, cy - 12, 5, 24, c);
  }
}

// 底区 chrome 叠到 fb（非 0 = 不透明）——旋封面之上，玻璃键不随封面转/闪。
static void blitMusicChrome() {
  if (!musicChrome || !musicChromeReady) return;
  musicEnsureChords();
  uint16_t *fb = gfx->getFramebuffer();
  for (int y = MUSIC_CHROME_Y0; y < LCD_HEIGHT; y++) {
    int h = chordHalf[y];
    if (!h) continue;
    int xa = CX - h, xb = CX + h;
    if (xa < 0) xa = 0;
    if (xb > LCD_WIDTH) xb = LCD_WIDTH;
    const uint16_t *src = musicChromeRow(y);
    uint16_t *dst = fb + (size_t)y * LCD_WIDTH;
    for (int x = xa; x < xb; x++) if (src[x]) dst[x] = src[x];
  }
}

// 差分捕获底区 overlay（相对 cover+scrim 底）。
static void musicCaptureChrome(const uint16_t *baseFb) {
  musicChromeReady = false;
  if (!baseFb) return;
  if (!musicChrome) musicChrome = (uint16_t *)ps_malloc(musicChromeBytes());
  if (!musicChrome) return;
  const uint16_t *fb = gfx->getFramebuffer();
  for (int y = MUSIC_CHROME_Y0; y < LCD_HEIGHT; y++) {
    const uint16_t *brow = baseFb + (size_t)y * LCD_WIDTH;
    const uint16_t *frow = fb + (size_t)y * LCD_WIDTH;
    uint16_t *crow = musicChromeRow(y);
    for (int x = 0; x < LCD_WIDTH; x++) crow[x] = (frow[x] != brow[x]) ? frow[x] : 0;
  }
  musicChromeReady = true;
}

// 歌名/歌手/进度/玻璃三键（静止 overlay）。
static void drawMusicOverlay() {
  const int titleY = 242, artistY = 276;
  bool linked = bleConnected && bleAuthed;
  if (hidMusicMode && linked && !appleMedia.available) {
    printCentered("PHONE MEDIA KEYS", CX, titleY, 2, MUSIC_COL_TITLE);
    printCentered("OPEN MUSIC APP ON PHONE", CX, artistY, 2, MUSIC_COL_ARTIST);
  } else if (linked) {
    drawTextImgC(5, titleY, 0xE8, MUSIC_COL_TITLE);
    drawTextImgC(6, artistY, 0xC8, MUSIC_COL_ARTIST);
  } else {
    printCentered("PHONE OFFLINE", CX, 260, 3, MUSIC_COL_ARTIST);
  }

  const char *feedback[] = {"", "SENT", "PHONE RECEIVED", "DONE", "NO RESPONSE", "NO PLAYER", "HID SENT", "HID NOT READY", "HID SEND FAILED", "AMS SENT", "AMS SEND FAILED", "AMS QUEUED"};
  const char *hint = nullptr;
  if (musicAck > 0 && musicAck <= 11) hint = feedback[musicAck];
  else if (hidMusicMode && linked) {
    if (!hidMediaReady() && !appleMedia.controlMask()) hint = "PAIR HID IN PHONE SETTINGS";
    else if (!hidMediaReady() && appleMedia.controlMask()) hint = "AMS MEDIA READY";
    else if (!appleMedia.available) hint = "HID READY / AMS UNAVAILABLE";
  }
  // musicA.38：原版冷雾银在歌名下叠 1–2 行歌词（复用 #ML / imgs[12]）
  bool showLy = linked && hidMusicMode && musicLyrics && appleMedia.available && imgs[12].showW > 0;
  if (showLy) drawTextImgC(12, 300, 0xD0, MUSIC_COL_TITLE);
  else if (hint) printCentered(hint, CX, 308, 2, musicAck == 4 ? C_ORANGE : MUSIC_COL_TIME);

  int progY = showLy ? 336 : 318;
  if (hidMusicMode && appleMedia.available) {
    double duration = AppleMediaData::duration(appleMedia.values[3].text.c_str());
    if (duration > 0 && appleMedia.playback.known) {
      double elapsed = appleMedia.playback.elapsed + appleMedia.playback.rate * (millis() - appleMedia.playbackAt) / 1000.0;
      drawMusicProgressBar(progY, elapsed, duration);
    }
  } else if (musicDurMs > 0) {
    double duration = musicDurMs / 1000.0;
    double elapsed = musicElapsedMs / 1000.0;
    if (musicPlaying && musicProgAt) elapsed += (millis() - musicProgAt) / 1000.0;
    drawMusicProgressBar(progY, elapsed, duration);
  }

  for (int i = 0; i < 3; i++) {
    int x = CX + (i - 1) * MUSIC_BTN_GAP;
    int r = (i == 1) ? MUSIC_BTN_R_MAIN : MUSIC_BTN_R_SIDE;
    bool enabled = hidMusicMode ? (hidMediaReady() || appleMedia.controlReady(i)) : linked && musicHave;
    drawMusicGlassButton(x, MUSIC_BTN_Y, r, enabled, enabled && musicPressed == i);
    uint16_t ic = enabled ? MUSIC_COL_ICON : 0x7BEF;
    bool unk = hidMusicMode && !appleMedia.playback.known;
    drawMusicControlIcons(x, MUSIC_BTN_Y, i, unk && i == 1, musicPlaying, ic);
  }
}

#include "MusicThemes.h"

void drawMusic() {
  onWaitScreen = false;
  frameIsMap = false;
  // musicA.38：非原版主题走独立布局（无唱针旋帧 chrome）；原版保持冷雾银路径。
  if (musicStyle == 1) { drawMusicThemeCarousel(); needFlush = true; musicChromeReady = false; return; }
  if (musicStyle == 2) { drawMusicThemeSoftNeon(); needFlush = true; musicChromeReady = false; return; }
  if (musicStyle == 3) { drawMusicThemeLyrics();   needFlush = true; musicChromeReady = false; return; }
  if (musicStyle == 4) { drawMusicThemeGauge();    needFlush = true; musicChromeReady = false; return; }

  drawMusicCoverScene(coverAngle);
  drawMusicBottomScrim();

  // 底区快照作差分底，再画玻璃 overlay → 捕获为静态 chrome。
  static uint16_t *musicBase = nullptr;
  const size_t BYTES = (size_t)LCD_WIDTH * LCD_HEIGHT * 2;
  if (!musicBase) musicBase = (uint16_t *)ps_malloc(BYTES);
  if (musicBase) memcpy(musicBase, gfx->getFramebuffer(), BYTES);
  drawMusicOverlay();
  if (musicBase) musicCaptureChrome(musicBase);
  needFlush = true;
}

// 旋封面帧：氛围+圆封面+唱针+scrim，再 blit 静态 chrome（玻璃键不闪）。
static void drawMusicSpinFrame() {
  onWaitScreen = false;
  frameIsMap = false;
  drawMusicCoverScene(coverAngle);
  drawMusicBottomScrim();
  if (musicChromeReady) blitMusicChrome();
  else drawMusicOverlay();                               // chrome 未就绪时兜底
  needFlush = true;
}

// 拖动状态
bool dragging = false;
int dragTarget = 0, dragSign = 1;
bool dragHoriz = true;
float dragP = 0, dragV = 0;                 // 新页面已经进来多少像素；速度（像素/毫秒）
unsigned long dragT = 0;

uint16_t *shownFrame() {
  if (fbBuf[1] && flushSrc) return flushSrc;
  return gfx->getFramebuffer();
}

static inline uint16_t dim565(uint16_t c, uint32_t k) {   // k: 0~32，32 = 原样
  uint32_t r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
  return (uint16_t)(((r * k >> 5) << 11) | ((g * k >> 5) << 5) | (b * k >> 5));
}

// 画一个页面到画布（不改当前页）
// zelda40.7：page 1 右滑槽位由天气改为电子吧唧；首页天气展示仍由 weatherTick/HomePage 负责。
void renderPage(int p) {
  int keep = page;
  bool ks = sliding;
  page = p; sliding = false;
  if (p == 2) drawMusic();
  else if (p == 1) drawBadgePage();                 // 右滑：电子吧唧（原天气页槽位）
  else if (p == 3) drawGForce();                    // v7.12.9-ui.6 上划 G 值速度页
  else if (p == 4) drawTpms();                      // v7.12.9-ui.6 下划胎压页
  else if (inNav) drawNavFrame();
  else drawWaiting(bleConnected ? WAIT_READY : WAIT_NOLINK);
  page = keep; sliding = ks;
}

// 准备滑动：存下屏幕上的旧画面，在画布上画好目标页面存起来
bool slidePrepare(int target) {
  const size_t BYTES = (size_t)LCD_WIDTH * LCD_HEIGHT * 2;
  if (!slideOld) slideOld = (uint16_t *)heap_caps_malloc(BYTES, MALLOC_CAP_SPIRAM);
  if (!slideNew) slideNew = (uint16_t *)heap_caps_malloc(BYTES, MALLOC_CAP_SPIRAM);
  if (!slideOld || !slideNew) return false;
  if (!chordReady) {
    for (int y = 0; y < LCD_HEIGHT; y++) {
      float d = y + 0.5f - LCD_HEIGHT / 2.0f, r = LCD_WIDTH / 2.0f + 1;
      chordHalf[y] = d * d < r * r ? (int16_t)ceilf(sqrtf(r * r - d * d)) : 0;
    }
    chordReady = true;
  }
  if (fbBuf[1]) { flushWait(); xSemaphoreGive(flushDone); }   // 等正在推的那帧推完，旧画面才完整
  memcpy(slideOld, shownFrame(), BYTES);
  renderPage(target);
  memcpy(slideNew, gfx->getFramebuffer(), BYTES);
  sliding = true;
  needFlush = false; frameIsMap = false;
  return true;
}

// 合成一帧：新页面已经进来 p 像素（盖在上面），旧页面往同方向退 30%、变暗，交界处一道阴影
void slideCompose(int p) {
  const int W = LCD_WIDTH, H = LCD_HEIGHT;
  if (p < 0) p = 0;
  if (p > (dragHoriz ? W : H)) p = dragHoriz ? W : H;
  uint16_t *fb = gfx->getFramebuffer();
  int full = dragHoriz ? W : H;
  int q = p * 3 / 10;                                   // 旧页面退后的距离
  uint32_t k0 = 32 - (uint32_t)(p * 13 / full);         // 旧页面整体亮度：100% → 约 60%
  const int SH = 22;                                    // 阴影宽度
  if (dragHoriz) {
    static uint8_t colK[LCD_WIDTH];
    for (int x = 0; x < W; x++) {
      int dist = dragSign > 0 ? x - p : (W - p - 1) - x;   // 离交界多远（旧页面这一侧）
      uint32_t k = k0;
      if (p > 0 && dist < SH) k = k * (14 + dist * 18 / SH) / 32;
      colK[x] = (uint8_t)k;
    }
    for (int y = 0; y < H; y++) {
      int h = chordHalf[y];
      if (!h) continue;
      int xa = max(0, W / 2 - h), xb = min(W, W / 2 + h);
      uint16_t *row = fb + (size_t)y * W;
      const uint16_t *o = slideOld + (size_t)y * W, *n = slideNew + (size_t)y * W;
      if (dragSign > 0) {                               // 新页面从左边进来
        int a = xa, b = min(p, xb);
        if (b > a) memcpy(row + a, n + (W - p) + a, (b - a) * 2);
        for (int x = max(p, xa); x < xb; x++) row[x] = colK[x] >= 32 ? o[x - q] : dim565(o[x - q], colK[x]);
      } else {                                          // 新页面从右边进来
        int a = max(W - p, xa), b = xb;
        if (b > a) memcpy(row + a, n + (a - (W - p)), (b - a) * 2);
        for (int x = xa; x < min(W - p, xb); x++) row[x] = colK[x] >= 32 ? o[x + q] : dim565(o[x + q], colK[x]);
      }
      // 新页面前沿一道细亮边，看得出是一张“卡片”盖过来
      if (p > 1 && p < W - 1) {
        int ex = dragSign > 0 ? p - 1 : W - p;
        if (ex >= xa && ex < xb) row[ex] = 0x4A69;
        int ex2 = ex + (dragSign > 0 ? -1 : 1);
        if (ex2 >= xa && ex2 < xb) row[ex2] = 0x2945;
      }
    }
  } else {
    for (int y = 0; y < H; y++) {
      int h = chordHalf[y];
      if (!h) continue;
      int xa = max(0, W / 2 - h), xb = min(W, W / 2 + h);
      uint16_t *row = fb + (size_t)y * W;
      bool isNew = dragSign > 0 ? y < p : y >= H - p;
      if (isNew) {
        int sy = dragSign > 0 ? y + (H - p) : y - (H - p);
        memcpy(row + xa, slideNew + (size_t)sy * W + xa, (xb - xa) * 2);
        int edge = dragSign > 0 ? p - 1 : H - p;                // 前沿细亮边
        if (p > 1 && p < H - 1 && (y == edge || y == edge + (dragSign > 0 ? -1 : 1))) {
          uint16_t ec = y == edge ? 0x4A69 : 0x2945;
          for (int x = xa; x < xb; x++) row[x] = ec;
        }
      } else {
        int sy = dragSign > 0 ? y - q : y + q;
        int dist = dragSign > 0 ? y - p : (H - p - 1) - y;
        uint32_t k = k0;
        if (p > 0 && dist < SH) k = k * (14 + dist * 18 / SH) / 32;
        const uint16_t *o = slideOld + (size_t)sy * W;
        for (int x = xa; x < xb; x++) row[x] = k >= 32 ? o[x] : dim565(o[x], k);
      }
    }
  }
  flushAsync();                                         // 两块画布轮流：推这一帧的同时画下一帧
  needFlush = false; frameIsMap = false;
}

// 换页完成
void slideCommit() {
  const size_t BYTES = (size_t)LCD_WIDTH * LCD_HEIGHT * 2;
  sliding = false; dragging = false;
  page = dragTarget;
  pageAt = millis();
  memcpy(gfx->getFramebuffer(), slideNew, BYTES);
  needFlush = true; frameIsMap = false;
  if (page == 2) {
    onWaitScreen = false; musicDirty = true; pendingEvt = "T,M,Q";
  } else if (page == 1) {
    lastSlideSx = dragHoriz ? dragSign : 0; lastSlideSy = dragHoriz ? 0 : dragSign;
    onWaitScreen = false;
    badgeDirty = true;                                 // 右滑进入电子吧唧
  } else {
    if (inNav) navDirty = true;
    else {
      // musicA.2：从音乐页滑回时强制恢复待机态 + 整环/时钟，避免 onWaitScreen 残留 false、秒刻度被擦光。
      onWaitScreen = true;
      ringShownSec = 255;
      riderPrevF = -1; riderPrevZ = -2;
      catForce = true;
      drawWaiting(shownConn ? WAIT_READY : WAIT_NOLINK);
    }
  }
}

// 没换成：回到原页面
void slideCancel() {
  sliding = false; dragging = false;
  renderPage(page);
  if (page == 2) musicDirty = true;
  else if (page == 1) badgeDirty = true;
  else if (inNav) navDirty = true;
  else { onWaitScreen = true; catForce = true; }
  needFlush = true;
}

// 从 p0 顺势滑到 p1（先快后慢）；ms = 时长
void slideAnimate(float p0, float p1, int ms) {
  unsigned long t0 = millis();
  for (;;) {
    float t = (millis() - t0) / (float)ms;
    if (t >= 1) break;
    float e = 1 - (1 - t) * (1 - t) * (1 - t);         // ease-out
    slideCompose((int)(p0 + (p1 - p0) * e));
  }
  slideCompose((int)p1);
}

// 触摸向量统一换算：先修正芯片坐标的镜像，再按刷屏的逆旋转映射到画布。
// 位移和相对屏幕中心的点都使用这一个函数，避免旋转后点按与手势方向不一致。
void touchVectorToCanvas(float &dx, float &dy) {
  if (TOUCH_FLIP_X) dx = -dx;
  if (TOUCH_FLIP_Y) dy = -dy;
  float ox = dx, oy = dy;
  if (rotFree) { dx = ox * rotCosF + oy * rotSinF; dy = -ox * rotSinF + oy * rotCosF; }
  else if (screenRot == 1) { dx = oy; dy = -ox; }
  else if (screenRot == 2) { dx = -ox; dy = -oy; }
  else if (screenRot == 3) { dx = -oy; dy = ox; }
}

// 手指在拖：(dx, dy) = 从按下到现在的位移
void dragMove(int dx, int dy, unsigned long now) {
  float vx = dx, vy = dy;
  touchVectorToCanvas(vx, vy);
  dx = (int)lroundf(vx); dy = (int)lroundf(vy);
  if (!dragging) {
    int threshold = page == 2 ? 36 : DRAG_START;
    if (abs(dx) < threshold && abs(dy) < threshold) return;
    dragHoriz = abs(dx) >= abs(dy);
    int d = dragHoriz ? dx : dy;
    dragSign = d > 0 ? 1 : -1;
    // v7.12.9-ui.7 所有副页（音乐/天气/G值/胎压）任意方向滑动都回主页——
    // 和天气页一样，同方向再滑一次也能回来，四个方向都能循环切换
    if (page >= 1) dragTarget = 0;
    else if (dragHoriz) dragTarget = dragSign < 0 ? 1 : 2;  // 右滑=吧唧(page1) / 另一侧=音乐；天气页槽位已让给吧唧
    else dragTarget = dragSign < 0 ? 3 : 4;         // ui.6 主页：上划 G 值速度页，下划胎压页（原来上下滑都是天气）
    if (!slidePrepare(dragTarget)) return;
    dragging = true;
    dragP = 0; dragV = 0; dragT = now;
  }
  float p = (dragHoriz ? dx : dy) * dragSign;
  if (p < 0) p = 0;
  float dt = now - dragT;
  if (dt > 0) dragV = dragV * 0.5f + ((p - dragP) / dt) * 0.5f;
  dragP = p; dragT = now;
  slideCompose((int)p);
}

// 松手：拖过 30% 或者往前甩出去 → 顺势滑完；否则弹回
void dragEnd() {
  if (!dragging) return;
  int full = dragHoriz ? LCD_WIDTH : LCD_HEIGHT;
  bool go = dragP > full * 0.3f || (dragV > 0.35f && dragP > 24);
  if (dragV < -0.35f) go = false;                       // 往回甩：取消
  float target = go ? full : 0;
  float left = fabsf(target - dragP);
  int ms = 140 + (int)(260 * left / full);
  if (go && dragV > 0.5f) ms = max(140, (int)(ms * 0.75f));   // 甩得快就滑得快
  slideAnimate(dragP, target, ms);
  if (go) slideCommit(); else slideCancel();
}

// 来电等情况：直接放弃拖动
void dragAbort() {
  if (!dragging) return;
  sliding = false; dragging = false;
  if (page == 2) musicDirty = true; else if (page == 1) badgeDirty = true; else if (inNav) navDirty = true;
}

// 将触摸点换算到画布坐标，按钮使用较大的点按范围。
int musicButtonAt(int x, int y) {
  float dx = x - CX, dy = y - CX;
  touchVectorToCanvas(dx, dy);
  x = lroundf(CX + dx); y = lroundf(CX + dy);
  return musicButtonAtStyle(x, y);   // musicA.38：按当前音乐样式命中
}

void musicTap(int x, int y) {
  if (dragging || page != 2 || callOn || pairOn || !bleConnected || !bleAuthed) return;
  int button = musicButtonAt(x, y);
  if (hidMusicMode) {
    if (button < 0) return;
    bool hidReady = hidMediaReady();
    if (!hidReady && !appleMedia.controlReady(button)) { musicAck = 7; musicAckUntil = millis() + 2500; musicDirty = true; return; }
    if (hidTapAt && millis() - hidTapAt < 120) return;
    hidTapAt = millis();
    if (!hidReady) {
      musicAck = appleMedia.control(button) ? 11 : 10; musicAckUntil = millis() + 2500; musicDirty = true; return;
    }
    if (!hidPulses.enqueue(button)) { musicAck = 8; musicAckUntil = millis() + 2500; musicDirty = true; }
    return; // no T,M,P/T/N: do not also control Apple Music through the App
  }
  if (button < 0 || !musicHave) {
    pendingEvt = String("T,M,H,") + x + "," + y + "," + button;
    return;
  }
  pendingEvt = String("T,M,") + (button == 0 ? "P" : button == 1 ? "T" : "N") + "," + x + "," + y + "," + button;
  musicAck = 1; musicAckUntil = millis() + 2500; musicDirty = true;
}

// 自动换页（导航中看天气超时 / 快到路口）：整段动画
void showPageAnimated(int target, int sx, int sy) {
  if (page == target) return;
  dragHoriz = sx != 0 || sy == 0;
  dragSign = dragHoriz ? (sx ? sx : 1) : sy;
  dragTarget = target;
  if (!slidePrepare(target)) { page = target; renderPage(target); needFlush = true; return; }
  dragging = true;
  slideAnimate(0, dragHoriz ? LCD_WIDTH : LCD_HEIGHT, 420);
  slideCommit();
}

// 音乐页在路口前回导航，其余时候保持显示，便于连续切歌。
// zelda40.9-musicA.5：预放大 COVER_HI 最近邻慢旋；脏帧重建 chrome，旋帧只 blit 静态玻璃/文案。
// musicA.10：唱针随播停缓动；旋帧重画氛围/圆封面/唱针（唱针在 chrome 上方）。
void musicTick() {
  if (musicAckUntil && (long)(millis() - musicAckUntil) >= 0) {
    if (musicAck == 1) { musicAck = 4; musicAckUntil = millis() + 2500; }
    else { musicAck = 0; musicAckUntil = 0; }
    musicDirty = true;
  }
  if (page != 2 || callOn || pairOn || sliding) return;
  if (inNav && !cruiseOn && navDir != "S" && navDir.length() && navDist > 0 && navDist < 200 && millis() - pageAt > 3000) {
    showPageAnimated(0, -1, 0);
    return;
  }
  bool localProg = (!hidMusicMode && musicDurMs > 0 && musicPlaying) ||
                   (hidMusicMode && appleMedia.available && appleMedia.playback.known);
  static unsigned long musicProgDrawAt = 0;
  if (localProg && millis() - musicProgDrawAt >= 500) { musicProgDrawAt = millis(); musicDirty = true; }

  // 唱针缓动仅原版·冷雾银（与封面角解耦）
  if (musicStyle == 0) {
    float target = musicPlaying ? 1.0f : 0.0f;
    float d = target - tonearmT;
    if (fabsf(d) > 0.001f) {
      tonearmT += d * 0.18f;
      if (tonearmT < 0) tonearmT = 0;
      if (tonearmT > 1) tonearmT = 1;
      if (fabsf(target - tonearmT) < 0.01f) tonearmT = target;
      musicDirty = true;                                 // 臂动期间重建（含 chrome 差分底）
    }
  }

  bool spinning = (musicStyle == 0) && coverSpinEnabled && coverValid && coverImg && musicPlaying;
  bool spinFrame = false;
  if (spinning) {
    unsigned long now = millis();
    if (!coverSpinAt) coverSpinAt = now;
    unsigned long dt = now - coverSpinAt;
    if (dt >= 42) {                                      // v7.12.10 旋转帧率锁定 ~24fps（1000/24≈41.7ms）
      coverAngle += (float)dt * (2.0f * (float)PI / 36000.0f);
      if (coverAngle > 2.0f * (float)PI) coverAngle -= 2.0f * (float)PI;
      coverSpinAt = now;
      spinFrame = true;
    }
  } else {
    coverSpinAt = millis();
  }
  if (musicDirty || (musicStyle == 0 && !musicChromeReady)) {
    musicDirty = false;
    drawMusic();                                         // 全量 + 捕获 chrome
  } else if (spinFrame) {
    drawMusicSpinFrame();                                // 旋封面 + blit 静态 chrome
  }
}

// 每圈调用：首页天气请求/刷新。右滑页已改为吧唧，不再进入独立天气页。
void weatherTick() {
  unsigned long now = millis();
  // v7.12.9-ui.4x 待机页也自动要天气（首连时待机天气一直空白）：
  // 待机中 + 蓝牙已连 + 没数据或过期 15 分钟 → 发 T,W 让手机查天气推过来；
  // wxAskAt==0 表示开机后还没要过，连上立即要；之后 30 秒节流，防止没配置天气 Key 时狂刷
  if (page == 0 && onWaitScreen && !sliding && !callOn && !pairOn && bleConnected &&
      (!wxHave || now - wxAt > 15UL * 60 * 1000) &&
      (wxAskAt == 0 || now - wxAskAt > 30000)) {
    pendingEvt = "T,W"; wxAskAt = now;
  }
  // 独立天气页已停用（右滑=吧唧）；仍消费 wxDirty 以驱动首页重画路径。
  if (page != 0 || callOn || sliding) return;
}

void drawStaleOverlay() {
  drawRedRing();
  printCentered("NO DATA", CX, 412, 3, C_RED);
  needFlush = true;
}
