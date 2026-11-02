#pragma once
// 首页骑手动画和等待画面。由 MotoNav.ino 按依赖顺序包含，仅编译一次。
// ================= 整屏画面 =================

#define WAIT_READY  0
#define WAIT_NOLINK 1
#define WAIT_LOST   2

// ================= 塞尔达40周年特别版 待机动画（v7.12.10-ui.2-zelda40） =================
// 在线：林克单手抱塞尔达骑兽形摩托，大师之剑 举/挥 两帧循环（1.3s/帧），
//       车身 ±3px 悬浮、车尾尾气、风线刮过（青光呼吸烘在精灵颜色里）
// 离线：故障摩托在左冒白烟（青光喘熄烘在精灵里），林克躺在塞尔达怀里睡（右），ZZZ 漂浮
// 素材由 motonav-effect/zelda_assets.py 生成；布局常量与效果图 anim_v4.py 定稿一致
#include "../../resources/ZeldaHomeBg.h"
#include "../../resources/ZeldaHomeSprites.h"

#define Z_GOLD     0xFE27                                       // 金黄 (255,200,60)
#define Z_GOLD_SUB 0xD5F1                                       // 暗金 (210,190,140)

bool onWaitScreen = false;
bool sliding = false;                       // v5.5 正在拖动/滑动换页：别的画面先别画
int waitSt = 0;
int riderPrevF = -1, riderPrevZ = -2;       // 变量名保留（PageNavigation.h 沿用）
bool catForce = false;                      // 首页刚重画：马上画一次

// 从待机背景纹理恢复一块区域（代替擦黑）
void zbgRect(int x, int y, int w, int h) {
  uint16_t *fb = gfx->getFramebuffer();
  if (!fb) { gfx->fillRect(x, y, w, h, C_BLACK); return; }
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > 466) w = 466 - x;
  if (y + h > 466) h = 466 - y;
  if (w <= 0 || h <= 0) return;
  for (int yy = 0; yy < h; yy++)
    memcpy(fb + (size_t)(y + yy) * 466 + x, ZeldaHomeBg + (size_t)(y + yy) * 466 + x, w * 2);
}

// StatusWidgets.h 的擦除钩子：待机页恢复纹理背景，其他页黑底
void statusErase(int x, int y, int w, int h) {
  if (onWaitScreen && !sliding) { zbgRect(x, y, w, h); return; }
  gfx->fillRect(x, y, w, h, C_BLACK);
}

// 网格精灵按 cell 像素放大绘制（1bpp 掩码跳过背景；缺省 3px，离线场景用 2px 缩小）
void zBlit(int x0, int y0, int w, int h, const uint16_t *rgb, const uint8_t *mask, int cell = ZSPRITE_CELL) {
  uint16_t *fb = gfx->getFramebuffer();
  if (!fb) return;
  const int bpr = (w + 7) / 8;
  for (int yy = 0; yy < h; yy++) {
    for (int xx = 0; xx < w; xx++) {
      if (!((mask[yy * bpr + (xx >> 3)] >> (xx & 7)) & 1)) continue;
      uint16_t c = rgb[yy * w + xx];
      int px = x0 + xx * cell, py = y0 + yy * cell;
      for (int cy = 0; cy < cell; cy++)
        for (int cx = 0; cx < cell; cx++) {
          int X = px + cx, Y = py + cy;
          if ((unsigned)X < 466 && (unsigned)Y < 466) fb[Y * 466 + X] = c;
        }
    }
  }
}

