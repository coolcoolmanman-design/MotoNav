#pragma once
// ================= musicA.37 上方槽 / 中央槽 / 9 种时间样式 / 全屏指针表盘 =================
// musicA.41：全屏无数字日期 / 无外圈秒刻度 / 真全屏指针 / 轴心上方实心电量；
//           中央=时间隐上/左上/右上并放大；内嵌小指针盘禁止数字时间/日期。
// musicA.42：中央=时间上移到真圆心并再放大（圆边裁切）；B 外环与 A 同真全屏半径；四功能黑圆略放大。
// musicA.43：中央=时间跳过事后 wipe；全屏 B 四功能圈半径 33→56（radial 保持 118，不外移）。
// musicA.44：B 圈 radial 118→133（小幅外移 +15）；标签改 ASCII（SPD/LIVE G/TPMS/WX）修乱码。
// musicA.45：B 圈内标签+数字放大；fit-to-circle 保证不溢出黑圆（circleR 56 / radial 133 不变）。
// musicA.58：fullFaceAB=2 → C Infograph（烘焙底图 + 四弧轨内彩填 + N/E/S/W 子盘字 + 诚实扁针）。
// 本文件在 HomePage.h 之后包含：能直接看到 clkNow / ringXY / zbgRect / liveG / drawWxIcon 等。
// 几何按实机 466×466：上方槽 y36–134，中央槽 BAND_Y0=136 / BAND_H=224，秒环 r=222。
// 秒针刻度与红点已分离：drawRingTicks() 由 hasTimeFace() 门控，ringDotAt() 由 dotOn() 门控。
extern bool nightOn;
extern bool wxHourly;
extern int wxN, wxAQI;
extern int wxFh[6], wxFl[6];
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
// ---------- fix4：参考图日期 "Tue, Nov 5"（Zeller 求星期，协议零改动） ----------
static const char *csWdName[7] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
static const char *csMoName[12] = {"Jan","Feb","Mar","Apr","May","Jun",
                                   "Jul","Aug","Sep","Oct","Nov","Dec"};
static int csWeekday(int y, int m, int d) {
  if (m < 3) { m += 12; y--; }
  int h = (d + (13 * (m + 1)) / 5 + y + y / 4 - y / 100 + y / 400) % 7;
  if (h < 0) h += 7;                     // Zeller: 0=Sat 1=Sun 2=Mon ...
  static const int mp[7] = {6,0,1,2,3,4,5};
  return mp[h];
}
static void csDateText(char *out, size_t cap, int y, int m, int d) {
  if (m < 1 || m > 12) m = 1;
  snprintf(out, cap, "%s, %s %d", csWdName[csWeekday(y,m,d)], csMoName[m-1], d);
}
// 立体描边字：8 邻域暗描边 + 亮芯，模拟参考图金属立体字（实机深色底）
static void csEmboss(const String &txt, int cx, int y, int sz, uint16_t core, uint16_t edge) {
  for (int dy = -2; dy <= 2; dy += 2)
    for (int dx = -2; dx <= 2; dx += 2)
      if (dx || dy) printCentered(txt, cx + dx, y + dy, sz, edge);
  printCentered(txt, cx, y, sz, core);
}
// 斜体点阵：逐行 x 偏移，做出参考图的倾斜粗体
static void csDotItalic(const char *s, int cx, int y, int px, uint16_t c) {
  int n = (int)strlen(s);
  int x0 = cx - n * 6 * px / 2;
  for (int i = 0; i < n; i++) {
    int idx = (s[i] >= '0' && s[i] <= '9') ? s[i] - '0' : (s[i] == ':' ? 10 : -1);
    if (idx < 0) continue;
    for (int col = 0; col < 5; col++)
      for (int row = 0; row < 7; row++)
        if (csFont[idx][col] & (1 << row))
          gfx->fillRect(x0 + i * 6 * px + col * px + (6 - row) * px / 5, y + row * px,
                        px + 1, px, c);
  }
}

