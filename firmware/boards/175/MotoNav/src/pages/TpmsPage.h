#pragma once
// v7.12.9-ui.6 下划：TPMS 胎压监测页（像素摩托，对应效果图定稿 v3）。
// v7.12.10 真实胎压通道：手机 App 扫描 LyTPMS 气嘴，读数经 #TP 命令透传到圆屏。
// 无 SIM 模拟：未收到数据时数值显示 0；电量经 #TP 第 5/6 参透传（-1 = 未知，不画）。
// 由 MotoNav.ino 在 PageNavigation.h 之前、GForcePage.h 之后包含，仅编译一次。

// #TP 透传的真实气嘴读数（由 CommandReceiver.h 写入）
int  tpFrontKpa = 0, tpRearKpa = 0;     // 前/后轮气压 kPa
int  tpFrontTemp = 0, tpRearTemp = 0;   // 前/后轮温度 °C
int  tpFrontBatt = -1, tpRearBatt = -1; // 前/后轮气嘴电量 %（-1 = 未知）
bool tpValid = false;                    // 是否收到过真实 #TP
unsigned long tpRxAt = 0;                // 最近一次收到 #TP 的时刻

#define TP_TEAL   0x0CB5   // #0E97AC 车身主色
#define TP_TEAL_D 0x0BF2   // #0B7F91 车身暗部
#define TP_CY_H   0x675E   // #62E9F5 高光
#define TP_PINK   0xFB78   // #FF6FC4 风挡
#define TP_PINK_L 0xFEBE   // #FFD6F2 风挡反光
#define TP_K      0x18E4   // #1D1D26 座垫/车架
#define TP_K2     0x31A7   // #35353F 暗灰
#define TP_METAL  0xAD56   // #A9A9B6 金属亮
#define TP_MET_D  0x738F   // #71717E 金属暗
#define TP_ENG    0x2966   // #2C2C36 发动机
#define TP_ENG_M  0x4A4B   // #4A4A58 发动机中
#define TP_LAMP   0xFEA1   // #FFD60A 大灯
#define TP_TAIL   0xFA27   // #FF453A 尾灯
#define TP_TIRE   0x3B88   // #3A3A45 胎面
#define TP_TIRE_D 0x2205   // #23232B 胎侧
#define TP_SPOKE  0x8C73   // #8E8E9C 辐条
#define TP_DASH   0x5D8D   // #5A5A68 引导虚线
#define TP_GND    0x1143   // #14141A 地面阴影

const int TP_S = 8, TP_BX = CX - 23 * TP_S, TP_BY = 300 - 12 * TP_S;   // 位图 46x24 格，轮心行 16
const int TP_HUB_FX = TP_BX + 36 * TP_S + 4, TP_HUB_RX = TP_BX + 9 * TP_S + 4, TP_HUB_Y = TP_BY + 16 * TP_S + 4;

// 像素摩托（面向右）：分段位图，字符 → TP 调色板，'.'=空
struct TpArtSeg { const int8_t y; const int8_t x; const char *s; };
static const TpArtSeg TP_ART[] = {
  {1,25,"pp"},{1,29,"m"},
  {2,24,"pPPp"},{2,28,"MM"},
  {3,23,"PppppP"},{3,29,"m"},
  {4,22,"DCHHHCCD"},
  {5,21,"DCHHHCCCYD"},
  {6,4,"RKKKK"},{6,21,"DCHHHCCCCYYD"},
  {7,4,"DKKKKKK"},{7,10,"KkkKKK"},{7,16,"DCHHCC"},{7,22,"CCHCCCCD"},{7,30,"M"},{7,31,"m"},
  {8,5,"DKKKKk"},{8,10,"KkKKKK"},{8,16,"DCHCCC"},{8,22,"CCkkkCCC"},{8,30,"Mm"},
  {9,6,"DDDD"},{9,10,"KkKKKk"},{9,16,"DDCCCD"},{9,22,"CCkkkCCC"},{9,30,"Mm"},{9,35,"DCCCCD"},
  {10,6,"kDDk"},{10,10,"k"},{10,11,"kkKKK"},{10,16,"DDCCDD"},{10,22,"DDDDCCCD"},{10,30,"Mm"},{10,33,"DCCCCCCD"},
  {11,9,"m"},{11,10,"mmmmm"},{11,15,"EeeeeeeE"},{11,23,"EE"},{11,25,"DDDDD"},{11,30,"MM"},{11,12,"R"},
  {12,10,"mmmm"},{12,14,"k"},{12,15,"EeMMeeee"},{12,23,"eE"},{12,26,"MMMMMm"},{12,27,"e"},{12,12,"e"},
  {13,10,"m.m.m"},{13,15,"EeeeeEE"},{13,22,"e"},{13,23,"E"},{13,25,"MMMMMM"},{13,31,"m"},{13,33,"Mm"},{13,12,"R"},
  {14,10,"mMm"},{14,15,"EEEEEEEe"},{14,23,"eE"},{14,25,"DMMMm"},{14,34,"Mm"},
  {15,10,"mmmm"},{15,16,"EeeeeE"},{15,35,"Mm"}
};

