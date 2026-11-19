#pragma once
// 地图外圈、渐暗及风格特效。由 MotoNav.ino 按依赖顺序包含，仅编译一次。
// 画一整帧小地图
uint32_t mapFrames = 0, mapDrawMsAcc = 0, mapMsAcc = 0, ovlMsAcc = 0;
unsigned long statAt = 0;
String pendingStat = "";

// ---- v5.9 外圈和边缘渐暗：逐行、两个核心一起画 ----
static inline uint16_t blendF(uint16_t a, uint16_t b, uint32_t t) {   // t: 0~256，256 = 全是 b
  uint32_t ar = a >> 11, ag = (a >> 5) & 63, ab = a & 31;
  uint32_t br = b >> 11, bg = (b >> 5) & 63, bb = b & 31;
  uint32_t r = (ar * (256 - t) + br * t) >> 8, g = (ag * (256 - t) + bg * t) >> 8, bl = (ab * (256 - t) + bb * t) >> 8;
  return (uint16_t)((r << 11) | (g << 5) | bl);
}

// 快速 atan2（误差约 0.01°），返回 0~1：0 = 正上方，顺时针
static inline float ringAngle(float dx, float dy) {
  float ax = fabsf(dx), ay = fabsf(dy);
  float mx = fmaxf(ax, ay), mn = fminf(ax, ay);
  float a = mx > 0 ? mn / mx : 0, s2 = a * a;
  float r = ((-0.0464964749f * s2 + 0.15931422f) * s2 - 0.327622764f) * s2 * a + a;
  if (ay > ax) r = 1.57079637f - r;
  if (dx < 0) r = 3.14159265f - r;      // 这里的 “x” 是向上的分量，见下面调用
  if (dy < 0) r = -r;
  float t = r / 6.2831853f;
  return t < 0 ? t + 1 : t;
}

// 外圈参数
struct { uint16_t *fb; bool track; float prog; float ph; uint16_t pcol; bool over; float trip; } RP;   // v7.4 trip：行程环（-1 = 不画）
static inline float bandCov(float r, float rIn, float rOut) {
  float a = fminf(r - rIn + 0.5f, rOut - r + 0.5f);
  return a <= 0 ? 0 : (a >= 1 ? 1 : a);
}

// v6.0 整数版。坐标放大 2 倍取整：ix = 2x+1-W，iy = 2y+1-H，q = ix²+iy² = 4×半径²
#define RQ_MIN  (4 * 209 * 209)              // 半径 209 以内不用管
#define RQ_MAX  (4 * 235 * 235)              // 半径 235 以外整段清零
#define RQ_SH   4
uint16_t *rq2r = nullptr;                    // (q - RQ_MIN) >> 4 → 半径 × 64
// musicA.116：WiFi 地图加速窗口内抑制内部 RAM 可选缓存的再分配。
// 定义提前到这里，因为 ringTablesInit()（rq2r 环半径表，≈5.8KB）也要看它。
bool wifiMapNoInternalCaches = false;
int16_t ringInH[LCD_HEIGHT], ringOutH[LCD_HEIGHT];   // 每行：内圈半宽（以内跳过）、外圈半宽（以外清零）
float ringUx = 0, ringUy = -1;               // 进度条末端方向
float tripUx = 0, tripUy = -1;               // v7.4 行程环“走过 / 没走”交界方向

bool ringTablesInit() {
  if (rq2r) return true;
  if (wifiMapNoInternalCaches) return false;   // musicA.116：加速期间不再吃 5.8KB，退回老绘制路径
  int n = ((RQ_MAX - RQ_MIN) >> RQ_SH) + 2;
  rq2r = (uint16_t *)heap_caps_malloc(n * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!rq2r) return false;
  for (int k = 0; k < n; k++) rq2r[k] = (uint16_t)(sqrtf((float)(RQ_MIN + (k << RQ_SH) + 8)) * 32.0f);   // √q/2×64
  for (int y = 0; y < LCD_HEIGHT; y++) {
    int iy = 2 * y + 1 - LCD_HEIGHT;
    long iy2 = (long)iy * iy;
    int hin = 0, hout = 0;
    for (int x = LCD_WIDTH / 2; x < LCD_WIDTH; x++) {                // 右半边往外找
      int ix = 2 * x + 1 - LCD_WIDTH;
      long q = (long)ix * ix + iy2;
      if (q < RQ_MIN) hin = x - LCD_WIDTH / 2 + 1;
      if (q < RQ_MAX) hout = x - LCD_WIDTH / 2 + 1;
    }
    ringInH[y] = hin; ringOutH[y] = hout;
  }
  return true;
}