// variant：0 石英三针 1 包豪斯几何 2 月相机械 3 数字指针混合
// withHands=false：只画面盘（供全屏 B 先画黑圆再叠指针）
static void csAnalogHands(int cx, int cy, int r, int H, int Mi, int S,
                          const CsPal &p, bool full, int variant = 0) {
  float ah = ((H % 12) + Mi / 60.0f) * 0.52359878f - 1.5707963f;
  float am = (Mi + S / 60.0f) * 0.10471976f - 1.5707963f;
  float as = S * 0.10471976f - 1.5707963f;
  // musicA.41：全屏真全屏近表圈（原 0.52/0.78/0.90）
  float hr = full ? 0.58f : 0.52f;
  float mr = full ? 0.86f : 0.78f;
  float sr = full ? 0.97f : 0.90f;
  int hhx = cx + (int)(r * hr * cosf(ah)), hhy = cy + (int)(r * hr * sinf(ah));
  int mmx = cx + (int)(r * mr * cosf(am)), mmy = cy + (int)(r * mr * sinf(am));
  int ssx = cx + (int)(r * sr * cosf(as)), ssy = cy + (int)(r * sr * sinf(as));
  thickLine(cx, cy, hhx, hhy, full ? 10 : 6, p.ink);
  thickLine(cx, cy, mmx, mmy, full ? 7 : 4, p.ink);
  // fix4：0 石英细刻度+红秒针 / 1 运动粗胶囊+蓝秒针 / 2 罗马时标+暖白秒针
  uint16_t secCol = (variant == 0) ? p.red : ((variant == 1) ? p.cyan : p.gold);
  thickLine(cx, cy, ssx, ssy, (variant == 1) ? 3 : 2, secCol);
  float dx = hhx - cx, dy = hhy - cy, len = sqrtf(dx * dx + dy * dy);
  if (len > 24) {
    float ux = dx / len, uy = dy / len, bx = hhx - ux * (full ? 20 : 12), by = hhy - uy * (full ? 20 : 12);
    gfx->fillTriangle(hhx, hhy,
                      bx - uy * (full ? 6 : 4), by + ux * (full ? 6 : 4),
                      bx + uy * (full ? 6 : 4), by - ux * (full ? 6 : 4), p.ink);
  }
  aaDisc(cx, cy, full ? 8 : 5, p.ink);
  aaDisc(cx, cy, full ? 3 : 2, secCol);
}

static void csAnalog(int cx, int cy, int r, int H, int Mi, int S, int D,
                     const CsPal &p, int variant, bool full, bool withHands = true) {
  (void)D;
  aaDisc(cx, cy, r + 6, p.line);
  aaDisc(cx, cy, r + 3, p.bg);
  if (variant == 1) {
    // 运动三针：12 根粗胶囊时标 + 蓝秒针
    for (int k = 0; k < 12; k++) {
      float a = (k * 30 - 90) * 0.01745329f;
      int r0 = r - (full ? 30 : 16), r1 = r - 8;
      thickLine(cx + (int)(r0 * cosf(a)), cy + (int)(r0 * sinf(a)),
                cx + (int)(r1 * cosf(a)), cy + (int)(r1 * sinf(a)),
                (k % 3 == 0) ? 9 : 6, (k % 3 == 0) ? p.ink : p.sub);
    }
  } else if (variant == 2) {
    // 罗马时标 XII / III / VI / IX（B 外环模式下日期会被黑圆压住，故不画盘内日期）
    static const char *rom[4] = {"XII", "III", "VI", "IX"};
    for (int k = 0; k < 4; k++) {
      float a = (k * 90 - 90) * 0.01745329f;
      int tx = cx + (int)((r - (full ? 44 : 24)) * cosf(a));
      int ty = cy + (int)((r - (full ? 44 : 24)) * sinf(a));
      printCentered(String(rom[k]), tx, ty - 10, full ? 3 : 2, p.ink);
    }
    for (int k = 0; k < 12; k++) {
      if (k % 3 == 0) continue;
      float a = (k * 30 - 90) * 0.01745329f;
      int r0 = r - (full ? 20 : 12), r1 = r - 8;
      thickLine(cx + (int)(r0 * cosf(a)), cy + (int)(r0 * sinf(a)),
                cx + (int)(r1 * cosf(a)), cy + (int)(r1 * sinf(a)), 4, p.sub);
    }
  } else {
    // 石英三针：60 格细线刻度 + 红秒针
    for (int k = 0; k < 60; k++) {
      float a = (k * 6 - 90) * 0.01745329f;
      int r0 = r - ((k % 5 == 0) ? (full ? 20 : 12) : (full ? 11 : 7)), r1 = r - 3;
      gfx->drawLine(cx + (int)(r0 * cosf(a)), cy + (int)(r0 * sinf(a)),
                    cx + (int)(r1 * cosf(a)), cy + (int)(r1 * sinf(a)),
                    (k % 5 == 0) ? p.sub : p.dim);
    }
  }
  if (withHands) csAnalogHands(cx, cy, r, H, Mi, S, p, full, variant);
}


