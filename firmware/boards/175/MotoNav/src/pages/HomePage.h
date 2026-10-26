#pragma once
// 首页骑手动画和等待画面。由 MotoNav.ino 按依赖顺序包含，仅编译一次。
// ================= 整屏画面 =================

#define WAIT_READY  0
#define WAIT_NOLINK 1
#define WAIT_LOST   2

// ================= v4.6 首页像素骑手（代替 v3.6 的猫猫） =================
// 原创的像素女骑手 + 小摩托：连上手机时围巾和马尾随风飘、偶尔眨眼、每隔几秒挥挥手；
// 没连上时摩托车灯熄灭、她闭眼打瞌睡，头上飘 z
// v4.6 首页像素骑手（原创角色）：40×26，每格放大 RIDER_S 倍；由 rider2.py 生成
#include "../../resources/RiderSprites.h"
uint16_t riderColor(char ch) {
  switch (ch) {
    case 'K': return 0x18C6;
    case 'H': return 0x2E57;
    case 'h': return 0xAFBD;
    case 'V': return 0x29ED;
    case 'v': return 0x7D1C;
    case 'S': return 0xFEB5;
    case 'E': return 0x18C6;
    case 'B': return 0xFCB2;
    case 'R': return 0x9225;
    case 'J': return 0xFC65;
    case 'j': return 0xD2E2;
    case 'F': return 0xFF07;
    case 'G': return 0x39E8;
    case 'P': return 0x3A91;
    case 'p': return 0x29AC;
    case 'b': return 0x6A25;
    case 'W': return 0xFFFF;
    case 'M': return 0xE187;
    case 'm': return 0xA0E5;
    case 'C': return 0xCE7A;
    case 'c': return 0x8432;
    case 'T': return 0x2966;
    case 't': return 0x94B4;
    case 'L': return 0xFF8F;
    case 'l': return 0x6B6C;
    case 's': return 0x4185;
    default: return 0;
  }
}

#define RIDER_S  6
#define RIDER_X0 (CX - RIDER_W * RIDER_S / 2)
#define RIDER_Y0 160
bool onWaitScreen = false;
bool sliding = false;                       // v5.5 正在拖动/滑动换页：别的画面先别画
int waitSt = 0;
int riderPrevF = -1, riderPrevZ = -2;
bool catForce = false;                      // 首页刚重画：马上画一次

// f = 第几帧；zph = 睡觉时 z 的动画帧（-1 = 不睡）
void drawRider(int f, int zph) {
  gfx->fillRect(RIDER_X0 - 4, RIDER_Y0 - 8, RIDER_W * RIDER_S + 8, RIDER_H * RIDER_S + 14, C_BLACK);   // v7.13 上边不擦到时钟
  drawStatus();                             // v7.2 上面擦黑的那块盖住了蓝牙点和电量：马上补画（z 字母画在它上面）
  for (int y = 0; y < RIDER_H; y++) {
    const char *row = RIDER[f][y];
    for (int x = 0; row[x] && x < RIDER_W; x++) {
      uint16_t c = riderColor(row[x]);
      if (c || row[x] == 'K') gfx->fillRect(RIDER_X0 + x * RIDER_S, RIDER_Y0 + y * RIDER_S, RIDER_S, RIDER_S, c ? c : 0x18C6);
    }
  }
  if (zph >= 0) {                           // 三个 z 从她头边往右上飘（v7.13 改往侧上，别飘进时钟区）
    for (int k = 0; k <= zph; k++)
      printCentered("z", CX + 58 + k * 14, RIDER_Y0 + 10 - k * 7, 2 + (k > 0 ? 1 : 0), k == zph ? C_CYAN : C_GREY);
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
  gfx->fillRect(CX - 82, 42, 164, 20, C_BLACK);            // 日期行
  gfx->fillRect(CX - 156, 64, 312, 84, C_BLACK);           // 大时钟
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
  printCentered(String(db), CX, 46, 2, C_GREY);
  printCentered(String(tb), CX, 74, 10, C_WHITE);
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
    gfx->fillRect(x - 4, y - 4, 9, 9, C_BLACK);               // 先整块抹掉 9x9 红点（含削角黑像素）
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
void animateCat() {
  if (!onWaitScreen || sliding) return;
  static unsigned long last = 0;
  unsigned long now = millis();
  if (!catForce && now - last < 100) return;
  catForce = false;
  last = now;
  static int clkShownMin = -1, clkShownDay = -1;          // v7.13 每分钟/跨天刷新待机时钟
  if (clkSyncMs) {
    int Y, Mo, D, H, Mi, S;
    clkNow(Y, Mo, D, H, Mi, S);
    if (Mi != clkShownMin || D != clkShownDay) { clkShownMin = Mi; clkShownDay = D; drawIdleClock(); }
    if (ringShownSec > 59) drawSecondRing(S);             // v7.12.9-ui.4 首次对时后补画整环
    else tickSecondRing(S);                               // 每秒推进红点
  }
  int f, zph = -1;
  int t = (int)((now / 400) % 2);           // 围巾、马尾飘动
  if (waitSt == WAIT_NOLINK) {
    f = 6;                                  // 睡觉
    zph = (int)((now / 700) % 3);
  } else if (waitSt == WAIT_READY && (now % 5000) < 1500) {
    f = 4 + (int)((now / 250) % 2);         // 挥手
  } else if ((now % 3600) < 160) {
    f = 2 + t;                              // 眨眼
  } else {
    f = t;
  }
  if (f == riderPrevF && zph == riderPrevZ) return;
  riderPrevF = f; riderPrevZ = zph;
  drawRider(f, zph);
  needFlush = true;
}

// 页面：导航左滑天气，右滑音乐，上下滑也可查看天气
int page = 0;                               // 0 = 导航 / 首页，1 = 天气，2 = 音乐
unsigned long pageAt = 0;

// 页面顺序对应滑动位置：音乐在左，主页居中，天气在右；导航主页不显示。
void drawPageDots() {
  if (page == 0 && inNav) return;
  const int pageOrder[] = {2, 0, 1};
  for (int i = 0; i < 3; i++) {
    aaDisc(CX + (i - 1) * 20, 444, 4, page == pageOrder[i] ? C_WHITE : 0x4208);
  }
}

void drawWaiting(int st) {
  waitSt = st;
  if (sliding) return;                                                 // v5.5 正在滑动：滑完再画
  if (page == 2) { onWaitScreen = false; musicDirty = true; return; }
  if (page == 1) { onWaitScreen = false; wxDirty = true; return; }   // v5.3 正在看天气：先不画首页，滑回来时再画
  gfx->fillScreen(C_BLACK);
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
    drawTextImg(3, 352, 0xFF);                             // "地点 · 天气现象" 中文位图（手机渲染好推来的），宽度自动缩放进圆
    String t = String(wxTemp);
    int tw = t.length() * 24;                              // size 4 每字 24 像素宽
    int icw = 28, gap = 10;                                // 图标直径、图标和温度的间距
    int x0 = CX - (icw + gap + tw + 8) / 2;                // +8 = 温度右上角的小度数圈
    drawWxIcon(wxIcon, x0 + icw / 2, 400, 14);
    printCentered(t, x0 + icw + gap + tw / 2, 390, 4, C_WHITE);
    aaDisc(x0 + icw + gap + tw + 3, 395, 4, C_WHITE);      // 温度右上角的小度数圈
    aaDisc(x0 + icw + gap + tw + 3, 395, 2, C_BLACK);
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
    if (hl.length()) printCentered(hl, CX, 424, 2, C_GREY);
  }
  drawStatus();
  drawPageDots();
  needFlush = true;
}