// 一段圆环的覆盖率（0~64）：r64 = 半径×64；带宽 [a, b]
static inline int bandCov64(int r64, int a, int b) {
  int c = min(r64 - (a * 64 - 32), (b * 64 + 32) - r64);
  return c <= 0 ? 0 : (c >= 64 ? 64 : c);
}

#include "../../RingFastColors.h"
uint16_t* ringFastColors=nullptr;
RingFastColors::Key ringFastKey={};
bool ringFastValid=false,ringFastTried=false;
// musicA.115：WiFi 地图加速期间抑制内部 RAM 可选缓存的再分配。
// .114 实测：reclaim 释放后雷达下一帧立即重新分配（ringFastTried 复位 / vigKeep 置空重建），
// 12.9K+12.5K 被吃回，bringup 时 largest 仍是 18420。此标志为 true 期间，
// 两个缓存都不再分配，走既有降级路径（无 fast colors / 软件 vignette）。
// musicA.116：标志本身已上移到 ringTablesInit 之前声明（rq2r 也要看它）。
void prepareRingFastColors() {
  if(!ringFastColors && !ringFastTried && !wifiMapNoInternalCaches) {
    ringFastTried=true;
    const size_t bytes=4*RingFastColors::Count*sizeof(uint16_t);
    if(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)>bytes+32768)
      ringFastColors=(uint16_t*)heap_caps_malloc(bytes,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    Serial.printf("FPS RINGCACHE %d %u\n",ringFastColors!=nullptr,(unsigned)bytes);
  }
  if(!ringFastColors)return;
  RingFastColors::Key key={RP.track,RP.trip>=0,RP.prog>0.002f,RP.over,(int)RP.ph,RP.pcol};
  if(ringFastValid && RingFastColors::same(key,ringFastKey))return;
  for(int phase=0;phase<4;phase++)for(int i=0;i<RingFastColors::Count;i++)
    ringFastColors[phase*RingFastColors::Count+i]=RingFastColors::color(i+RingFastColors::First,phase,key,RING_R,TRIP_H,C_TRACK,TRIP_DONE_C,TRIP_LEFT_C,C_RED);
  ringFastKey=key;ringFastValid=true;
}
static inline __attribute__((always_inline)) void ringPixel(uint16_t *px, int ix, long iy2, int iy) {
  long q = (long)ix * ix + iy2;
  int k = (int)((q - RQ_MIN) >> RQ_SH);
  int r64 = rq2r[k < 0 ? 0 : k];
  if(ringFastColors && ringFastValid && r64>=RingFastColors::First && r64<=RingFastColors::Last) {
    bool right=ix>=0;int phase=0;
    if(RP.trip>=0) {
      bool before=(ix*tripUy-iy*tripUx)>=0;
      bool done=RP.trip>=0.9999f?true:RP.trip<=0.0001f?false:(RP.trip<=0.5f?(right&&before):(right||before));
      if(done)phase|=2;
    }
    if(RP.prog>0.002f) {
      bool before=(ix*ringUy-iy*ringUx)>=0;
      if(RP.prog>=0.9999f || (RP.prog<=0.5f?(right&&before):(right||before)))phase|=1;
    }
    *px=ringFastColors[phase*RingFastColors::Count+r64-RingFastColors::First];return;
  }
  uint16_t c = *px;
  int a = bandCov64(r64, 211, 240);
  if (a) c = a >= 64 ? C_BLACK : blendF(c, C_BLACK, a * 4);
  if (RP.track) { a = bandCov64(r64, RING_R - 5, RING_R + 5); if (a) c = a >= 64 ? C_TRACK : blendF(c, C_TRACK, a * 4); }
  if (RP.trip >= 0) {                                  // v7.4 行程环：走过的灰，没走的绿
    a = bandCov64(r64, RING_R - TRIP_H, RING_R + TRIP_H);
    if (a) {
      bool right = ix >= 0;
      bool before = (ix * tripUy - iy * tripUx) >= 0;
      bool done = RP.trip >= 0.9999f ? true : RP.trip <= 0.0001f ? false : (RP.trip <= 0.5f ? (right && before) : (right || before));
      uint16_t tc = done ? TRIP_DONE_C : TRIP_LEFT_C;
      c = a >= 64 ? tc : blendF(c, tc, a * 4);
    }
  }
  if (RP.prog > 0.002f) {
    int h = (int)RP.ph;
    a = bandCov64(r64, RING_R - h, RING_R + h);
    if (a) {
      // 在进度范围内吗：正上方顺时针转到末端方向 (ux, uy)
      bool right = ix >= 0;
      bool before = (ix * ringUy - iy * ringUx) >= 0;
      bool in = RP.prog >= 0.9999f ? true : (RP.prog <= 0.5f ? (right && before) : (right || before));
      if (in) c = a >= 64 ? RP.pcol : blendF(c, RP.pcol, a * 4);
    }
  }
  if (RP.over) { a = bandCov64(r64, 228, 233); if (a) c = a >= 64 ? C_RED : blendF(c, C_RED, a * 4); }
  *px = c;
}