// ---------- 中央槽 = 时间：9 种样式；musicA.42 上移真圆心 + 放大，避开圆边裁切 ----------
void drawCenterClock() {
  // fix4：中央槽「时间」只保留 6 款数字（st 0..5），指针盘全部移到全屏表盘 st 6..8。
  zbgRect(CX - 140, 76, 280, 240);
  CsPal p = csPal();
  const int cy = CX;
  if (!clkSyncMs) {
    printCentered("--:--", CX, cy - 28, 10, p.sub);
    printCentered("等待手机对时", CX, cy + 52, 2, p.dim);
    needFlush = true;
    return;
  }
  int Y, Mo, D, H, Mi, S;
  clkNow(Y, Mo, D, H, Mi, S);
  char t[8], db[24];
  snprintf(t, sizeof(t), "%02d:%02d", H, Mi);
  csDateText(db, sizeof(db), Y, Mo, D);            // "Tue, Nov 5"
  switch (clockStyle) {
    case 0:                                        // 立体银白大数字（参考图左上）
      csEmboss(String(t), CX, cy - 62, 9, 0xFFDE, 0x6B4D);
      gfx->fillRect(CX - 74, cy + 20, 148, 2, p.line);
      printCentered(String(db), CX, cy + 44, 2, p.sub);
      break;
    case 1: {                                      // 荧光七段（参考图中上）
      const uint16_t on = 0x07E0, off = 0x0285;
      const int W = 46, Hh = 80, gap = 8;
      int x0 = CX - (4 * W + 3 * gap + 12) / 2;
      int digs[4] = {H / 10, H % 10, Mi / 10, Mi % 10};
      for (int i = 0; i < 4; i++)
        csSegDigit(x0 + i * (W + gap) + (i >= 2 ? 12 : 0), cy - 62, W, Hh, digs[i], on, off);
      gfx->fillRect(CX - 4, cy - 38, 8, 8, on);
      gfx->fillRect(CX - 4, cy - 6, 8, 8, on);
      printCentered(String(db), CX, cy + 44, 2, on);
      break;
    }
    case 2: {                                      // 线框纵向 09 / 45（参考图右上）
      String hh = String(t).substring(0, 2), mm = String(t).substring(3, 5);
      // 逐层偏移描出空心轮廓，再用底色填芯
      for (int k = 0; k < 2; k++) {
        String v = k ? mm : hh;
        int yy = cy - 108 + k * 108;
        for (int dy = -2; dy <= 2; dy += 2)
          for (int dx = -2; dx <= 2; dx += 2)
            printCentered(v, CX - 40 + dx, yy + dy, 9, p.sub);
        printCentered(v, CX - 40, yy, 9, p.bg);
      }
      gfx->fillRect(CX + 46, cy - 108, 2, 176, p.line);
      printCentered(String(db).substring(0, 3), CX + 118, cy - 30, 2, p.ink);
      printCentered(String(db).substring(5), CX + 118, cy + 2, 2, p.sub);
      break;
    }
    case 3:                                        // LED 点阵（参考图左中）
      csDotText(t, CX, cy - 58, 7, 0xFD20);
      csDotText(db, CX, cy + 52, 2, p.sub);
      break;
    case 4:                                        // 斜体粗体 + 蓝下划线（参考图左下）
      csDotItalic(t, CX, cy - 62, 8, p.ink);
      gfx->fillRect(CX - 108, cy + 26, 216, 5, 0x04DF);
      printCentered(String(db), CX, cy + 52, 2, p.sub);
      break;
    case 5: {                                      // 潮流纵向双色（参考图中下）
      String hh = String(t).substring(0, 2), mm = String(t).substring(3, 5);
      printCentered(hh, CX - 40, cy - 112, 10, p.ink);
      printCentered(mm, CX - 40, cy + 2, 10, p.cyan);
      gfx->fillRect(CX + 44, cy - 112, 4, 186, 0x04DF);
      printCentered(String(db).substring(0, 3), CX + 116, cy - 34, 2, p.ink);
      printCentered(String(db).substring(5), CX + 116, cy - 2, 2, p.sub);
      break;
    }
    default: break;                                // 6..8 归全屏表盘
  }
  needFlush = true;
}


// ---------- 全屏指针表盘（中央槽第 3 项） ----------
// musicA.41 A 纯净：无数字日期、无外圈秒刻度、真全屏、轴心上方实心电量含%、隐四功能槽。
// B 外环：近中心四黑圆，电量在轴心上方，指针叠在黑圆与电量之上。
// musicA.42：B 与 A 同真全屏半径；四功能黑圆略放大且避开电量/%。
// musicA.43：B 黑圆半径 33→56（+70%）；圆心径向保持 118（不外移）；不挡电量%；指针仍置顶。
// musicA.44：radial 118→133（+15 小幅外移，勿回 168）；标签 ASCII 修乱码；circleR 仍 56。
// musicA.45：标签/数字放大；按 Adafruit 6×8 字宽高 fit 进黑圆，不溢出。
// musicA.58 C Infograph：跳过程序面盘；叠在 type8 烘焙底图上画四弧+子盘字+扁针。
// musicA.60：底图/叠层相对 07v2 确认稿收边撑满；针心电量下移避开北子盘 SPD。
// musicA.64：叠层 IG_SCALE 改为与 BASE DIAL_SCALE（1.2469）一致（勿再用 1.14）。

