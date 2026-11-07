#pragma once
// ================= musicA.37 上方槽 / 中央槽 / 9 种时间样式 / 全屏指针表盘 =================
// 本文件在 HomePage.h 之后包含：能直接看到 clkNow / ringXY / zbgRect / liveG / drawWxIcon 等。
// 几何按实机 466×466：上方槽 y36–134，中央槽 BAND_Y0=136 / BAND_H=224，秒环 r=222。
// 秒针刻度与红点已分离：drawRingTicks() 由 hasTimeFace() 门控，ringDotAt() 由 dotOn() 门控。
extern bool nightOn;
#include <string.h>                              // strlen

struct CsPal {
  uint16_t bg, ink, sub, dim, gold, red, cyan, line, panel;
};
static CsPal csPal() {
  // musicA.40：待机表盘始终深色（取消白天浅底）
  (void)nightOn;
  return {0x0000, 0xFFFF, 0xD69B, 0x6B4D, 0xFE27, 0xF800, 0x07FF, 0x2945, 0x18C6};
}

// ---------- 上方槽：0 时间 / 1..4 兼容旧功能 / 5 隐藏（musicA.40 App 只发 0/5） ----------
void drawTopSlot() {
  // musicA.40 P0：擦除起点抬到 y42，避开加大后的电量底边（约 y20..38）
  zbgRect(0, 42, 466, 94);
  if (topFunc == 5 || topFunc > 5) { needFlush = true; return; }  // 隐藏
  if (topFunc == 0) { drawIdleClock(); return; }     // 时间：顶栏小日期 + size10 大时钟
  uint16_t c   = slotColor(Z_GOLD, slotTimeColor);
  uint16_t sub = slotColor(Z_GOLD_SUB, slotTimeColor);
  if (!clkSyncMs) { printCentered("--", CX, 62, 8, sub); return; }
  int Y, Mo, D, H, Mi, S;
  clkNow(Y, Mo, D, H, Mi, S);
  char db[16]; snprintf(db, sizeof(db), "%04d/%02d/%02d", Y, Mo, D);
  printCentered(String(db), CX, 44, 2, sub);         // 略下移，避开抬高后的电量区
  switch (topFunc) {
    case 1:                                          // 速度（兼容旧协议）
      printCentered(String(speedKmh >= 0 ? speedKmh : 0), CX, 58, 8, c);
      printCentered("km/h", CX, 122, 2, sub);
      break;
    case 2:
      printCentered("LIVE G", CX, 50, 2, sub);
      printCentered(String(liveG(), 2), CX, 66, 8, c);
      break;
    case 3: {
      char l[24];
      if (tpValid) snprintf(l, sizeof(l), "F %d / R %d", tpFrontKpa, tpRearKpa);
      else snprintf(l, sizeof(l), "F -- / R --");
      printCentered(String(l), CX, 68, 4, c);
      printCentered("kPa", CX, 114, 2, sub);
      break;
    }
    case 4:
      if (wxHave) {
        printCentered(String(wxTemp), CX, 58, 8, c);
        drawWxIcon(wxIcon, CX, 126, 10);
      } else {
        printCentered("--", CX, 68, 8, sub);
      }
      break;
    default: break;
  }
  needFlush = true;
}

// ---------- 60 格刻度环（与红点分离；hasTimeFace 门控） ----------
void drawRingTicks() {
  if (!hasTimeFace()) {
    // 从「有时间」切到「两槽都不是时间」时，上下边缘的刻度不在场景带擦除范围内，
    // 必须主动逐格恢复背景，否则会留一圈灰点。
    for (int i = 0; i < 60; i++) { int x, y; ringXY(i * 6, x, y); zbgRect(x - 4, y - 4, 9, 9); }
    ringShownSec = 255;
    return;
  }
  for (int i = 0; i < 60; i++) {
    int x, y, sz; uint16_t c;
    ringXY(i * 6, x, y); ringTickSize(i, sz, c);
    zbgRect(x - 4, y - 4, 9, 9);                  // 先清掉 9×9 红点残影，再画灰刻度
    gfx->fillRect(x - sz / 2, y - sz / 2, sz, sz, c);
  }
  needFlush = true;
}