void __attribute__((optimize("O3"))) ringRows(int y0, int y1, int step) {
  uint16_t *fb = RP.fb;
  const int W = LCD_WIDTH, HW = LCD_WIDTH / 2;
  for (int y = y0; y < y1; y += step) {
    uint16_t *row = fb + y * W;
    int iy = 2 * y + 1 - LCD_HEIGHT;
    long iy2 = (long)iy * iy;
    int ho = ringOutH[y], hi = ringInH[y];
    // 圆外两头：整段清零（黑色 = 0）
    if (HW - ho > 0) memset(row, 0, (HW - ho) * 2);
    if (HW + ho < W) memset(row + HW + ho, 0, (W - HW - ho) * 2);
    // 左边一段 [HW-ho, HW-hi)，右边一段 [HW+hi, HW+ho)
    for (int x = HW - ho; x < HW - hi; x++) ringPixel(row + x, 2 * x + 1 - W, iy2, iy);
    for (int x = HW + hi; x < HW + ho; x++) ringPixel(row + x, 2 * x + 1 - W, iy2, iy);
  }
}

// 画外圈：黑边 + 进度环底（track）+ 进度条（prog 0~1，粗 thick）+ 超速红圈
void drawRingLayer(bool track, float prog, int thick, uint16_t pcol, bool over, float trip) {
  RP.fb = gfx->getFramebuffer();
  if (!RP.fb) return;
  RP.track = track; RP.prog = prog > 1 ? 1 : prog; RP.ph = thick / 2.0f; RP.pcol = pcol; RP.over = over;
  RP.trip = trip; tripUx = sinf(trip * 2 * PI); tripUy = -cosf(trip * 2 * PI);
  if (!ringTablesInit()) {                             // 内存不够：退回老办法
    fillRingBand(0, 1, 211, 240, C_BLACK);
    if (trip >= 0) drawTripRing(trip);
    if (track) drawRingTrack();
    if (prog > 0.002f) drawRingProgress(prog, thick, pcol);
    if (over) fillRingBand(0, 1, 228, 233, C_RED);
    return;
  }
  ringUx = sinf(RP.prog * 2 * PI); ringUy = -cosf(RP.prog * 2 * PI);
  prepareRingFastColors();
  parallelRows(ringRows, 0, LCD_HEIGHT);
  if (RP.prog > 0.002f) {                              // 进度条两头的圆头
    float h = RP.ph, a = RP.prog * 2 * PI;
    aaDisc(CX, CX - RING_R, h, pcol);
    aaDisc(CX + RING_R * sinf(a), CX - RING_R * cosf(a), h, pcol);
  }
  if (trip >= 0) drawTripDot(trip);
}

// 边缘渐暗：半径 r0~r1 之间越往外越暗；暗度按 “半径平方” 查表（表放在芯片内部内存）
uint8_t *vigKeep = nullptr;                // 下标 = 半径² - r0²，值 = 保留亮度（1~256 存成 0~255）
int vigK0 = 0, vigK1 = 0;