// Infograph 几何（arcs-v6 × IG_SCALE）：0°=12 点，顺时针。
// FO-208：弧文字切线平行；天气/AQI 字脚朝圆心，BAT/UVI 字脚朝外；无 TEMP；无中心电量 pill。
// musicA.64：IG_SCALE 必须与烘焙 BASE 的 DIAL_SCALE（≈1.2469）一致。
// 旧值 1.14 会把彩弧/子盘字画偏内，BASE 灰轨露在外侧 → 实机「四角多余灰 + 像叠层」。
#define IG_SCALE 1.2469f
static void igPolarXY(float angDeg, float r, int &x, int &y) {
  float a = angDeg * 0.01745329f;
  x = CX + (int)(r * sinf(a));
  y = CX - (int)(r * cosf(a));
}
// Adafruit classic 5×8 列（仅弧读数用：空格 / % / - / 0-9），避免再 include glcdfont
static bool igFontCols(char ch, uint8_t out[5]) {
  static const uint8_t kSp[5]  = {0x00, 0x00, 0x00, 0x00, 0x00};
  static const uint8_t kPct[5] = {0x23, 0x13, 0x08, 0x64, 0x62};
  static const uint8_t kMinus[5] = {0x08, 0x08, 0x08, 0x08, 0x08};
  static const uint8_t kDig[10][5] = {
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, {0x00, 0x42, 0x7F, 0x40, 0x00},
    {0x72, 0x49, 0x49, 0x49, 0x46}, {0x21, 0x41, 0x49, 0x4D, 0x33},
    {0x18, 0x14, 0x12, 0x7F, 0x10}, {0x27, 0x45, 0x45, 0x45, 0x39},
    {0x3C, 0x4A, 0x49, 0x49, 0x31}, {0x41, 0x21, 0x11, 0x09, 0x07},
    {0x36, 0x49, 0x49, 0x49, 0x36}, {0x46, 0x49, 0x49, 0x29, 0x1E},
  };
  const uint8_t *src = nullptr;
  if (ch == ' ') src = kSp;
  else if (ch == '%') src = kPct;
  else if (ch == '-') src = kMinus;
  else if (ch >= '0' && ch <= '9') src = kDig[ch - '0'];
  else return false;
  for (int i = 0; i < 5; i++) out[i] = src[i];
  return true;
}
// 切线平行单字：feetIn=字脚朝圆心（rot=-a）；否则字脚朝外（rot=180-a），与确认稿 arcs-v6 一致。
static void igBlitGlyphTangent(char ch, float angDeg, float r, int sz, uint16_t color, bool feetIn) {
  uint8_t cols[5];
  if (!igFontCols(ch, cols) || sz < 1) return;
  if (r > 224.0f) r = 224.0f;
  int px, py;
  igPolarXY(angDeg, r, px, py);
  const float th = (feetIn ? -angDeg : (180.0f - angDeg)) * 0.017453292f;
  const float ct = cosf(th), st = sinf(th);
  const float cxg = 2.5f * (float)sz, cyg = 3.5f * (float)sz;
  for (int col = 0; col < 5; col++) {
    const uint8_t bits = cols[col];
    for (int row = 0; row < 8; row++) {
      if (!(bits & (1u << row))) continue;
      for (int dy = 0; dy < sz; dy++) {
        for (int dx = 0; dx < sz; dx++) {
          float gx = (float)(col * sz + dx) + 0.5f - cxg;
          float gy = (float)(row * sz + dy) + 0.5f - cyg;
          int sx = px + (int)lroundf(gx * ct - gy * st);
          int sy = py + (int)lroundf(gx * st + gy * ct);
          if ((unsigned)sx < (unsigned)LCD_WIDTH && (unsigned)sy < (unsigned)LCD_HEIGHT)
            gfx->writePixel(sx, sy, color);
        }
      }
    }
  }
}
// 沿弧排字：readingCw=真则随顺时针前进；BAT/UVI 翻转时用 false 保持可读顺序。
static void igPrintPolarTangent(const String &s, float angDeg, float r, int sz, uint16_t c,
                                bool feetIn, bool readingCw) {
  if (r > 224.0f) r = 224.0f;
  const int n = (int)s.length();
  if (n <= 0 || sz < 1) return;
  const float adv = 6.0f * (float)sz;
  const float total = adv * (float)n;
  const float dir = readingCw ? 1.0f : -1.0f;
  float acc = -0.5f * total;
  for (int i = 0; i < n; i++) {
    const float mid = acc + 0.5f * adv;
    const float a = angDeg + dir * (mid / r) * 57.29578f;
    igBlitGlyphTangent(s.charAt(i), a, r, sz, c, feetIn);
    acc += adv;
  }
}
static void igPrintPolar(const String &s, float angDeg, float r, int sz, uint16_t c) {
  // 兼容旧调用：默认切线 + 字脚朝圆心 + 顺时针阅读
  igPrintPolarTangent(s, angDeg, r, sz, c, true, true);
}
static void igFitLabVal(int bx, int by, int circleR, const char *lab, const char *val,
                        const CsPal &p) {
  const int pad = 6, gap = 3;
  const int maxW = 2 * (circleR - pad);
  const int labLen = (int)strlen(lab), valLen = (int)strlen(val);
  int labSz = 2, valSz = 3;
  while (valSz > 2 && valLen * 6 * valSz > maxW) valSz--;
  while (labSz > 1 && labLen * 6 * labSz > maxW) labSz--;
  while ((8 * labSz + gap + 8 * valSz) > maxW && (labSz > 1 || valSz > 2)) {
    if (valSz > labSz + 1) valSz--;
    else if (labSz > 1) labSz--;
    else if (valSz > 2) valSz--;
    else break;
  }
  const int stackH = 8 * labSz + gap + 8 * valSz;
  const int topY = by - stackH / 2;
  printCentered(String(lab), bx, topY, labSz, p.dim);
  printCentered(String(val), bx, topY + 8 * labSz + gap, valSz, p.ink);
}
static void igArcFill(float a0, float a1, float rMid, float halfW, float frac, uint16_t c) {
  if (frac < 0) frac = 0;
  if (frac > 1) frac = 1;
  if (frac <= 0.001f) return;
  float span = a1 - a0;
  arcBand(a0, a0 + span * frac, rMid - halfW, rMid + halfW, c, 255);
}
// arcs-v5：多段渐变色轨（AQI/UVI/天气满轨；按 frac 截断也可）
static uint16_t igLerp565(uint16_t a, uint16_t b, float t) {
  if (t <= 0) return a;
  if (t >= 1) return b;
  int r0 = (a >> 11) & 31, g0 = (a >> 5) & 63, b0 = a & 31;
  int r1 = (b >> 11) & 31, g1 = (b >> 5) & 63, b1 = b & 31;
  int r = r0 + (int)((r1 - r0) * t + 0.5f);
  int g = g0 + (int)((g1 - g0) * t + 0.5f);
  int bb = b0 + (int)((b1 - b0) * t + 0.5f);
  return (uint16_t)((r << 11) | (g << 5) | bb);
}
static uint16_t igGradSample(const uint16_t *stops, const float *pos, int n, float t) {
  if (t <= pos[0]) return stops[0];
  if (t >= pos[n - 1]) return stops[n - 1];
  for (int i = 0; i < n - 1; i++) {
    if (t <= pos[i + 1]) {
      float u = (t - pos[i]) / (pos[i + 1] - pos[i] + 1e-6f);
      return igLerp565(stops[i], stops[i + 1], u);
    }
  }
  return stops[n - 1];
}
static void igArcGrad(float a0, float a1, float rMid, float halfW, float frac,
                      const uint16_t *stops, const float *pos, int nStops) {
  if (frac < 0) frac = 0;
  if (frac > 1) frac = 1;
  if (frac <= 0.001f) return;
  float span = (a1 - a0) * frac;
  // FO-209：arcBand 极重；段数 capped（原 max 120 会每秒拖死主循环→触摸卡死）
  const int steps = (int)(fabsf(span) * 0.9f);
  int N = steps < 10 ? 10 : (steps > 28 ? 28 : steps);
  for (int i = 0; i < N; i++) {
    float t0 = (float)i / (float)N;
    float t1 = (float)(i + 1) / (float)N;
    float aa0 = a0 + span * t0;
    float aa1 = a0 + span * t1;
    float tg = (t0 + t1) * 0.5f;
    uint16_t c = igGradSample(stops, pos, nStops, tg);
    arcBand(aa0, aa1, rMid - halfW, rMid + halfW, c, 255);
  }
}

