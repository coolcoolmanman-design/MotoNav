#pragma once
// 进度环动画和导航页面。由 MotoNav.ino 按依赖顺序包含，仅编译一次。
#include "../../TurnRingMotion.h"
TurnRingMotion turnRingMotion;
uint32_t ringAnimationDrawAt = 0;
void updateRingTarget() {
  bool hasRing = (navDir != "S") && navDist <= STAGE_FAR_M;
  float target = hasRing ? (float)(STAGE_FAR_M - navDist) / STAGE_FAR_M : 0;
  const uint32_t now = millis();
  turnRingMotion.update(ringShown, target, navDir != ringDir || !ringActive || !hasRing, now);
  ringShown = turnRingMotion.value(now);
  ringTarget = target;
  ringActive = hasRing;
  ringDir = navDir;
}

void animateRing() {
  if (!inNav || !ringActive || staleShown || rerouteAt) return;
  const uint32_t now = millis();
  if (now - ringAnimationDrawAt < 33) return;
  ringAnimationDrawAt = now;
  const float next = turnRingMotion.value(now);
  if (next == ringShown) return;
  ringShown = next;
  navDirty = true;
}

// ===================== ui.6 第 6 种地图样式「大字转向」 =====================
// 参考图排版：无底图、超大转向箭头 + 超大距离数字 + 路名 + 车速 + 外圈进度弧。
// 昼 = 晴空蓝底 + 蓝主色；夜 = 深蓝底 + 琥珀黄主色（用户拍板严格按参考图）。
// crossReady() / mapModeActive() 已门控 → 路口放大图与车道自动隐藏；
// 超速红边与电量按用户要求保留。5 种播放样式与协议均不变（#t,12,1 复用旧命令）。
#define BT_DAY_BG1   0xF7DF   // #F4F9FF 中心
#define BT_DAY_BG2   0xCF1F   // #C9E0FF 边缘
#define BT_DAY_INK   0x0884   // #0A1220 深墨
#define BT_DAY_ACC   0x135F   // #1668FF 晴空蓝
#define BT_DAY_TRK   0xBDF7   // 淡蓝底圈
#define BT_NIGHT_BG1 0x1127   // #12243A 中心
#define BT_NIGHT_BG2 0x0042   // #050B14 边缘
#define BT_NIGHT_INK 0xFFFF
#define BT_NIGHT_ACC 0xFDA0   // #FFB703 琥珀黄
#define BT_NIGHT_TRK 0x18E3   // 深蓝底圈

void drawBigTurnFrame() {
  onWaitScreen = false;
  viewInit = false;
  const bool night = nightOn;
  const uint16_t bg1 = night ? BT_NIGHT_BG1 : BT_DAY_BG1;
  const uint16_t bg2 = night ? BT_NIGHT_BG2 : BT_DAY_BG2;
  const uint16_t ink = night ? BT_NIGHT_INK : BT_DAY_INK;
  const uint16_t acc = night ? BT_NIGHT_ACC : BT_DAY_ACC;
  const uint16_t trk = night ? BT_NIGHT_TRK : BT_DAY_TRK;

  // 纵向渐变（466 行逐行混合；圆外物理不可见，与原 fillScreen 一致）
  for (int y = 0; y < LCD_HEIGHT; y++) {
    int t = (int)((long)y * 255 / (LCD_HEIGHT - 1));
    gfx->fillRect(0, y, LCD_WIDTH, 1, blend565(bg1, bg2, t));
  }

  // 外圈：淡底圈 + 参考图式弧；转弯倒计时期间改为实时进度
  fillRingBand(0, 1, RING_R - 2, RING_R + 2, trk);
  if (ringActive) drawRingProgress(ringShown, 5, acc);
  else            fillRingBand(0, 0.79f, RING_R - 2, RING_R + 2, acc);

  // 超速红边（保留）
  bool over = hasCam && camLimit > 0 && speedKmh > camLimit;
  if (over) fillRingBand(0, 1, 228, 233, C_RED);

  // 电量 + 蓝牙点（保留）。不用 drawStatus()：它带一条黑底擦除带，会破坏浅色背景。
  if (slotMask & 0x04) aaDisc(CX - 24, 29, 8, bleConnected ? slotColor(C_GREEN, slotConnColor) : C_GREY);
  if (slotMask & 0x02) drawBattery(CX - 10, 20, false);

  // 超大转向箭头（放大可用区域与缩放系数，颜色随昼夜）
  drawBigIconAt(navDir, acc, 150, 1.35f, CX, 68, 218, 210);

  // 超大距离数字 + 小单位
  String d = distText(navDist);
  String num = d, unit = "";
  if (d.endsWith("km"))     { num = d.substring(0, d.length() - 2); unit = "km"; }
  else if (d.endsWith("m")) { num = d.substring(0, d.length() - 1); unit = "m"; }
  int numW = (int)num.length() * 6 * 8;
  printCentered(num, CX - 14, 252, 8, ink);
  gfx->setTextSize(2); gfx->setTextColor(ink);
  gfx->setCursor(CX + numW / 2 - 12, 292); gfx->print(unit);

  // 路名位图（按 ink 着色，浅底深字 / 深底白字）+ 车速小字
  drawTextImgC(0, 336, 0xE8, ink);
  String spd = String(speedKmh >= 0 ? speedKmh : 0) + " km/h";
  printCentered(spd, CX, 380, 2, over ? C_RED : ink);

  needFlush = true;
}