// musicA.112：SoftAP bring-up 前吐出可选 INTERNAL 缓存，给 WiFi/LWIP 连续块。
void wifiMapReclaimInternalHeap() {
  wifiMapNoInternalCaches = true;
  if (ringFastColors) {
    heap_caps_free(ringFastColors);
    ringFastColors = nullptr;
    ringFastValid = false;
    ringFastTried = true;          // 抑制再分配（WiFi 加速期间）
    Serial.println("WIFI MAP reclaim ringFastColors");
  }
  if (vigKeep) {
    heap_caps_free(vigKeep);
    vigKeep = nullptr;
    Serial.println("WIFI MAP reclaim vigKeep");
  }
  if (rq2r) {                        // musicA.116：环半径查找表 ≈5.8KB，ringTablesInit 会重建
    heap_caps_free(rq2r);
    rq2r = nullptr;
    Serial.println("WIFI MAP reclaim rq2r");
  }
}

void wifiMapRestoreInternalCaches() {
  wifiMapNoInternalCaches = false;
  ringFastTried = false;           // 允许按需重建（vigKeep 置空即自然重建）
  Serial.println("WIFI MAP caches restored");
}

struct { uint16_t *fb; float r0, r1; } VP;

void __attribute__((optimize("O3"))) vignetteRows(int y0, int y1, int step) {
  uint16_t *fb = VP.fb;
  float r0 = VP.r0, r1 = VP.r1, ri2 = r0 * r0, ro2 = r1 * r1;
  for (int y = y0; y < y1; y += step) {
    float dy = y + 0.5f - CX, dy2 = dy * dy;
    if (dy2 >= ro2) continue;
    float ho = sqrtf(ro2 - dy2), hi = dy2 < ri2 ? sqrtf(ri2 - dy2) : -1;
    uint16_t *row = fb + y * LCD_WIDTH;
    for (int side = 0; side < 2; side++) {
      int xa, xb;
      if (hi < 0) { if (side) break; xa = (int)(CX - ho); xb = (int)(CX + ho); }
      else if (side == 0) { xa = (int)(CX - ho); xb = (int)(CX - hi) + 1; }
      else { xa = (int)(CX + hi); xb = (int)(CX + ho) + 1; }
      xa = max(xa, 0); xb = min(xb, LCD_WIDTH - 1);
      for (int x = xa; x <= xb; x++) {
        float dx = x + 0.5f - CX;
        int k = (int)(dx * dx + dy2) - vigK0;
        if (k < 0 || k >= vigK1 - vigK0) continue;
        uint32_t keep = (uint32_t)vigKeep[k] + 1;
        if (keep >= 256) continue;
        uint32_t c = row[x];
        row[x] = (uint16_t)((((c >> 11) * keep >> 8) << 11) | ((((c >> 5) & 63) * keep >> 8) << 5) | ((c & 31) * keep >> 8));
      }
    }
  }
}

void drawVignette(float r0, float r1, int maxDark) {
  VP.fb = gfx->getFramebuffer();
  if (!VP.fb) return;
  if (!vigKeep && wifiMapNoInternalCaches) { radarVignette(r0, r1, maxDark); return; }   // musicA.115 抑制期走软件路径
  if (!vigKeep || vigK0 != (int)(r0 * r0) || vigK1 != (int)(r1 * r1) + 1) {
    vigK0 = (int)(r0 * r0); vigK1 = (int)(r1 * r1) + 1;
    if (!vigKeep) vigKeep = (uint8_t *)heap_caps_malloc(vigK1 - vigK0 + 64, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!vigKeep) { radarVignette(r0, r1, maxDark); return; }
    for (int k = 0; k < vigK1 - vigK0; k++) {
      float r = sqrtf((float)(k + vigK0) + 0.5f);
      float t = (r - r0) / (r1 - r0);
      if (t < 0) t = 0;
      if (t > 1) t = 1;
      int dark = (int)(t * t * maxDark);
      vigKeep[k] = (uint8_t)(255 - dark);
    }
  }
  VP.r0 = r0; VP.r1 = r1;
  parallelRows(vignetteRows, (int)(CX - r1) - 1, (int)(CX + r1) + 2);
}

// ================= v6.1 风格特效 =================
static inline void projXY(float x, float y, float cx, float cy, float sc, float cs, float sn, float &px, float &py) {
  px = CX + sc * ((x - cx) * cs - (y - cy) * sn);
  py = MAP_PY - sc * ((x - cx) * sn + (y - cy) * cs);
}