// 在线：抱塞尔达骑行，举/挥两帧 + 悬浮 + 尾气 + 风线
// zelda40.1：场景带 146..324——上不碰大时钟（74..144），下不碰连接状态文字（328 起）
void drawZeldaOnline(unsigned long now) {
  const int w = Z_ON_RAISE_W;
  const int gx0 = CX - w * ZSPRITE_CELL / 2;
  // musicA.3：左右各留 16px，少擦 SEC_RING 3/9 点刻度；上沿仍避开大时钟。
  zbgRect(16, 146, 434, 178);
  float phase = (now % 2600) / 2600.0f;
  static const float wspd[4] = {1.15f, 0.85f, 1.35f, 0.65f};
  static const int wy[4] = {196, 232, 268, 302};
  static const int wln[4] = {46, 34, 52, 30};
  for (int k = 0; k < 4; k++) {                                          // 风线（精灵后面）
    float off = fmodf(phase * wspd[k] + k * 0.37f, 1.0f);
    int xh = (int)(-60 + off * (466 + 120));
    const uint16_t lc[3] = {0x07FF, 0x03EF, 0x0142};
    for (int s = 0; s < 3; s++) {
      int lx = xh - s * wln[k];
      int a0 = lx < 0 ? 0 : lx, a1 = lx + wln[k] > 466 ? 466 : lx + wln[k];
      if (a1 > a0) gfx->fillRect(a0, wy[k], a1 - a0, 2, lc[s]);
    }
  }
  bool slash = (now / Z_ONLINE_FRAME_MS) % 2;
  const uint16_t *rgb = slash ? Z_ON_SLASH_RGB : Z_ON_RAISE_RGB;
  const uint8_t *mk = slash ? Z_ON_SLASH_MASK : Z_ON_RAISE_MASK;
  int dy = (int)lroundf(3.0f * sinf(phase * 6.2831853f));
  zBlit(gx0, Z_ONLINE_Y0 + dy, w, Z_ON_RAISE_H, rgb, mk);
  int tx = gx0 + (int)(w * ZSPRITE_CELL * 0.90f);                        // 车尾尾气
  int ty = Z_ONLINE_Y0 + (int)(Z_ON_RAISE_H * ZSPRITE_CELL * 0.55f) + dy;
  for (int k = 0; k < 4; k++) {
    float life = fmodf(phase * 1.6f + k / 4.0f, 1.0f);
    int px = (int)(tx + life * 26), py = (int)(ty - life * 30);
    int rad = 2 + (int)(life * 5);
    uint16_t c = BootAnimation::zeldaScale(0xCE79, (uint32_t)((1.0f - life) * 160));
    aaDisc(px, py, rad * 3, c);
  }
}

// 离线：故障摩托（左）冒白烟 + 林克躺塞尔达怀里睡（右）+ ZZZ
// zelda40.1：2px 网格缩小（原 3px 太大，场景带擦到了时钟下部），场景带 150..325
void drawZeldaOffline(unsigned long now) {
  float phase = (now % 2000) / 2000.0f;
  const int cell = 2;
  const int bw = Z_OFF_BIKE_W * cell, dw = Z_OFF_DOZE_W * cell;
  const int total = bw + Z_OFF_GAP_PX + dw;
  const int x0 = CX - total / 2;
  const int bx0 = x0, by0 = Z_OFF_BIKE_BOTTOM - Z_OFF_BIKE_H * cell;     // 底边 318 不变
  const int gx0 = x0 + bw + Z_OFF_GAP_PX;
  const int gy0 = Z_OFF_BIKE_BOTTOM - Z_OFF_DOZE_H * cell + ((phase < 0.5f) ? 1 : 0);
  zbgRect(bx0 - 30, 150, total + 60, 175);                               // 场景区恢复（150..325，白烟/ZZZ 均在带内）
  zBlit(bx0, by0, Z_OFF_BIKE_W, Z_OFF_BIKE_H, Z_OFF_BIKE_RGB, Z_OFF_BIKE_MASK, cell);
  zBlit(gx0, gy0, Z_OFF_DOZE_W, Z_OFF_DOZE_H, Z_OFF_DOZE_RGB, Z_OFF_DOZE_MASK, cell);
  int sx = (int)(bx0 + bw * 0.45f), sy = (int)(by0 + Z_OFF_BIKE_H * cell * 0.30f);
  for (int k = 0; k < 6; k++) {                                          // 白烟（只画明亮段）
    float life = fmodf(phase * 1.4f + k / 6.0f, 1.0f);
    float a = (1.0f - life) * 0.9f;
    if (a < 0.30f) continue;
    int px = (int)(sx + sinf(life * 3.1f + k * 1.9f) * 9 - life * 7);
    int py = (int)(sy - life * 60);
    int rad = 2 + (int)(life * 5);
    aaDisc(px, py, rad * 3, BootAnimation::zeldaScale(0xFFFF, (uint32_t)(a * 255)));
  }
  int hx = gx0 + (int)(Z_ZZZ_GX * cell) - 2;                             // ZZZ 向左上漂
  int hy = gy0 + (int)(Z_ZZZ_GY * cell) - 20;
  for (int k = 0; k < 3; k++) {
    float life = fmodf(phase * 3.0f + k / 3.0f, 1.0f);
    int zx = (int)(hx + k * 8 - life * 16);
    int zy = (int)(hy - life * 52);
    printCentered("Z", zx, zy, 2, life < 0.5f ? C_CYAN : C_GREY);
  }
}