// FO-209：Infograph 底+色轨+读数缓存（不含指针）。秒针只 blit 缓存再叠针，避免每秒重跑 arcBand。
static uint16_t *igUnderHands = nullptr;
static bool igUnderValid = false;
static uint32_t igUnderSig = 0;
void invalidateInfographCache() { igUnderValid = false; }
static void igWxHiLo(int &lo, int &hi) {
  lo = hi = wxHave ? wxTemp : 0;
  if (!wxHave || wxN <= 0) return;
  if (wxHourly) {
    lo = hi = wxFh[0];
    for (int i = 0; i < wxN; i++) {
      if (wxFh[i] < lo) lo = wxFh[i];
      if (wxFh[i] > hi) hi = wxFh[i];
    }
    if (wxTemp < lo) lo = wxTemp;
    if (wxTemp > hi) hi = wxTemp;
  } else {
    // 按天：wxFh=最高，wxFl=最低
    hi = wxFh[0];
    lo = wxFl[0];
    if (wxTemp < lo) lo = wxTemp;
    if (wxTemp > hi) hi = wxTemp;
  }
}
static uint32_t igDataSig() {
  int lo = 0, hi = 0; igWxHiLo(lo, hi);
  uint32_t g = (uint32_t)lroundf(liveG() * 20.0f);   // 0.05g 量化，待机几乎不变
  return ((uint32_t)(uint16_t)batPct)
       | ((uint32_t)(uint8_t)(wxHave ? (wxTemp + 64) : 0) << 9)
       | ((uint32_t)(uint8_t)(wxAQI >= 0 ? (wxAQI > 255 ? 255 : wxAQI) : 255) << 17)
       | ((uint32_t)(wxHave ? 1u : 0u) << 25)
       | ((uint32_t)(uint8_t)(speedKmh >= 0 ? (speedKmh > 255 ? 255 : speedKmh) : 0) << 26)
       ^ (g * 0x9E3779B9u)
       ^ ((uint32_t)(tpValid ? tpFrontKpa : 0x7FFF) * 0x85EBCA6Bu)
       ^ ((uint32_t)(uint16_t)lo << 1) ^ ((uint32_t)(uint16_t)hi << 11);
}
static void igDrawHands(int H, int Mi, int S, const CsPal &p) {
  // 诚实扁针：确认稿比例 × IG_SCALE；宽 10/7/2
  float ah = ((H % 12) + Mi / 60.0f) * 0.52359878f - 1.5707963f;
  float am = (Mi + S / 60.0f) * 0.10471976f - 1.5707963f;
  float as = S * 0.10471976f - 1.5707963f;
  const float r = 172.0f * IG_SCALE;
  int hhx = CX + (int)(r * 0.52f * cosf(ah)), hhy = CX + (int)(r * 0.52f * sinf(ah));
  int mmx = CX + (int)(r * 0.78f * cosf(am)), mmy = CX + (int)(r * 0.78f * sinf(am));
  int ssx = CX + (int)(r * 0.90f * cosf(as)), ssy = CX + (int)(r * 0.90f * sinf(as));
  thickLine(CX, CX, hhx, hhy, 10, p.ink);
  thickLine(CX, CX, mmx, mmy, 7, p.ink);
  thickLine(CX, CX, ssx, ssy, 2, p.red);
  float dx = hhx - CX, dy = hhy - CX, len = sqrtf(dx * dx + dy * dy);
  if (len > 24) {
    float ux = dx / len, uy = dy / len;
    float bx = hhx - ux * 20, by = hhy - uy * 20;
    gfx->fillTriangle(hhx, hhy, bx - uy * 6, by + ux * 6, bx + uy * 6, by - ux * 6, p.ink);
  }
  aaDisc(CX, CX, 8, p.ink);
  aaDisc(CX, CX, 3, p.red);
}