// 蓝图：跟着地图走的网格，每 50 米一条线
void drawBlueprintGrid(float cx, float cy, float sc, float cs, float sn, float viewR) {
  const float G = 50;
  uint16_t col = nightOn ? 0x1A0F : 0x2AB2;
  float x0 = floorf((cx - viewR) / G) * G, y0 = floorf((cy - viewR) / G) * G;
  for (float g = x0; g <= cx + viewR; g += G) {
    float ax, ay, bx, by;
    projXY(g, cy - viewR, cx, cy, sc, cs, sn, ax, ay); projXY(g, cy + viewR, cx, cy, sc, cs, sn, bx, by);
    gfx->drawLine((int)ax, (int)ay, (int)bx, (int)by, col);
  }
  for (float g = y0; g <= cy + viewR; g += G) {
    float ax, ay, bx, by;
    projXY(cx - viewR, g, cx, cy, sc, cs, sn, ax, ay); projXY(cx + viewR, g, cx, cy, sc, cs, sn, bx, by);
    gfx->drawLine((int)ax, (int)ay, (int)bx, (int)by, col);
  }
}

// 雷达：三圈距离环 + 绕着车转的扫描线（后面拖一串渐暗的线）
void drawRadarSweep() {
  for (int r = 70; r <= 210; r += 70) gfx->drawCircle(CX, MAP_PY, r, 0x0A44);
  float a0 = (millis() % 3200) / 3200.0f * 2 * PI;
  const int TRAIL = 26;
  for (int k = TRAIL - 1; k >= 0; k--) {
    float a = a0 - k * 0.022f;
    uint16_t c = blendF(0x00C1, 0x8FF4, (uint32_t)((TRAIL - k) * 256 / TRAIL));
    gfx->drawLine(CX, MAP_PY, (int)(CX + sinf(a) * 300), (int)(MAP_PY - cosf(a) * 300), c);
  }
}

// musicA.102 · 龙珠雷达 v3：亮前缘 + 指数软拖尾（无 blip/ping，贴 v3 mock）
// ~2.4° 前缘 + ~10° 近缘辉光 + ~55° 拖尾；顺时针 ~2.6s/周；圆心 (CX,CX)。
void drawDragonRadarSweep() {
  const float a0 = (millis() % 2600UL) / 2600.0f * 2.0f * (float)PI;
  const int TRAIL = 44;   // 44 * 0.022 ≈ 0.97 rad ≈ 55°
  for (int k = TRAIL - 1; k >= 0; k--) {
    float a = a0 - k * 0.022f;
    float t = 1.0f - (float)k / (float)TRAIL;   // 0=尾 1=前缘
    uint32_t mix = (uint32_t)(28.0f + 210.0f * t * t);  // 指数感衰减
    if (k <= 8) mix = 160 + (uint32_t)((8 - k) * 10);   // ~10° 辉光
    if (k <= 2) mix = 250;                               // ~2.4° 亮芯
    uint16_t c = blendF(0x36C7, 0xEFFF, mix > 256 ? 256 : mix);
    gfx->drawLine(CX, CX, (int)(CX + sinf(a) * 300), (int)(CX - cosf(a) * 300), c);
  }
  gfx->drawLine(CX, CX, (int)(CX + sinf(a0) * 300), (int)(CX - cosf(a0) * 300), 0xFFFF);
}

