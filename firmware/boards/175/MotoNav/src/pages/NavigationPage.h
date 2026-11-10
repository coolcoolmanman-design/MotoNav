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


// ===================== ui.7 第 7 种地图样式「矢量简图」(id 13) =====================
// 用户拍板：A 新增独立第 7 种 / 夜间严格薄荷绿 / 不做红绿灯倒计时 /
//           路口放大图仍隐藏、底部车道条完全由中部车道胶囊取代。
// 参考图 932x932 = 466x466 圆屏的 2 倍，下面所有坐标即固件像素坐标。
// 无栅格底图、无新增 BLE 协议：#t,13,1 + #j（车道）+ #v（车速）+ #k（限速）+ #e（行程）。
// musicA.55：中部改读 bulk type 2 的真实路线 routeX/routeY（局部米坐标），
// 车恒在窗口起点、车头朝上，路线按 posHdg 旋转后投影；无路线数据时才降级到示意折线。
// musicA.56：修掉投影的 4 处几何缺陷（贴边 clamp / 车位漂移 / 缩放跳 / 弯道抹平）。
// musicA.57：参照龙珠雷达改画「完整主导航路线」，不再用前方窗口折线（实机线不完整）。
// 数据一直在固件里，过去只是没消费；iOS / 协议 / bulk 类型均零改动。
#define VH_DAY_BG     0xF7BF   // #F4F6F8
#define VH_DAY_ACC    0x0B1F   // #0A63FF
#define VH_DAY_INK    0x0884   // #0B1220
#define VH_DAY_SUB    0x5B2E   // #5A6573
#define VH_DAY_ROAD   0xC67A   // #C3CCD6
#define VH_DAY_GREEN  0xCF39   // #CDE7C8
#define VH_DAY_WATER  0xDF5E   // #DDE9F6
#define VH_DAY_PILL   0xE75D   // #E3E8EE（细环同色）
#define VH_DAY_PILLI  0x8CB5   // #8A97A8
#define VH_NIGHT_BG   0x0862   // #0B0F14
#define VH_NIGHT_ACC  0x1F16   // #19E3B1 薄荷绿
#define VH_NIGHT_INK  0xFFFF
#define VH_NIGHT_SUB  0x8CD5   // #8D9AA8
#define VH_NIGHT_ROAD 0x29A8   // #2A3745
#define VH_NIGHT_GREEN 0x1123  // #11261F
#define VH_NIGHT_WATER 0x10E4  // #101C25
#define VH_NIGHT_PILL 0x1926   // #1B2733
#define VH_NIGHT_PILLI 0x63B1  // #65758A
#define VH_NIGHT_RING 0x1946   // #1E2833
#define VH_RED        0xF9C6   // #FF3B30

// 带起端圆头的粗线（thickLine 自带终点圆头）
static void vhSeg(float x0, float y0, float x1, float y1, float w, uint16_t c) {
  thickLine(x0, y0, x1, y1, w, c);
  aaDisc(x0, y0, w / 2, c);
}

// 左上大转向箭头：V 形箭头 + 直角下折尾（右转做 x=136 镜像）
static void vhBigArrow(const String &dir, uint16_t acc) {
  if (dir == "S") {
    vhSeg(136, 140, 136, 60, 15, acc);
    arrowHead(136, 58, 0, 24, acc);
    return;
  }
  if (dir == "A") {
    vhSeg(105, 95, 130, 120, 15, acc);
    vhSeg(130, 120, 170, 60, 15, acc);
    return;
  }
  const bool mir = (dir == "R");
  auto X = [mir](float x) -> float { return mir ? 272.0f - x : x; };
  vhSeg(X(128), 76, X(155), 76, 15, acc);   // 尾：水平右行
  vhSeg(X(155), 76, X(172), 96, 15, acc);   // 圆角
  vhSeg(X(172), 96, X(172), 140, 15, acc);  // 下折
  vhSeg(X(137), 52, X(100), 85, 15, acc);   // 箭头 V 上臂
  vhSeg(X(100), 85, X(139), 112, 15, acc);  // 箭头 V 下臂
}

// 中部车道胶囊：完全取代原来的底部车道条；推荐车道亮主色，其余灰
static void vhLanePill(uint16_t pill, uint16_t pillInk, uint16_t acc) {
  if (laneN <= 0) return;
  gfx->fillRoundRect(118, 168, 150, 44, 22, pill);
  float step = fminf(50.0f, 150.0f / laneN);
  float h = fminf(32.0f, step * 0.85f);
  float y0 = 168 + (44 - h) / 2;
  recIcon = false;                            // 直接画到 framebuffer，不进车道缓存
  for (int i = 0; i < laneN; i++) {
    float cx = 118 + step * (i + 0.5f);
    bool rec = laneFr[i] != 0xF;
    uint8_t code = rec ? laneFr[i] : laneBg[i];
    drawLaneGlyph(cx, y0, h, laneFlags(code), rec ? acc : pillInk);
  }
}