static void igDrawUnderHands(const CsPal &p) {
  // arcs-v6：色轨加厚，嵌在灰轨环带内；半宽约 6.2px @×1.14
  const float hw = 5.4f * IG_SCALE;
  const float rWx = 174.0f * IG_SCALE;   // weather / AQI
  const float rBot = 170.0f * IG_SCALE;  // bat / UVI
  const uint16_t colBat = 0x2E9B;        // (40,210,220) 电量蓝弧按 %

  static const uint16_t wxStops[] = {0x45BF, 0x2E9B, 0x466C, 0xED26, 0xFCCA, 0xE8E4};
  static const float    wxPos[]   = {0.f, 0.22f, 0.42f, 0.62f, 0.82f, 1.f};
  static const uint16_t aqiStops[] = {0x264F, 0xB4C8, 0xF540, 0xEAE6, 0xA0B4};
  static const float    aqiPos[]   = {0.f, 0.35f, 0.60f, 0.80f, 1.f};
  static const uint16_t uviStops[] = {0x2DE2, 0xC6A6, 0xF500, 0xE986, 0x8834};
  static const float    uviPos[]   = {0.f, 0.30f, 0.55f, 0.75f, 1.f};

  int lo = 0, hi = 0;
  igWxHiLo(lo, hi);
  int cur = wxHave ? wxTemp : 0;
  float batFrac = (batPct >= 0) ? (batPct / 100.0f) : 0;
  igArcGrad(17, 59, rWx, hw, 1.0f, wxStops, wxPos, 6);
  igArcGrad(121, 161, rBot, hw, 1.0f, uviStops, uviPos, 5);
  igArcFill(197, 241, rBot, hw, batFrac, colBat);
  igArcGrad(296, 346, rWx, hw, 1.0f, aqiStops, aqiPos, 5);

  // FO-208：天气/AQI 字脚朝圆心+顺读；BAT/UVI 字脚朝外+反读（与确认 arcs-v6 一致）
  const float rTip = rWx + hw + 4.0f * IG_SCALE;
  if (wxHave) {
    igPrintPolarTangent(String(lo), 17.0f - 2.2f, rTip, 2, p.ink, true, true);
    igPrintPolarTangent(String(hi), 59.0f + 2.2f, rTip, 2, p.ink, true, true);
    igPrintPolarTangent(String(cur), 38.0f, rWx, 2, p.ink, true, true);
  } else {
    igPrintPolarTangent("--", 17.0f - 2.2f, rTip, 2, p.dim, true, true);
    igPrintPolarTangent("--", 59.0f + 2.2f, rTip, 2, p.dim, true, true);
    igPrintPolarTangent("--", 38.0f, rWx, 2, p.dim, true, true);
  }
  if (batPct >= 0) igPrintPolarTangent(String(batPct) + "%", 219.0f, rBot, 2, p.ink, false, false);
  else igPrintPolarTangent("--%", 219.0f, rBot, 2, p.dim, false, false);
  if (wxAQI >= 0) igPrintPolarTangent(String(wxAQI), 321.0f, rWx, 2, p.ink, true, true);
  else igPrintPolarTangent("--", 321.0f, rWx, 2, p.dim, true, true);
  igPrintPolarTangent("--", 141.0f, rBot, 2, p.dim, false, false);

  const char *lab[4] = {"SPD", "LIVE G", "TPMS", "WX"};
  const int off = (int)(83 * IG_SCALE + 0.5f);
  const int circleR = (int)(39.5f * IG_SCALE + 0.5f);
  const int sx[4] = {CX, CX + off, CX, CX - off};
  const int sy[4] = {CX - off, CX, CX + off, CX};
  char val[4][16];
  snprintf(val[0], sizeof(val[0]), "%d", speedKmh >= 0 ? speedKmh : 0);
  snprintf(val[1], sizeof(val[1]), "%.2f", liveG());
  if (tpValid) snprintf(val[2], sizeof(val[2]), "%d", tpFrontKpa);
  else snprintf(val[2], sizeof(val[2]), "--");
  if (wxHave) snprintf(val[3], sizeof(val[3]), "%d", wxTemp);
  else snprintf(val[3], sizeof(val[3]), "--");
  // musicA.64：先垫黑心再写字，避免 rebuild/缓存边界留下旧读数残影
  for (int k = 0; k < 4; k++) {
    gfx->fillCircle(sx[k], sy[k], circleR - 6, 0x0000);
    igFitLabVal(sx[k], sy[k], circleR, lab[k], val[k], p);
  }
}