// ---------- 七段数字（G-Shock）/ 5×7 点阵（LED） ----------
static const uint8_t csSeg[10] = {0x3F,0x06,0x5B,0x4F,0x66,0x6D,0x7D,0x07,0x7F,0x6F};
static void csSegDigit(int x, int y, int w, int h, int n, uint16_t on, uint16_t off) {
  const int th = w / 6 + 1;
  uint8_t m = csSeg[n % 10];
  gfx->fillRect(x + th, y,           w - 2 * th, th, (m & 0x01) ? on : off);   // a
  gfx->fillRect(x + th, y + h/2 - th/2, w - 2*th, th, (m & 0x40) ? on : off); // g
  gfx->fillRect(x + th, y + h - th,  w - 2 * th, th, (m & 0x08) ? on : off);   // d
  gfx->fillRect(x,         y + th,           th, h/2 - th, (m & 0x20) ? on : off); // f
  gfx->fillRect(x + w - th,y + th,           th, h/2 - th, (m & 0x02) ? on : off); // b
  gfx->fillRect(x,         y + h/2 + th/2,   th, h/2 - th, (m & 0x10) ? on : off); // e
  gfx->fillRect(x + w - th,y + h/2 + th/2,   th, h/2 - th, (m & 0x04) ? on : off); // c
}
static const uint8_t csFont[11][5] = {
  {0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},{0x42,0x61,0x51,0x49,0x46},
  {0x21,0x41,0x45,0x4B,0x31},{0x18,0x14,0x12,0x7F,0x10},{0x27,0x45,0x45,0x45,0x39},
  {0x3C,0x4A,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},{0x36,0x49,0x49,0x49,0x36},
  {0x06,0x49,0x49,0x29,0x1E},{0x00,0x36,0x36,0x00,0x00}                    // ':'
};
static void csDotText(const char *s, int cx, int y, int px, uint16_t c) {
  int n = (int)strlen(s);
  int x0 = cx - n * 6 * px / 2;
  for (int i = 0; i < n; i++) {
    int idx = (s[i] >= '0' && s[i] <= '9') ? s[i] - '0' : (s[i] == ':' ? 10 : -1);
    if (idx < 0) continue;
    for (int col = 0; col < 5; col++)
      for (int row = 0; row < 7; row++)
        if (csFont[idx][col] & (1 << row))
          gfx->fillRect(x0 + i * 6 * px + col * px, y + row * px, px, px, c);
  }
}

// ---------- 模拟指针表盘（中央槽 4 样式 / 全屏 4 样式共用） ----------
// variant：0 石英三针 1 包豪斯几何 2 月相机械 3 数字指针混合
// withHands=false：只画面盘（供全屏 B 先画黑圆再叠指针）
static void csAnalogHands(int cx, int cy, int r, int H, int Mi, int S,
                          const CsPal &p, bool full) {
  float ah = ((H % 12) + Mi / 60.0f) * 0.52359878f - 1.5707963f;
  float am = (Mi + S / 60.0f) * 0.10471976f - 1.5707963f;
  float as = S * 0.10471976f - 1.5707963f;
  int hhx = cx + (int)(r * 0.52f * cosf(ah)), hhy = cy + (int)(r * 0.52f * sinf(ah));
  int mmx = cx + (int)(r * 0.78f * cosf(am)), mmy = cy + (int)(r * 0.78f * sinf(am));
  int ssx = cx + (int)(r * 0.90f * cosf(as)), ssy = cy + (int)(r * 0.90f * sinf(as));
  thickLine(cx, cy, hhx, hhy, full ? 10 : 6, p.ink);
  thickLine(cx, cy, mmx, mmy, full ? 7 : 4, p.ink);
  thickLine(cx, cy, ssx, ssy, 2, p.red);
  float dx = hhx - cx, dy = hhy - cy, len = sqrtf(dx * dx + dy * dy);
  if (len > 24) {
    float ux = dx / len, uy = dy / len, bx = hhx - ux * (full ? 20 : 12), by = hhy - uy * (full ? 20 : 12);
    gfx->fillTriangle(hhx, hhy,
                      bx - uy * (full ? 6 : 4), by + ux * (full ? 6 : 4),
                      bx + uy * (full ? 6 : 4), by - ux * (full ? 6 : 4), p.ink);
  }
  aaDisc(cx, cy, full ? 8 : 5, p.ink);
  aaDisc(cx, cy, full ? 3 : 2, p.red);
}