// HUD 让位：把该区域往背景色淡化 82%，路线在下方仍隐约连续可见（不硬切断）
static void vhFadeRect(int x0, int y0, int w, int h, uint16_t bg) {
  uint16_t *fb = gfx->getFramebuffer();
  if (!fb) return;
  for (int y = y0; y < y0 + h; y++) {
    if (y < 0 || y >= LCD_HEIGHT) continue;
    uint16_t *row = fb + (size_t)y * LCD_WIDTH;
    for (int x = x0; x < x0 + w; x++) {
      if (x < 0 || x >= LCD_WIDTH) continue;
      row[x] = blend565(row[x], bg, 82);
    }
  }
}

// ---------- musicA.57：龙珠雷达式「完整主导航路线」 ----------
// 用户实机反馈：56 的前方 200m / 16 点窗口折线「导航线不完整」，要求参照龙珠雷达
// 保证主导航路线完整。现在不再自己投影窗口，直接复用 drawMapFrame 的视图变换
// （车在中心、朝向平滑、缩放随车速）+ drawRouteRange(routeProg → 终点)。
// drawRouteRange 用 mapPolyline 自带视口裁剪，线连续贯穿屏幕、不会在窗口边界断掉；
// 整条剩余主路线始终跟着真实 routeX/routeY 走，车标随车平滑移动。
static void vhDrawFullRoute(uint16_t acc, uint16_t bg) {
  const unsigned long t = millis();
  float cx, cy, hPred;
  predictPose(t, leadMs / 1000.0f, cx, cy, hPred);
  viewTargetHdg = hPred;
  if (corrAt) {                                      // 与地图帧同款 GPS 平滑修正
    const float f = fmaxf(0, 1 - (t - corrAt) / 400.0f);
    cx += corrX * f; cy += corrY * f;
  }
  if (!viewInit) { viewHdg = hPred; viewInit = true; }
  float dh = hPred - viewHdg;
  while (dh > 180) dh -= 360;
  while (dh < -180) dh += 360;
  static unsigned long vhViewAt = 0;
  float vdt = vhViewAt ? (t - vhViewAt) / 1000.0f : 0.05f;
  vhViewAt = t;
  if (vdt > 0.25f) vdt = 0.25f;
  const float hmax = fminf(70.0f * vdt, 3.5f);
  viewHdg += constrain(dh * (1.0f - expf(-vdt / 0.22f)), -hmax, hmax);
  const float targetS = constrain(0.85f - posKmh * 0.006f, 0.38f, 0.85f);
  if (viewScale < 0.05f) viewScale = 0.55f;
  viewScale += (targetS - viewScale) * (1.0f - expf(-vdt / 1.2f));
  const float sc = viewScale;
  const float hs = viewHdg * PI / 180.0f, cs = cosf(hs), sn = sinf(hs);
  const float viewR = (CX + ((float)MAP_PY - CX) + 30.0f) / sc;
  const float wk = sc / 0.6f;
  const float w0 = fmaxf(6.0f, 13.0f * wk), w1 = fmaxf(4.0f, 9.0f * wk);
  if (!cruiseOn && routeN > 1 && routeFrame == posFrame) {
    updateRouteProgress(cx, cy);
    const float split = routeProg >= 0 ? routeProg : 0;
    if (split > 0)                                   // 已走过的一段压暗
      drawRouteRange(0, split, cx, cy, sc, cs, sn, viewR,
                     w0 * 0.8f, w1 * 0.8f, bg, blend565(bg, acc, 30));
    drawRouteRange(split, (float)(routeN - 1), cx, cy, sc, cs, sn, viewR,
                   w0, w1, C_WHITE, acc);           // 主路线：白描边 + 主色内芯
  }
  drawPlayer(CX, MAP_PY);                          // 龙珠雷达同款车头
}