// 按当前全部状态画一整帧导航画面
void drawNavFrame() {
  onWaitScreen = false;                                 // v3.6 离开首页，猫猫不再动
  // ui.6：第 6 种「大字转向」走独立排版，必须早于 mapModeActive() 判定。
  if (themeIdx == ST_BIGTURN) { drawBigTurnFrame(); return; }
  if (mapModeActive()) { drawMapFrame(); return; }      // v2.0 小地图
  viewInit = false;
  if (cruiseOn) {                                       // v4.8 巡航但还没有地图：只显示车速和测速
    gfx->fillScreen(C_BLACK);
    bool over = hasCam && camLimit > 0 && speedKmh > camLimit;
    if (over) fillRingBand(0, 1, 228, 233, C_RED);
    drawStatus();
    drawSpeedBlock(over);
    printCentered("CRUISE", CX, 300, 4, C_CYAN);
    needFlush = true;
    return;
  }
  gfx->fillScreen(C_BLACK);
  bool isS = (navDir == "S");
  bool isFar = !isS && navDist > STAGE_FAR_M;
  bool lanes = laneN > 0 && ringActive;                 // 与地图页一样，跟随转弯倒计时
  bool over = hasCam && camLimit > 0 && speedKmh > camLimit;
  float laneK = lanes ? 0.85f : 1.0f;
  float iconCy = lanes ? ICON_CY_LANE : ICON_CY;

  // 边缘：进度环 / 超速红边
  float tp = ringActive ? -1 : tripFrac();              // v7.4 平时外圈是行程环，转弯倒计时时让位
  if (tp >= 0) drawTripRing(tp);
  else if (!isS) drawRingTrack();
  if (ringActive) {
    bool isNear = navDist <= STAGE_NEAR_M;
    drawRingProgress(ringShown, isNear ? 14 : 10, isNear ? C_ORANGE : C_YELLOW);
  }
  if (over) fillRingBand(0, 1, 228, 233, C_RED);

  drawStatus();
  drawSpeedBlock(over);

  // 中间图标
  // v1.8.2 图标可用区域：车速下方 ~ 车道（或距离数字）上方，各留 8 像素
  float iconTop = ICON_TOP, iconBot = (lanes ? LANES_Y : DIST_Y) - 8;
  if (isFar)     drawPreview(navDir, iconCy, 0.66f * laneK, iconTop, iconBot);
  else if (isS)  drawBigIcon("S", C_WHITE, iconCy, laneK, iconTop, iconBot);
  else           drawBigIcon(navDir, navDist <= STAGE_NEAR_M ? C_ORANGE : C_WHITE, iconCy, laneK, iconTop, iconBot);

  // 车道、距离、路名、底部信息
  if (lanes) drawLanes(LANES_Y);
  printCentered(distText(navDist), CX, lanes ? DIST_Y_LANE : DIST_Y, lanes ? 6 : 7, C_WHITE);
  drawTextImg(0, lanes ? ROAD_Y_LANE : ROAD_Y, 0xDD);
  drawTextImg(1, BOTTOM_Y, 0x99);                      // v1.7.3：常驻显示
  drawNavHints(128);                                    // v5.0
  needFlush = true;
}