static void csAnalog(int cx, int cy, int r, int H, int Mi, int S, int D,
                     const CsPal &p, int variant, bool full, bool withHands = true) {
  aaDisc(cx, cy, r + 6, p.line);
  aaDisc(cx, cy, r + 3, p.bg);
  for (int k = 0; k < 60; k++) {
    float a = (k * 6 - 90) * 0.01745329f;
    int r0 = r - ((k % 5 == 0) ? (full ? 20 : 12) : (full ? 11 : 7)), r1 = r - 3;
    gfx->drawLine(cx + (int)(r0 * cosf(a)), cy + (int)(r0 * sinf(a)),
                  cx + (int)(r1 * cosf(a)), cy + (int)(r1 * sinf(a)),
                  (k % 5 == 0) ? p.sub : p.dim);
  }
  if (variant == 1) {
    gfx->fillRect(cx - r / 3, cy - r / 2, r / 3, 5, p.red);
    gfx->fillRect(cx + 2, cy + r / 2 - 5, r / 3, 5, p.cyan);
    aaDisc(cx, cy, r / 3, p.gold); aaDisc(cx, cy, r / 3 - 4, p.bg);
  } else if (variant == 2) {                        // 月相机械：罗马时标 + 可见月相图
    static const char *rom[4] = {"XII", "III", "VI", "IX"};
    for (int k = 0; k < 4; k++) {
      float a = (k * 90 - 90) * 0.01745329f;
      int tx = cx + (int)((r - (full ? 40 : 24)) * cosf(a));
      int ty = cy + (int)((r - (full ? 40 : 24)) * sinf(a));
      if (k == 2 && full) continue;                 // 全屏数字改到轴心上方，6 点仍可留空
      printCentered(String(rom[k]), tx, ty - 8, full ? 2 : 1, p.gold);
    }
    // musicA.40：月相窗加大并画清盈亏（示意半月），确保设置/圆屏都看得见月相图
    int mw = full ? 62 : 38, my = cy + (full ? 62 : 40);
    aaDisc(cx, my, mw / 2 + 3, p.gold);             // 金边孔径
    aaDisc(cx, my, mw / 2 + 1, 0x4208);
    aaDisc(cx, my, mw / 2, 0xF7BE);                 // 月面暖白
    aaDisc(cx + mw / 4, my - mw / 16, mw / 2 - 1, p.bg);  // 暗面 terminator
    aaDisc(cx - mw / 6, my - mw / 8, mw / 10, 0xE71C);    // 环形山示意
    aaDisc(cx + mw / 10, my + mw / 7, mw / 14, 0xDEFB);
  } else if (variant == 0) {
    int wx = cx + r - (full ? 56 : 34), wy = cy - (full ? 17 : 12);
    int ww = full ? 44 : 30, wh = full ? 34 : 24;
    gfx->fillRect(wx, wy, ww, wh, p.panel);
    gfx->fillRect(wx, wy, ww, 1, p.line);
    gfx->fillRect(wx, wy + wh - 1, ww, 1, p.line);
    gfx->fillRect(wx, wy, 1, wh, p.line);
    gfx->fillRect(wx + ww - 1, wy, 1, wh, p.line);
    char dd[4]; snprintf(dd, sizeof(dd), "%02d", D);
    printCentered(String(dd), wx + ww / 2, wy + (full ? 9 : 5), full ? 3 : 2, p.ink);
  } else if (!full) {                               // 数字指针混合（中央槽）：上方青色数字；全屏改轴心上方另画
    char t[6]; snprintf(t, sizeof(t), "%02d:%02d", H, Mi);
    printCentered(String(t), cx, cy - r / 2 - 18, 2, p.cyan);
  }
  if (withHands) csAnalogHands(cx, cy, r, H, Mi, S, p, full);
}

// ---------- 中央槽 = 时间：9 种样式；musicA.40 收进安全区避开上/左/右槽 ----------
void drawCenterClock() {
  zbgRect(0, BAND_Y0, 466, BAND_H);
  CsPal p = csPal();
  // 安全区：水平远离 SLOT_LX/RX（84/382），竖直避开 SLOT_Y=148 与上方槽
  const int cy = 268, ar = 72;
  if (!clkSyncMs) {
    printCentered("--:--", CX, 220, 7, p.sub);
    printCentered("等待手机对时", CX, 290, 2, p.dim);
    needFlush = true;
    return;
  }
  int Y, Mo, D, H, Mi, S;
  clkNow(Y, Mo, D, H, Mi, S);
  char t[8], db[24];
  snprintf(t, sizeof(t), "%02d:%02d", H, Mi);
  snprintf(db, sizeof(db), "%04d/%02d/%02d", Y, Mo, D);
  switch (clockStyle) {
    case 0:
      printCentered(String(t), CX, 210, 7, p.gold);
      printCentered(String(db), CX, 292, 2, p.sub);
      break;
    case 1:
      printCentered(String(t).substring(0, 2), CX, 168, 9, p.ink);
      gfx->fillRect(CX - 72, 246, 144, 2, p.line);
      printCentered(String(t).substring(3, 5), CX, 254, 9, p.ink);
      printCentered(String(db), CX, 332, 2, p.sub);
      break;
    case 2: {
      uint16_t on = 0x07E0, off = 0x0285;
      gfx->fillRect(CX - 130, 172, 260, 120, p.panel);
      gfx->fillRect(CX - 130, 172, 260, 1, p.line);
      gfx->fillRect(CX - 130, 291, 260, 1, p.line);
      gfx->fillRect(CX - 130, 172, 1, 120, p.line);
      gfx->fillRect(CX + 129, 172, 1, 120, p.line);
      const int W = 46, Hh = 76, gap = 8;
      int x0 = CX - (4 * W + 3 * gap) / 2;
      int digs[4] = {H / 10, H % 10, Mi / 10, Mi % 10};
      for (int i = 0; i < 4; i++) csSegDigit(x0 + i * (W + gap) + (i >= 2 ? gap : 0), 188, W, Hh, digs[i], on, off);
      gfx->fillRect(CX - 4, 208, 8, 8, on); gfx->fillRect(CX - 4, 232, 8, 8, on);
      printCentered(String(db) + "  " + String(S / 10) + String(S % 10), CX, 308, 2, p.sub);
      break;
    }
    case 3:
      gfx->fillRect(CX - 40, 168, 4, 170, p.gold);
      printCentered(String(t).substring(0, 2), CX + 20, 172, 8, p.ink);
      printCentered(String(t).substring(3, 5), CX + 20, 256, 8, p.cyan);
      printCentered(String(db), CX, 334, 2, p.sub);
      break;
    case 4:
      csDotText(t, CX, 210, 6, 0xFD20);
      csDotText(db, CX, 286, 3, p.dim);
      break;
    case 5:
      csAnalog(CX, cy, ar, H, Mi, S, D, p, 0, false);
      printCentered(String(t), CX, 340, 2, p.sub);
      break;
    case 6:
      csAnalog(CX, cy, ar, H, Mi, S, D, p, 1, false);
      printCentered(String(t) + "  " + String(db), CX, 340, 2, p.sub);
      break;
    case 7:                                        // 月相机械（含月相图）
      csAnalog(CX, cy, ar, H, Mi, S, D, p, 2, false);
      printCentered(String(db), CX, 342, 2, p.sub);
      break;
    case 8:
      printCentered(String(t), CX, 176, 6, p.cyan);
      csAnalog(CX, 286, 60, H, Mi, S, D, p, 3, false);
      printCentered(String(db), CX, 342, 2, p.sub);
      break;
    default: break;
  }
  needFlush = true;
}