static void drawInfographFace(int H, int Mi, int S, const CsPal &p) {
  // FO-209：数据未变则 blit 缓存（壁纸+色轨+字），只重画指针；触摸期间由 animateCat 跳过本函数
  // musicA.64：缓存命中时必须先整屏恢复再叠针，禁止在旧帧上直接画针（双缓冲/跳帧会叠影）
  const size_t BYTES = (size_t)LCD_WIDTH * LCD_HEIGHT * 2;
  uint16_t *fb = gfx->getFramebuffer();
  uint32_t sig = igDataSig();
  if (!igUnderHands) igUnderHands = (uint16_t *)ps_malloc(BYTES);
  bool rebuild = !igUnderValid || !igUnderHands || !fb || sig != igUnderSig;
  if (rebuild) {
    zbgRect(0, 0, 466, 466);           // 从 type8 BASE 擦净（含指针/字残影）
    igDrawUnderHands(p);
    if (igUnderHands && fb) {
      memcpy(igUnderHands, fb, BYTES); // 仅缓存「无指针」层
      igUnderValid = true;
      igUnderSig = sig;
    }
  } else if (igUnderHands && fb) {
    memcpy(fb, igUnderHands, BYTES);   // 整屏 blit 无针层，再画指针
  } else {
    zbgRect(0, 0, 466, 466);           // 无缓存兜底：仍擦净再画
    igDrawUnderHands(p);
  }
  igDrawHands(H, Mi, S, p);
}