// 寻宝罗盘：屏幕上一层淡淡的六边形网格（开机第一次用时把网格上的像素位置算好存起来）
uint32_t *hexIdx = nullptr;
int hexN = 0;
bool hexTried = false;
void buildHexGrid() {
  hexTried = true;
  uint8_t *m = (uint8_t *)ps_malloc(LCD_WIDTH * LCD_HEIGHT);
  if (!m) return;
  memset(m, 0, LCD_WIDTH * LCD_HEIGHT);
  const float R = 26, H = R * 1.7320508f;
  auto line = [&](float x0, float y0, float x1, float y1) {
    int n = (int)(fmaxf(fabsf(x1 - x0), fabsf(y1 - y0))) + 1;
    for (int i = 0; i <= n; i++) {
      int x = (int)(x0 + (x1 - x0) * i / n), y = (int)(y0 + (y1 - y0) * i / n);
      if (x >= 0 && x < LCD_WIDTH && y >= 0 && y < LCD_HEIGHT) m[y * LCD_WIDTH + x] = 1;
    }
  };
  for (int col = -1; col * R * 1.5f < LCD_WIDTH + R; col++)
    for (int row = -1; row * H < LCD_HEIGHT + H; row++) {
      float cx = col * R * 1.5f, cy = row * H + ((col & 1) ? H / 2 : 0);
      for (int k = 0; k < 6; k++) {
        float a0 = k * PI / 3, a1 = (k + 1) * PI / 3;
        line(cx + R * cosf(a0), cy + R * sinf(a0), cx + R * cosf(a1), cy + R * sinf(a1));
      }
    }
  int cnt = 0;
  for (int i = 0; i < LCD_WIDTH * LCD_HEIGHT; i++) if (m[i]) cnt++;
  hexIdx = (uint32_t *)ps_malloc(sizeof(uint32_t) * (cnt + 1));
  if (hexIdx) {
    for (int y = 0; y < LCD_HEIGHT; y++) {
      float dy = y + 0.5f - CX;
      for (int x = 0; x < LCD_WIDTH; x++) {
        if (!m[y * LCD_WIDTH + x]) continue;
        float dx = x + 0.5f - CX;
        if (dx * dx + dy * dy < 206.0f * 206.0f) hexIdx[hexN++] = y * LCD_WIDTH + x;
      }
    }
  }
  free(m);
}

void drawHexGrid() {
  if (!hexTried) buildHexGrid();
  if (!hexIdx) return;
  uint16_t *fb = gfx->getFramebuffer();
  for (int i = 0; i < hexN; i++) { uint16_t &p = fb[hexIdx[i]]; p = blendF(p, 0x3A4F, 80); }
}

// 寻宝罗盘：外圈刻度（每 5° 一根，东南西北长一点）+ 方位字母，跟着车头转
void drawCompassRose(float hdgDeg) {
  for (int k = 0; k < 72; k++) {
    float a = (k * 5 - hdgDeg) * PI / 180;
    bool major = k % 18 == 0;
    float r0 = 208, r1 = major ? 192 : (k % 2 == 0 ? 199 : 203);
    float sx = sinf(a), cy = -cosf(a);
    uint16_t c = major ? 0xFD80 : 0x7B48;
    gfx->drawLine((int)(CX + sx * r0), (int)(CX + cy * r0), (int)(CX + sx * r1), (int)(CX + cy * r1), c);
    if (major) gfx->drawLine((int)(CX + sx * r0) + 1, (int)(CX + cy * r0), (int)(CX + sx * r1) + 1, (int)(CX + cy * r1), c);
  }
  const char *L[4] = {"N", "E", "S", "W"};
  for (int k = 0; k < 4; k++) {
    float a = (k * 90 - hdgDeg) * PI / 180;
    float x = CX + sinf(a) * 178, y = CX - cosf(a) * 178;
    if (k == 0) obsAdd(x - 10, y - 10, x + 10, y + 10);
    printOutlined(L[k], (int)x, (int)y - 7, 2, k == 0 ? 0xFD80 : 0x7B48);
  }
}

// 寻宝罗盘：目的地 = 琥珀色菱形 + 往外扩的脉冲圈
void drawCompassBlip(float px, float py, bool outside) {
  if (!outside) {
    float ph = (millis() % 1400) / 1400.0f;
    for (int k = 0; k < 2; k++) {
      float t = fmodf(ph + k * 0.5f, 1.0f);
      int r = (int)(12 + t * 20);
      uint16_t c = blendF(0xFD80, TH.block, (uint32_t)(t * 256));
      gfx->drawCircle((int)px, (int)py, r, c);
      gfx->drawCircle((int)px, (int)py, r + 1, c);
    }
  }
  float s = outside ? 8 : 11;
  gfx->fillTriangle(px, py - s - 2, px + s + 2, py, px - s - 2, py, C_BLACK);
  gfx->fillTriangle(px, py + s + 2, px + s + 2, py, px - s - 2, py, C_BLACK);
  gfx->fillTriangle(px, py - s, px + s, py, px - s, py, 0xFD80);
  gfx->fillTriangle(px, py + s, px + s, py, px - s, py, 0xFD80);
}