// ---------- 全屏指针表盘（中央槽第 3 项） ----------
// A 纯净：隐上下左右功能槽，只留电量 + 指针；数字时间+日期在轴心上方抬高。
// B 外环：近中心四黑圆（速度/实时G/胎压/天气），指针叠在黑圆之上。
void drawFullAnalog() {
  zbgRect(0, 0, 466, 466);
  CsPal p = csPal();
  drawRingTicks();
  if (!clkSyncMs) {
    printCentered("--:--", CX, 210, 8, p.sub);
    printCentered("等待手机对时", CX, 288, 2, p.dim);
  } else {
    int Y, Mo, D, H, Mi, S;
    clkNow(Y, Mo, D, H, Mi, S);
    char t[8], db[24];
    snprintf(t, sizeof(t), "%02d:%02d", H, Mi);
    snprintf(db, sizeof(db), "%04d/%02d/%02d", Y, Mo, D);
    int variant = (clockStyle >= 5 && clockStyle <= 8) ? clockStyle - 5 : 2;
    int r = (fullFaceAB == 1) ? 172 : 190;
    // 先画面盘（不画指针），B 模式插入黑圆后再叠指针
    csAnalog(CX, CX, r, H, Mi, S, D, p, variant, true, false);
    if (fullFaceAB == 1) {
      const int ang[4] = {-45, 45, 135, 225};
      const char *lab[4] = {"速度", "实时G", "胎压", "天气"};
      char val[4][16];
      snprintf(val[0], sizeof(val[0]), "%d", speedKmh >= 0 ? speedKmh : 0);
      snprintf(val[1], sizeof(val[1]), "%.2f", liveG());
      if (tpValid) snprintf(val[2], sizeof(val[2]), "%d", tpFrontKpa);
      else snprintf(val[2], sizeof(val[2]), "--");
      if (wxHave) snprintf(val[3], sizeof(val[3]), "%d", wxTemp);
      else snprintf(val[3], sizeof(val[3]), "--");
      for (int k = 0; k < 4; k++) {
        float a = ang[k] * 0.01745329f;
        // 更靠中心的黑圆（原外环矩形 r≈168 → 黑圆 r≈108）
        int bx = CX + (int)(108 * cosf(a)), by = CX + (int)(108 * sinf(a));
        aaDisc(bx, by, 28, C_BLACK);
        aaDisc(bx, by, 27, 0x10A2);
        printCentered(String(lab[k]), bx, by - 12, 1, p.dim);
        printCentered(String(val[k]), bx, by - 1, 2, p.ink);
      }
    }
    csAnalogHands(CX, CX, r, H, Mi, S, p, true);
    // musicA.40：数字时间+日期在轴心正上方且抬高（贴近电量下沿）
    printCentered(String(db), CX, 72, 2, p.dim);
    printCentered(String(t), CX, 92, 4, p.sub);
  }
  drawStatus();
  drawPageDots();
  needFlush = true;
}
void tickFullFace() { drawFullAnalog(); }