void drawFullAnalog() {
  CsPal p = csPal();
  // musicA.41：删除最外圈秒刻度（不再调用 drawRingTicks）
  if (!clkSyncMs) {
    zbgRect(0, 0, 466, 466);
    printCentered("--:--", CX, 210, 8, p.sub);
    printCentered("等待手机对时", CX, 288, 2, p.dim);
    drawFullFaceBattery();
    drawPageDots();
    needFlush = true;
    return;
  }
  int Y, Mo, D, H, Mi, S;
  clkNow(Y, Mo, D, H, Mi, S);
  // musicA.58：C Infograph —— 壁纸 BASE 已含刻度/子盘铬/灰轨；不画 csAnalog 面盘、不画 B 实心圈
  if (fullFaceAB == 2) {
    (void)Y; (void)Mo; (void)D;
    drawInfographFace(H, Mi, S, p);
    drawPageDots();
    needFlush = true;
    return;
  }
  zbgRect(0, 0, 466, 466);
  // fix4：st 6/7/8 → 石英三针 / 运动三针 / 罗马时标；旧值 5 以下钳到 6
  int variant = (clockStyle >= 6 && clockStyle <= 8) ? clockStyle - 6 : 0;
  // musicA.42：A/B 同真全屏近表圈（屏半径 233）；不再把 B 缩一圈
  const int r = 226;
  // 图层：面盘 → B 黑圆 → 电量 → 指针置顶
  csAnalog(CX, CX, r, H, Mi, S, D, p, variant, true, false);
  if (fullFaceAB == 1) {
    const int ang[4] = {-45, 45, 135, 225};
    // printCentered 仅 Adafruit 内置 ASCII；UTF-8 中文会按字节乱码。固定英文微标。
    const char *lab[4] = {"SPD", "LIVE G", "TPMS", "WX"};
    char val[4][16];
    snprintf(val[0], sizeof(val[0]), "%d", speedKmh >= 0 ? speedKmh : 0);
    snprintf(val[1], sizeof(val[1]), "%.2f", liveG());
    if (tpValid) snprintf(val[2], sizeof(val[2]), "%d", tpFrontKpa);
    else snprintf(val[2], sizeof(val[2]), "--");
    if (wxHave) snprintf(val[3], sizeof(val[3]), "%d", wxTemp);
    else snprintf(val[3], sizeof(val[3]), "--");
    for (int k = 0; k < 4; k++) {
      float a = ang[k] * 0.01745329f;
      // musicA.44/45：circleR 56、radial 133 不变；仅放大圈内字号并 fit
      const int centerRadial = 133, circleR = 56;
      int bx = CX + (int)(centerRadial * cosf(a)), by = CX + (int)(centerRadial * sinf(a));
      aaDisc(bx, by, circleR, C_BLACK);
      aaDisc(bx, by, circleR - 1, 0x10A2);
      // Adafruit 内置字体：宽 6*sz、高 8*sz；y 为字顶。pad 留出黑盘边。
      const int pad = 6, gap = 3;
      const int maxW = 2 * (circleR - pad);
      const int labLen = (int)strlen(lab[k]), valLen = (int)strlen(val[k]);
      int labSz = 2, valSz = 4; // musicA.44 为 1/2；先抬一档再按宽高回落
      while (valSz > 2 && valLen * 6 * valSz > maxW) valSz--;
      while (labSz > 1 && labLen * 6 * labSz > maxW) labSz--;
      while ((8 * labSz + gap + 8 * valSz) > maxW && (labSz > 1 || valSz > 2)) {
        if (valSz > labSz + 1) valSz--;
        else if (labSz > 1) labSz--;
        else if (valSz > 2) valSz--;
        else break;
      }
      const int stackH = 8 * labSz + gap + 8 * valSz;
      const int topY = by - stackH / 2;
      printCentered(String(lab[k]), bx, topY, labSz, p.dim);
      printCentered(String(val[k]), bx, topY + 8 * labSz + gap, valSz, p.ink);
    }
  }
  drawFullFaceBattery();                                   // 轴心上方实心电量（指针之下）
  csAnalogHands(CX, CX, r, H, Mi, S, p, true, variant);    // 指针置顶（秒针配色随盘面）
  drawPageDots();
  needFlush = true;
}
void tickFullFace() { drawFullAnalog(); }