uint16_t tpColor(char ch) {
  switch (ch) {
    case 'D': return TP_TEAL_D; case 'C': return TP_TEAL;   case 'H': return TP_CY_H;
    case 'P': return TP_PINK;   case 'p': return TP_PINK_L; case 'K': return TP_K;
    case 'k': return TP_K2;     case 'M': return TP_METAL;  case 'm': return TP_MET_D;
    case 'E': return TP_ENG;    case 'e': return TP_ENG_M;  case 'Y': return TP_LAMP;
    case 'R': return TP_TAIL;
  }
  return 0;
}

void tpCell(int gx, int gy, uint16_t c) { gfx->fillRect(TP_BX + gx * TP_S, TP_BY + gy * TP_S, TP_S, TP_S, c); }

// 车轮：胎面/胎侧 + 刹车盘 + 旋转辐条（ang 随 tick 推进）
void tpWheel(int cx, int cy, float ang) {
  for (int yy = -8; yy <= 8; yy++)
    for (int xx = -8; xx <= 8; xx++) {
      float d = sqrtf((float)(xx * xx + yy * yy));
      if (d <= 7.5f && d > 5.7f) tpCell(cx + xx, cy + yy, TP_TIRE);
      else if (d <= 5.7f && d > 4.7f) tpCell(cx + xx, cy + yy, TP_TIRE_D);
    }
  for (int yy = -4; yy <= 4; yy++)
    for (int xx = -4; xx <= 4; xx++) {
      float d = sqrtf((float)(xx * xx + yy * yy));
      if (d <= 3.4f && d > 2.4f) tpCell(cx + xx, cy + yy, TP_ENG_M);
    }
  for (int k = 0; k < 4; k++) {
    float a = ang + k * 1.5707963f;
    for (float q = 1.2f; q < 3.3f; q += 0.6f)
      tpCell(cx + (int)lroundf(q * cosf(a)), cy + (int)lroundf(q * sinf(a)), TP_SPOKE);
  }
  tpCell(cx, cy, TP_METAL);
}

// 数据块 → 轮毂的流动虚线（u 相位随时间行进）
void tpDashes(int x, int hx, int hy, float t) {
  float ph = fmodf(t * 0.9f, 0.14f);
  for (float u = ph; u < 1.0f; u += 0.14f) {
    int px = (int)lroundf(x + (hx - x) * u), py = (int)lroundf(214 + (hy - 214) * u);
    gfx->fillRect(px, py, 3, 3, TP_DASH);
  }
}

// 胎压数据块（EN 标签 + kPa 大字 + 温度）
void tpBlock(int x, const char *en, int kpa, int tempC, bool hot) {
  uint16_t c = hot ? C_ORANGE : C_GREEN;
  printCentered(String(en), x, 100, 2, GF_CY);
  printCentered(String(kpa), x - 16, 138, 5, C_WHITE);
  printCentered("kPa", x + 56, 152, 2, GF_CY);
  printCentered(String(tempC), x - 4, 184, 3, c);
  aaDisc(x + 14, 189, 2, c);
  printCentered("C", x + 24, 184, 3, c);
}