// ================= v7.13 待机时钟：手机连接后发 #TIME 自动对时 =================
// 手机在 BLE 连上时和每小时发一次 "#TIME,yyyy-MM-dd HH:mm:ss"（本地时间）；
// 圆屏用 millis() 走秒，跨天自动翻日期。没对过时就不显示时钟。
extern bool wxHourly, wxHave;
extern int wxIcon, wxTemp, wxN;
extern int wxFh[6], wxFl[6];
void drawWxIcon(int id, float cx, float cy, float s);       // 定义在 WeatherPage.h（后包含，前置声明）

int clkY = 2026, clkMo = 1, clkD = 1;      // 对时那一刻的日期
int clkDaySec = 0;                         // 对时那一刻的当天秒数
unsigned long clkSyncMs = 0;               // 对时时的 millis()；0 = 还没对过时

bool clkLeap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

// 当前时刻：y/m/d/h/m/s（v7.12.9-ui.4 秒针环需要秒）
void clkNow(int &Y, int &Mo, int &D, int &H, int &Mi, int &S) {
  static const int mdays[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
  unsigned long el = (clkSyncMs ? (millis() - clkSyncMs) / 1000 : 0) + clkDaySec;
  long day = el / 86400L;
  int sec = el % 86400L;
  Y = clkY; Mo = clkMo; D = clkD;
  while (day-- > 0) {
    D++;
    int dm = mdays[Mo - 1] + (Mo == 2 && clkLeap(Y) ? 1 : 0);
    if (D > dm) { D = 1; Mo++; if (Mo > 12) { Mo = 1; Y++; } }
  }
  H = sec / 3600; Mi = (sec % 3600) / 60; S = sec % 60;
}

// v7.12.9-ui.4 没对过时：灰色 --:-- 占位（大时钟同款 size 10 字距，每格 60px）
void drawClockPlaceholder() {
  const int x0 = CX - 150, top = 74, cell = 60, u = 10;   // 5 格，每格 6u 宽、字高 7u
  for (int k = 0; k < 5; k++) {
    if (k == 2) {                                          // 冒号：上下两个点
      gfx->fillRect(x0 + k * cell + 2 * u, top + 2 * u, u, u, C_GREY);
      gfx->fillRect(x0 + k * cell + 2 * u, top + 5 * u, u, u, C_GREY);
    } else {                                               // 横杠：中间一行
      gfx->fillRect(x0 + k * cell + u, top + 3 * u, 5 * u, u, C_GREY);
    }
  }
}

void drawIdleClock() {
  zbgRect(CX - 82, 42, 164, 20);                           // 日期行
  zbgRect(CX - 156, 64, 312, 84);                          // 大时钟
  if (!clkSyncMs) {                                        // v7.12.9-ui.4 未对时占位，不再空白
    drawClockPlaceholder();
    needFlush = true;
    return;
  }
  int Y, Mo, D, H, Mi, S;
  clkNow(Y, Mo, D, H, Mi, S);
  char db[14], tb[6];
  snprintf(db, sizeof(db), "%04d/%02d/%02d", Y, Mo, D);
  snprintf(tb, sizeof(tb), "%02d:%02d", H, Mi);
  printCentered(String(db), CX, 46, 2, Z_GOLD_SUB);
  printCentered(String(tb), CX, 74, 10, Z_GOLD);
  needFlush = true;                                        // v7.13.1 别依赖骑手动画兜底刷屏
}

// ================= v7.12.9-ui.4 外圈像素秒针刻度环 =================
// 60 格 × 6°，r=222（对齐固件进度环半径，距屏边 11px）：主刻度×4（12/3/6/9 点位）5px 亮灰、
// 小时刻度×12 4px 中灰、秒刻度×44 3px 暗灰；红色像素圆点每秒离散跳一格。
// 未对时（clkSyncMs==0）不画环——没有秒数据。与待机时钟同用 millis() 时间基准。
#define SEC_RING_R 222
#define C_RING_S   0x144B                                       // 秒刻度 暗灰 #4a4a58
#define C_RING_H   0x8C73                                       // 小时刻度 中灰 #8e8e9c
#define C_RING_Q   0xD69B                                       // 主刻度 亮灰 #d0d0da
#define C_RING_RED 0xFA27                                       // 秒针红点 #FF4438
uint8_t ringShownSec = 255;                                     // 当前红点位置；255 = 还没画

void ringXY(int deg, int &x, int &y) {
  float a = (deg - 90) * 0.01745329f;                           // 0 秒在 12 点位
  x = CX + (int)lroundf(SEC_RING_R * cosf(a));
  y = CX + (int)lroundf(SEC_RING_R * sinf(a));  // 圆屏 y 圆心 = CX（同 ProgressRings 约定）
}

void ringTickSize(int idx, int &sz, uint16_t &c) {
  if (idx % 15 == 0)      { sz = 5; c = C_RING_Q; }
  else if (idx % 5 == 0)  { sz = 4; c = C_RING_H; }
  else                    { sz = 3; c = C_RING_S; }
}

// 画/擦一个刻度位（erase=true 时把红点位置还原成刻度）
void ringDotAt(int idx, bool erase) {
  int x, y, sz; uint16_t c;
  ringXY(idx * 6, x, y); ringTickSize(idx, sz, c);
  if (erase) {
    zbgRect(x - 4, y - 4, 9, 9);                              // 塞尔达40周年特别版：恢复纹理而不是擦黑
    gfx->fillRect(x - sz / 2, y - sz / 2, sz, sz, c);         // 再还原该位刻度
    return;
  }
  gfx->fillRect(x - 4, y - 4, 9, 9, C_RING_RED);                // 9x9 四角削 1px 的像素圆
  gfx->fillRect(x - 4, y - 4, 1, 1, C_BLACK);
  gfx->fillRect(x + 4, y - 4, 1, 1, C_BLACK);
  gfx->fillRect(x - 4, y + 4, 1, 1, C_BLACK);
  gfx->fillRect(x + 4, y + 4, 1, 1, C_BLACK);
}

// 整环重画（页面重画/首次对时后调用）
void drawSecondRing(int sec) {
  for (int i = 0; i < 60; i++) {
    int x, y, sz; uint16_t c;
    ringXY(i * 6, x, y); ringTickSize(i, sz, c);
    gfx->fillRect(x - sz / 2, y - sz / 2, sz, sz, c);
  }
  ringDotAt(sec, false);
  ringShownSec = (uint8_t)sec;
  needFlush = true;
}

// 每秒推进：只擦旧点、画新点（省总线）
void tickSecondRing(int sec) {
  if (ringShownSec > 59 || sec == ringShownSec) return;
  ringDotAt(ringShownSec, true);
  ringDotAt(sec, false);
  ringShownSec = (uint8_t)sec;
  needFlush = true;
}

// 每 0.1 秒检查一次，样子变了才重画（函数名沿用 v3.6）
// musicA.2：先画塞尔达场景带，再画时钟 + 整圈秒刻度——场景 zbgRect(0,146,466,178)
// 会擦掉 3/9 点侧刻度；原先先 tick 后场景导致秒环「消失」、时钟偶发被盖住难读。
void animateCat() {
  if (!onWaitScreen || sliding) return;
  static unsigned long last = 0;
  unsigned long now = millis();
  if (!catForce && now - last < 100) return;
  bool force = catForce;
  catForce = false;
  last = now;
  int f, zph = -1;
  (void)f; (void)zph;                                        // 骑手逻辑由塞尔达场景替代
  if (waitSt == WAIT_READY) { drawZeldaOnline(now); needFlush = true; }
  else if (waitSt == WAIT_NOLINK) { drawZeldaOffline(now); needFlush = true; }
  else return;                                               // WAIT_LOST 静态（红环在 drawWaiting 画）
  // zelda40.1：连接状态文字每拍重绘——此前场景恢复带会周期性把 "ready"/"no link" 擦掉
  if (waitSt == WAIT_READY) printCentered("ready", CX, 328, 3, C_GREEN);
  else printCentered("no link", CX, 328, 3, C_GREY);
  // 时钟与秒环放在场景之后：每拍重画（对时后），避免场景带擦侧刻度 / 双缓冲丢字。
  if (clkSyncMs) {
    int Y, Mo, D, H, Mi, S;
    clkNow(Y, Mo, D, H, Mi, S);
    (void)Y; (void)Mo; (void)D; (void)H; (void)Mi;
    drawIdleClock();                                         // 每拍保证可读
    drawSecondRing(S);                                       // 每拍整环
  } else {
    drawIdleClock();                                         // --:-- 占位；对时后立刻有字
  }
  (void)force;
}

// 页面：导航左滑天气，右滑音乐；上划 G 值速度页，下划胎压页（v7.12.9-ui.6，原来上下滑都是天气）
int page = 0;                               // 0 = 导航 / 首页，1 = 天气，2 = 音乐，3 = G 值速度，4 = 胎压
unsigned long pageAt = 0;

// 页面顺序对应滑动位置：音乐在左，主页居中，天气在右；G/胎压在上下（页点行用左右位表示，导航主页不显示）。
void drawPageDots() {
  if (page == 0 && inNav) return;
  const int pageOrder[] = { (page == 3 || page == 4) ? 4 : 2, 0, (page == 3 || page == 4) ? 3 : 1 };
  for (int i = 0; i < 3; i++) {
    aaDisc(CX + (i - 1) * 20, 444, 4, page == pageOrder[i] ? C_WHITE : 0x4208);
  }
}

void drawWaiting(int st) {
  waitSt = st;
  if (sliding) return;                                                 // v5.5 正在滑动：滑完再画
  if (page == 2) { onWaitScreen = false; musicDirty = true; return; }
  if (page == 1) { onWaitScreen = false; wxDirty = true; return; }   // v5.3 正在看天气：先不画首页，滑回来时再画
  if (page == 3 || page == 4) { onWaitScreen = false; return; }      // v7.12.9-ui.6 G/胎压页由 gTpmsTick 重画
  zbgRect(0, 0, 466, 466);                                           // 塞尔达40周年特别版：纹理背景代替黑屏
  onWaitScreen = true; waitSt = st;          // v3.6 首页猫猫 → v4.6 像素骑手
  riderPrevF = -1; riderPrevZ = -2;
  catForce = true;
  drawIdleClock();                           // v7.13 日期 + 大像素时钟（手机自动对时）
  if (clkSyncMs) {                           // v7.12.9-ui.4 整环重画（页面被清屏过）
    int Y, Mo, D, H, Mi, S;
    clkNow(Y, Mo, D, H, Mi, S);
    drawSecondRing(S);
  }
  animateCat();
  if (st == WAIT_READY) {
    printCentered("ready", CX, 328, 3, C_GREEN);
  } else if (st == WAIT_NOLINK) {
    printCentered("no link", CX, 328, 3, C_GREY);
  } else {
    drawRedRing();
    printCentered("LINK LOST", CX, 328, 4, C_RED);
  }
  if (st == WAIT_READY && (wxHave || imgs[3].showW)) {     // v7.13.1 待机天气：中文地点天气图 + 图标温度 + 时段高低温
    drawTextImgC(3, 352, 0xFF, Z_GOLD);                    // zelda40.2 "地点 · 天气现象" 中文位图改金色（原来是白字）
    String t = String(wxTemp);
    int tw = t.length() * 24;                              // size 4 每字 24 像素宽
    int icw = 28, gap = 10;                                // 图标直径、图标和温度的间距
    int x0 = CX - (icw + gap + tw + 8) / 2;                // +8 = 温度右上角的小度数圈
    drawWxIcon(wxIcon, x0 + icw / 2, 400, 14);
    printCentered(t, x0 + icw + gap + tw / 2, 390, 4, Z_GOLD);
    aaDisc(x0 + icw + gap + tw + 3, 395, 4, Z_GOLD);       // 温度右上角的小度数圈
    String hl;                                             // 第三行：逐小时数据里算时段高低温；按天（旧版协议）直接用 HI/LO
    if (wxHourly) {
      if (wxN > 0) {
        int hi = wxFh[0], lo = wxFh[0];
        for (int i = 1; i < wxN; i++) { if (wxFh[i] > hi) hi = wxFh[i]; if (wxFh[i] < lo) lo = wxFh[i]; }
        if (hi > lo) hl = String("H") + hi + "  L" + lo;
      }
    } else if (wxN > 0) {
      hl = String("HI ") + wxFh[0] + "  LO " + wxFl[0];
    }
    if (hl.length()) printCentered(hl, CX, 424, 2, Z_GOLD_SUB);
  }
  drawStatus();
  drawPageDots();
  needFlush = true;
}