void drawVectorHeroFrame() {
  onWaitScreen = false;
  const bool night = nightOn;
  const uint16_t bg   = night ? VH_NIGHT_BG   : VH_DAY_BG;
  const uint16_t acc  = night ? VH_NIGHT_ACC  : VH_DAY_ACC;
  const uint16_t ink  = night ? VH_NIGHT_INK  : VH_DAY_INK;
  const uint16_t sub  = night ? VH_NIGHT_SUB  : VH_DAY_SUB;
  const uint16_t road = night ? VH_NIGHT_ROAD : VH_DAY_ROAD;
  const uint16_t green= night ? VH_NIGHT_GREEN: VH_DAY_GREEN;
  const uint16_t water= night ? VH_NIGHT_WATER: VH_DAY_WATER;
  const uint16_t pill = night ? VH_NIGHT_PILL : VH_DAY_PILL;
  const uint16_t pillI= night ? VH_NIGHT_PILLI: VH_DAY_PILLI;
  const uint16_t ring = night ? VH_NIGHT_RING : VH_DAY_PILL;
  const bool over = hasCam && camLimit > 0 && speedKmh > camLimit;

  gfx->fillScreen(bg);
  const bool hasRoute = !cruiseOn && routeN > 1 && routeFrame == posFrame;
  if (!hasRoute) {
    // 无真实路线时才画降级示意装饰（绿地 / 水面 / 两根灰路）
    vhSeg(248, 160, 285, 55, 22, blend565(bg, road, 42));
    gfx->fillTriangle(0, 352, 95, 344, 0, 400, water);
    gfx->fillTriangle(95, 344, 95, 400, 0, 400, water);
    gfx->fillTriangle(95, 344, 190, 382, 95, 400, water);
    gfx->fillTriangle(190, 382, 190, 400, 95, 400, water);
    gfx->fillTriangle(16, 190, 112, 172, 58, 278, green);
    gfx->fillTriangle(16, 190, 58, 278, 16, 258, green);
    vhSeg(16, 300, 205, 252, 5, road);
    vhSeg(205, 252, 226, 250, 5, road);
    vhSeg(318, 166, 346, 378, 8, road);
  }
  // musicA.57：完整主路线 + 车（视图跟随，线连续贯穿屏幕，不再断在窗口边界）
  vhDrawFullRoute(acc, bg);

  // HUD 让位：只淡化文字实际占用的小块（半透明而非硬遮罩），
  // 主路线在其下方仍连续可见；车道胶囊 / 限速牌自带底色，无需处理。
  vhFadeRect(78, 40, 108, 112, bg);          // 大转向箭头
  vhFadeRect(186, 50, 178, 92, bg);          // 超大距离数字 + 单位
  vhFadeRect(182, 334, 158, 72, bg);         // 大字车速 + km/h
  vhFadeRect(140, 412, 190, 26, bg);         // 时间 · 剩余里程
  // 外圈：细环 + 参考图式弧（0=正上方顺时针：190°→322°，约 37% 圈）
  fillRingBand(0, 1, 225, 227, ring);
  // musicA.55：固定参考图 37% 圈，不再随 navDist 跳变（过路口重置曾让弧与车头一起乱跳）。
  const float q0 = 190.0f / 360.0f, q1 = q0 + 0.37f;
  const uint16_t arcCol = over ? VH_RED : acc;
  fillRingBand(q0, q1, 222, 228, arcCol);
  aaDisc(CX + 225 * sinf(q0 * 2 * PI), CX - 225 * cosf(q0 * 2 * PI), 3, arcCol);
  aaDisc(CX + 225 * sinf(q1 * 2 * PI), CX - 225 * cosf(q1 * 2 * PI), 3, arcCol);
  if (over) fillRingBand(0, 1, 228, 233, VH_RED);   // 超速红边（沿用上一稿约定）

  // 超大转向箭头 + 距离数字（左对齐 x=195）
  vhBigArrow(navDir, acc);
  String d = distText(navDist);
  String num = d, unit = "";
  if (d.endsWith("km"))      { num = d.substring(0, d.length() - 2); unit = "km"; }
  else if (d.endsWith("m"))  { num = d.substring(0, d.length() - 1); unit = "m"; }
  int numW = (int)num.length() * 6 * 8;
  printCentered(num, 195 + numW / 2, 70, 8, ink);
  gfx->setTextSize(3); gfx->setTextColor(sub);
  gfx->setCursor(200 + numW, 106); gfx->print(unit);

  // 车道胶囊（无 #j 数据时自动不画）
  vhLanePill(pill, pillI, acc);

  // 底部：限速牌 + 大字车速 + 时间 · 剩余里程
  if (hasCam && camLimit > 0) {
    gfx->fillCircle(160, 386, 25, VH_RED);
    gfx->fillCircle(160, 386, 20, C_WHITE);
    String lim = String(camLimit);
    printCentered(lim, 160, 374, 3, VH_DAY_INK);
  }
  String spd = String(speedKmh >= 0 ? speedKmh : 0);
  printCentered(spd, 205 + (int)spd.length() * 6 * 7 / 2, 350, 7, over ? VH_RED : ink);
  gfx->setTextSize(2); gfx->setTextColor(sub);
  gfx->setCursor(210 + (int)spd.length() * 6 * 7, 382); gfx->print("km/h");
  char tb[8];
  if (tripEta > 0) snprintf(tb, sizeof(tb), "%02d:%02d", (tripEta / 100) % 24, tripEta % 100);
  else { int Y, Mo, D, H, Mi, S; clkNow(Y, Mo, D, H, Mi, S); snprintf(tb, sizeof(tb), "%02d:%02d", H, Mi); }
  printCentered(String(tb) + " · " + (tripRemainM > 0 ? distText(tripRemainM) : String("0m")), 233, 424, 2, sub);

  needFlush = true;
}

// 按当前全部状态画一整帧导航画面
void drawNavFrame() {
  onWaitScreen = false;                                 // v3.6 离开首页，猫猫不再动
  // ui.7：第 7 种「矢量简图」走独立排版，必须早于 mapModeActive() 判定。
  if (themeIdx == ST_VECTOR) { drawVectorHeroFrame(); return; }
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