// 像素电池图标（x,y=左上角）：18x10 外框 + 2x4 正极凸头，4 格电量按 pct 填充；pct<0 只画空框
void tpBattery(int x, int y, int pct) {
  gfx->fillRect(x, y, 18, 10, TP_K2);
  gfx->fillRect(x + 1, y + 1, 16, 8, C_BLACK);
  gfx->fillRect(x + 18, y + 3, 2, 4, TP_K2);
  if (pct > 0) {
    int cells = (pct + 24) / 25;                    // 4 格：>75%=4, >50%=3...
    if (cells > 4) cells = 4;
    uint16_t c = (pct <= 20) ? C_ORANGE : C_GREEN;  // 低电量橙色警示
    for (int i = 0; i < cells; i++) gfx->fillRect(x + 2 + i * 4, y + 2, 3, 6, c);
  }
}

// 气嘴电量：电池图标 + 百分比数字（EN 标签上方居中；前轮 x=356 贴近圆弧，必须内收避免裁切）
void tpBattBadge(int x, int pct) {
  if (pct < 0) return;                              // 未收到电量：不画
  tpBattery(x - 26, 80, pct);                       // 图标 18x10+凸头 → x-26..x-4
  printCentered(String(pct) + "%", x + 8, 82, 1, C_GREY);   // 文字右缘 ≈x+20，圆屏 y≈85 边界内
}

void drawTpms() {
  onWaitScreen = false;
  float t = millis() / 1000.0f;
  gfx->fillScreen(C_BLACK);
  drawStatus();

  printCentered("TPMS", 140, 44, 3, GF_CY);
  // v7.12.10：收到过 #TP 即显示并一直保持最后一次读数（气嘴静止不广播也不清零，
  // 直到下次压力变化刷新）；开机后未收到过 #TP 时显示 0、无 LIVE。
  bool tpLive = tpValid;
  if (tpLive) printCentered("LIVE", 356, 48, 2, C_GREEN);
  int fK = tpLive ? tpFrontKpa : 0, fT = tpLive ? tpFrontTemp : 0;
  int rK = tpLive ? tpRearKpa : 0, rT = tpLive ? tpRearTemp : 0;

  tpBlock(356, "FRONT", fK, fT, tpLive && fT >= 60);
  tpBlock(112, "REAR", rK, rT, tpLive && rT >= 60);
  tpBattBadge(356, tpLive ? tpFrontBatt : -1);
  tpBattBadge(112, tpLive ? tpRearBatt : -1);
  tpDashes(356, TP_HUB_FX, TP_HUB_Y, t);
  tpDashes(112, TP_HUB_RX, TP_HUB_Y, t);

  // 地面阴影
  for (int x = 44; x <= 422; x += 14) gfx->fillRect(x, TP_BY + 23 * TP_S, 8, 3, TP_GND);

  // 车轮（辐条随时间转）+ 车身位图（车身后画，挡泥板盖住胎顶）
  float ang = t * 1.2f;
  tpWheel(9, 16, ang); tpWheel(36, 16, ang);
  for (const TpArtSeg &seg : TP_ART)
    for (int i = 0; seg.s[i]; i++) {
      uint16_t c = tpColor(seg.s[i]);
      if (c) tpCell(seg.x + i, seg.y, c);
    }

  // BLE 状态（闪烁连接点 + 说明）
  if ((millis() / 500) % 2) aaDisc(116, 420, 4, tpLive ? C_GREEN : C_GREY);
  printCentered(tpLive ? "BLE TPMS - LIVE" : "BLE TPMS", 239, 416, 1, C_GREY);

  drawPageDots();
  needFlush = true;
}

// 两页共用动画节拍：页面可见时每 500ms 重画一次（辐条旋转 + 虚线行进）
void gTpmsTick() {
  if (sliding || callOn || pairOn) return;
  static unsigned long last = 0;
  unsigned long now = millis();
  if (now - last < 500) return;
  last = now;
  if (page == 3) drawGForce();
  else if (page == 4) drawTpms();
}