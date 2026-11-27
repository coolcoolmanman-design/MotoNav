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
// musicA.131：矢量简图改 Leonida / GTA HUD 色（tilt 几何不变）
#define VH_DAY_BG     0xEF5E   // #E8E8F0 陆地
#define VH_DAY_ACC    0xF991   // #FC3088 粉路线 / HUD 强调
#define VH_DAY_INK    0xFFFF   // 白字
#define VH_DAY_SUB    0xAD75   // #A8B0B8
#define VH_DAY_ROAD   0x9D15   // #98A0AC
#define VH_DAY_GREEN  0x4C8F   // #489078 公园
#define VH_DAY_WATER  0x1599   // #10B0C8 青水
#define VH_DAY_PILL   0xD69A   // 浅灰胶囊
#define VH_DAY_PILLI  0x8C71   // 胶囊未选
#define VH_NIGHT_BG   0x1905   // #1C222C
#define VH_NIGHT_ACC  0xF991   // #FC3088
#define VH_NIGHT_INK  0xFFFF
#define VH_NIGHT_SUB  0xD6BA   // #D2DCE6
#define VH_NIGHT_ROAD 0x7413   // #708098
#define VH_NIGHT_GREEN 0x09C5  // #0C3828
#define VH_NIGHT_WATER 0x1C54  // #1888A0
#define VH_NIGHT_PILL 0x2128   // 深胶囊
#define VH_NIGHT_PILLI 0x6B6F
#define VH_NIGHT_RING 0x2945
#define VH_RED        0xF9C6   // #FF3B30
#include "VectorTilt.h"   // musicA.131：昼夜透视 / 近路口放大 / 路名行 + GTA 色

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
static void vhFadeRect(int x0, int y0, int w, int h, uint16_t bg, uint8_t alpha = 82) {
  uint16_t *fb = gfx->getFramebuffer();
  if (!fb) return;
  for (int y = y0; y < y0 + h; y++) {
    if (y < 0 || y >= LCD_HEIGHT) continue;
    uint16_t *row = fb + (size_t)y * LCD_WIDTH;
    for (int x = x0; x < x0 + w; x++) {
      if (x < 0 || x >= LCD_WIDTH) continue;
      row[x] = blend565(row[x], bg, alpha);
    }
  }
}

// ---------- musicA.59：矢量简图周边路网（灰色粗线） ----------
// App 把栅格底图的道路标签层矢量化成折线，走 bulk type 1 下发（10 KB 级，比栅格省一个数量级）。
// 这里沿用 drawMapFrame 同一套视图变换把要素画成灰线；主路线随后叠在最上层，路网永远在下面。
// 用户拍板：标准密度 + 主干 16 px，次干 12 px、支路 8 px 按比例退。
static void vhDrawRoadNet(float cx, float cy, float sc, float cs, float sn, float viewR) {
  if (!mapValid || mapFrame != posFrame || mapFeatN <= 0) return;
  const float lx = cx - mapOx, ly = cy - mapOy;
  const float wk = fminf(sc / 0.6f, 1.45f);   // musicA.130：放大后线宽封顶（原最大 sc=0.85 时 wk=1.42，不变）
  const uint16_t rd = nightOn ? VH_NIGHT_ROAD : VH_DAY_ROAD;
  const float wTier[4] = {0.0f, 16.0f * wk, 12.0f * wk, 8.0f * wk};
  const float wTierT[4] = {0.0f, 7.0f * wk, 5.5f * wk, 4.0f * wk};   // 透视模式：细线路网
  for (int i = 0; i < mapFeatN; i++) {
    const int16_t *bx = featBox + 4 * i;
    if (bx[2] < lx - viewR || bx[0] > lx + viewR || bx[3] < ly - viewR || bx[1] > ly + viewR) continue;
    int np = featNp[i];
    if (np < 2) continue;
    if (np > POLY_PTS) np = POLY_PTS;
    const uint8_t *pp = mapShow + featOff[i];
    for (int k = 0; k < np; k++) {
      float dx = rd16(pp + 4 * k) - lx, dy = rd16(pp + 4 * k + 2) - ly;
      tmpX[k] = CX + sc * (dx * cs - dy * sn);
      tmpY[k] = MAP_PY - sc * (dx * sn + dy * cs);
    }
    int c = featCls[i] & 0x7F;
    if (c < 1) c = 1;
    if (c > 3) c = 3;
    if (vhTilt) vhPolyTilt(tmpX, tmpY, np, fmaxf(3.0f, wTierT[c]), vhTiltRoad(), 1.5f);
    else mapPolyline(tmpX, tmpY, np, fmaxf(4.0f, wTier[c]), rd, false);
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
  const float targetS = vhApproachScale(constrain(0.85f - posKmh * 0.006f, 0.38f, 0.85f));   // musicA.130 近路口放大
  if (viewScale < 0.05f) viewScale = 0.55f;
  viewScale += (targetS - viewScale) * (1.0f - expf(-vdt / 1.2f));
  const float sc = viewScale;
  const float hs = viewHdg * PI / 180.0f, cs = cosf(hs), sn = sinf(hs);
  const float viewR = (CX + ((float)MAP_PY - CX) + 30.0f) / sc;
  const float wk = fminf(sc / 0.6f, 1.45f);   // musicA.130：放大后线宽封顶（原最大 sc=0.85 时 wk=1.42，不变）
  const bool tilt = vhTilt;
  const float viewRT = tilt ? viewR * 1.9f : viewR;
  if (vhBird) vhDrawBirdBuildings(cx, cy, sc, cs, sn, viewRT);   // musicA.133：密实楼块在路网下
  vhDrawRoadNet(cx, cy, sc, cs, sn, viewRT);
  if (!cruiseOn && routeN > 1 && routeFrame == posFrame) {
    updateRouteProgress(cx, cy);
    const float split = routeProg >= 0 ? routeProg : 0;
    if (vhBird) {
      // musicA.133：视频 HUD — 实心导航蓝（非粉虚线）
      const uint16_t blue = VH_BIRD_ROUTE;
      const float w0 = fmaxf(12.0f, 26.0f * wk);
      const float w1 = fmaxf(8.0f, 16.0f * wk);
      if (split > 0)
        vhRangeTilt(0, split, cx, cy, sc, cs, sn, viewRT,
                    w0 * 0.75f, w1 * 0.75f, blend565(bg, blue, 90), blend565(bg, blue, 140));
      vhRangeTilt(split, (float)(routeN - 1), cx, cy, sc, cs, sn, viewRT,
                  w0, w1, blue, 0x5B9F);
      // 视频目的地：蓝盘（非粉盘 + 青准星）
      float dx = routeX[routeN - 1] - cx, dy = routeY[routeN - 1] - cy;
      float px = CX + sc * (dx * cs - dy * sn), py = MAP_PY - sc * (dx * sn + dy * cs);
      if (tilt) vhTiltPt(px, py);
      float ex = px - CX, ey = py - CX, er = sqrtf(ex * ex + ey * ey);
      if (er > 192) { px = CX + ex / er * 192; py = CX + ey / er * 192; }
      aaDisc(px, py, 10, blue);
      aaDisc(px, py, 4, bg);
      aaDisc(px, py, 2, blue);
    } else {
      // 矢量简图：保留 GTA 粉虚线 + 青准星（不改 theme 13/14）
      const float wClear = tilt ? fmaxf(8.0f, 38.0f * wk) : 0;
      const float wDash  = tilt ? fmaxf(8.0f, 16.0f * wk) : fmaxf(7.0f, 12.0f * wk);
      const uint16_t pink = vhTiltAcc();
      if (split > 0)
        vhRangeDashed(0, split, cx, cy, sc, cs, sn, viewRT, wClear * 0.8f, wDash * 0.75f,
                      bg, blend565(bg, pink, 40), tilt);
      vhRangeDashed(split, (float)(routeN - 1), cx, cy, sc, cs, sn, viewRT,
                    wClear, wDash, bg, pink, tilt);
      float dx = routeX[routeN - 1] - cx, dy = routeY[routeN - 1] - cy;
      float px = CX + sc * (dx * cs - dy * sn), py = MAP_PY - sc * (dx * sn + dy * cs);
      if (tilt) vhTiltPt(px, py);
      float ex = px - CX, ey = py - CX, er = sqrtf(ex * ex + ey * ey);
      if (er > 192) { px = CX + ex / er * 192; py = CX + ey / er * 192; }
      aaDisc(px, py, 10, VH_DEST);
      aaDisc(px, py, 4, bg);
      const uint16_t ret = vhReticle();
      gfx->drawCircle(px, py, 14, ret);
      gfx->drawFastHLine(px - 18, py, 8, ret);
      gfx->drawFastHLine(px + 10, py, 8, ret);
      gfx->drawFastVLine(px, py - 18, 8, ret);
      gfx->drawFastVLine(px, py + 10, 8, ret);
    }
  }
  if (vhBird) vhDrawBirdChevron(CX, tilt ? vhTiltPy() : MAP_PY);   // 视频 chevron
  else drawPlayer(CX, tilt ? vhTiltPy() : MAP_PY);                // GTA 白三角（theme 13）
}


void drawVectorHeroFrame() {
  onWaitScreen = false;
  vhBird = (themeIdx == ST_VECTOR_3D_BIRDSEYE);
  const bool night = nightOn;
  // musicA.133 鸟瞰：白地 + 深墨 HUD + 导航蓝强调（视频观感；非 GTA 粉）
  const uint16_t bg   = vhBird ? (night ? VH_BIRD_BAND_NIGHT : VH_BIRD_BAND_DAY)
                               : (night ? VH_NIGHT_BG   : VH_DAY_BG);
  const uint16_t acc  = vhBird ? VH_BIRD_ROUTE
                               : (night ? VH_NIGHT_ACC  : VH_DAY_ACC);
  const uint16_t ink  = vhBird ? (night ? (uint16_t)0xFFFF : (uint16_t)0x2104)
                               : (night ? VH_NIGHT_INK  : VH_DAY_INK);
  const uint16_t sub  = vhBird ? (night ? VH_NIGHT_SUB : (uint16_t)0x8410)
                               : (night ? VH_NIGHT_SUB  : VH_DAY_SUB);
  const uint16_t road = vhBird ? vhTiltRoad()
                               : (night ? VH_NIGHT_ROAD : VH_DAY_ROAD);
  const uint16_t green= night ? VH_NIGHT_GREEN: VH_DAY_GREEN;
  const uint16_t water= night ? VH_NIGHT_WATER: VH_DAY_WATER;
  const uint16_t pill = night ? VH_NIGHT_PILL : VH_DAY_PILL;
  const uint16_t pillI= night ? VH_NIGHT_PILLI: VH_DAY_PILLI;
  const uint16_t ring = night ? VH_NIGHT_RING : (vhBird ? (uint16_t)0xC618 : VH_DAY_PILL);
  const bool over = hasCam && camLimit > 0 && speedKmh > camLimit;
  vhTilt = night ? (VH_TILT_NIGHT != 0) : (VH_TILT_DAY != 0);   // musicA.130：昼夜各自可开关透视

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
  if (!(vhTilt && night)) {                         // musicA.130：夜间透视版无细环/弧（视频 2），白天保留；超速红边始终保留
    fillRingBand(0, 1, 225, 227, ring);
    // musicA.55：固定参考图 37% 圈，不再随 navDist 跳变（过路口重置曾让弧与车头一起乱跳）。
    const float q0 = 190.0f / 360.0f, q1 = q0 + 0.37f;
    const uint16_t arcCol = over ? VH_RED : acc;
    fillRingBand(q0, q1, 222, 228, arcCol);
    aaDisc(CX + 225 * sinf(q0 * 2 * PI), CX - 225 * cosf(q0 * 2 * PI), 3, arcCol);
    aaDisc(CX + 225 * sinf(q1 * 2 * PI), CX - 225 * cosf(q1 * 2 * PI), 4.5f, VH_RED);   // 视频：弧末端红点
  }
  if (over) fillRingBand(0, 1, 228, 233, VH_RED);   // 超速红边（沿用上一稿约定）

  // 超大转向箭头 + 距离数字（左对齐 x=195）
  vhBigArrow(navDir, ink);   // musicA.131 GTA HUD：转向箭头白/夜白
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
  if (laneN <= 0) {                              // musicA.130 路名行（与车道胶囊同一位置，二选一）
    vhFadeRect(88, 196, 290, 36, bg, 55);        // 轻度让位，路网仍隐约可见
    drawTextImgC(0, 198, 0xE8, ink);             // slot 0 = 手机一直在发的路名位图
  }

  // 底部：限速牌 + 大字车速 + 时间 · 剩余里程
  if (hasCam && camLimit > 0) {
    gfx->fillCircle(160, 386, 25, VH_RED);
    gfx->fillCircle(160, 386, 20, C_WHITE);
    String lim = String(camLimit);
    printCentered(lim, 160, 374, 3, 0x0884);   // 限速牌内数字保持深墨
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

// ================= 样式16「昼夜通透」（id 16）：musicA.150 按参考稿逐像素复刻 =================
// 复用 MapFrame 的实时栅格/矢量地图、GPS 位姿、路线、红绿灯和终点数据，仅在其上重排 HUD。
// 坐标系 466，全部量自参考稿（_refd466.png / _refn466.png，昼夜同一套版式）：
//   光条 R216.5 宽 5、头端（红点）-21.5°、满长 125° | 时间 cap15 顶 33、中心 x238
//   大数字 cap89：字面左 213（格左 207）、顶 71、基线 160，格宽 41 | 单位 m x 高 22、基线 160
//   箭头框 x117..195 / y64..161 | 车道 y168..209、格宽 44、组中心 x239
//   车速圆 (76,222) r32.5 | 红徽 (395,227.5) r26 前方测速距离 | 限速 (379.5,297.5) | 指南针 (92,296) r16.5
//   musicA.159：外圈只保留「倒计弧」这一条（删除 musicA.151 加的右侧行程弧）——
//     真机参考照里倒计弧从左侧 8 点位绕过顶部、一直铺到右下 4 点位的红点，跨度约 218°，右侧没有第二条弧
//   路名中心 y364 / 底部胶囊 x143..323 / y393..423（musicA.159 两者一起下移 12px，对齐真机照）
// 字体：Bahnschrift（DIN 风格）烘焙字形（GlyphAtlas147.h：X/A/S/M/U/B/F 档）。
// 参考稿的 HUD 竖列（时间/大数字/路名/底部胶囊）中心在 x≈238，比屏幕中心偏右 5px；左右徽章按原坐标放。
#define DN_HUD_CX (CX + 5)
// musicA.153：渐变圆盘/胶囊（参考稿图标内部是「顶亮底暗」渐变，纯色显扁平）。
// colTop/colBot = 顶/底 RGB565；alpha 为整体不透明度；边缘按覆盖率抗锯齿。
// musicA.156 帧率优化：圆盘逐行的「覆盖半宽」只随 r 与 dy 变、同一行内是矩形；
// 原实现对每个像素算 sqrtf（车速圆 r30.5≈2900px + 红徽 r25.5≈2000px/帧，白耗）。
// 改为每行只算一次 sqrtf（求 dy 处的半宽），行内直接填；垂直渐变色同样每行一次 smMix。
static void dnGradDisc(float cx, float cy, float r, uint16_t colTop, uint16_t colBot, int alpha) {
  uint16_t *fb = gfx->getFramebuffer();
  if (!fb || r <= 0) return;
  const int ir = (int)ceilf(r);
  int x0 = max(0, (int)floorf(cx - r - 1)), x1 = min(LCD_WIDTH - 1, (int)ceilf(cx + r + 1));
  int y0 = max(0, (int)floorf(cy - r - 1)), y1 = min(LCD_HEIGHT - 1, (int)ceilf(cy + r + 1));
  const float d = 2.0f * r;
  for (int y = y0; y <= y1; y++) {
    const float dy = y + 0.5f - cy;
    const float ady = fabsf(dy);
    if (ady > r) continue;
    float t = (dy + r) / d; if (t < 0) t = 0; else if (t > 1) t = 1;
    const uint16_t col = smMix(colTop, colBot, (int)(t * 255.0f + 0.5f));
    const int halfW = (int)(sqrtf(r * r - ady * ady) + 0.5f);   // 每行仅 1 次 sqrtf
    int xa = max(x0, (int)ceilf(cx - halfW)), xb = min(x1, (int)floorf(cx + halfW));
    if (xb < xa) continue;
    uint16_t *row = fb + (size_t)y * LCD_WIDTH;
    for (int x = xa; x <= xb; x++) row[x] = blend565(row[x], col, alpha);
  }
  (void)ir;
}
static void dnGradPill(int x0, int y0, int w, int h, int rad, uint16_t colTop, uint16_t colBot, int alpha) {
  uint16_t *fb = gfx->getFramebuffer();
  if (!fb || w <= 0 || h <= 0) return;
  if (rad > h / 2) rad = h / 2;
  if (rad > w / 2) rad = w / 2;
  for (int y = 0; y < h; y++) {
    const float dy = y + 0.5f;
    float t = dy / h; if (t < 0) t = 0; else if (t > 1) t = 1;
    const uint16_t col = smMix(colTop, colBot, (int)(t * 255.0f + 0.5f));
    int ins = 0;
    if (y < rad)           { int d2 = rad - 1 - y; ins = rad - (int)(sqrtf((float)(rad * rad - d2 * d2)) + 0.5f); }
    else if (y >= h - rad) { int d2 = y - (h - rad); ins = rad - (int)(sqrtf((float)(rad * rad - d2 * d2)) + 0.5f); }
    if (ins < 0) ins = 0;
    const int ry = y0 + y; if (ry < 0 || ry >= LCD_HEIGHT) continue;
    uint16_t *row = fb + (size_t)ry * LCD_WIDTH;
    for (int x = x0 + ins; x < x0 + w - ins; x++) {
      if (x < 0 || x >= LCD_WIDTH) continue;
      row[x] = blend565(row[x], col, alpha);
    }
  }
}
static void dnGlassPill(int x0, int y0, int w, int h, int r, uint16_t col, int a) {
  uint16_t *fb = gfx->getFramebuffer();
  if (!fb || w <= 0 || h <= 0) return;
  if (r > h / 2) r = h / 2;
  if (r > w / 2) r = w / 2;
  for (int y = 0; y < h; y++) {
    int ins = 0;
    if (y < r)            { int dy = r - 1 - y; ins = r - (int)(sqrtf((float)(r * r - dy * dy)) + 0.5f); }
    else if (y >= h - r)  { int dy = y - (h - r); ins = r - (int)(sqrtf((float)(r * r - dy * dy)) + 0.5f); }
    if (ins < 0) ins = 0;
    const int ry = y0 + y;
    if (ry < 0 || ry >= LCD_HEIGHT) continue;
    uint16_t *row = fb + (size_t)ry * LCD_WIDTH;
    for (int x = x0 + ins; x < x0 + w - ins; x++) {
      if (x < 0 || x >= LCD_WIDTH) continue;
      row[x] = blend565(row[x], col, a);
    }
  }
}

// 半透明抗锯齿圆盘：圆内像素向 col 压 alpha（0..255），边缘按覆盖率过渡
static void dnDiscA(float cx, float cy, float r, uint16_t col, int alpha) {
  uint16_t *fb = gfx->getFramebuffer();
  if (!fb) return;
  int x0 = max(0, (int)floorf(cx - r - 1)), x1 = min(LCD_WIDTH - 1, (int)ceilf(cx + r + 1));
  int y0 = max(0, (int)floorf(cy - r - 1)), y1 = min(LCD_HEIGHT - 1, (int)ceilf(cy + r + 1));
  for (int y = y0; y <= y1; y++) {
    const float dy = y + 0.5f - cy;
    uint16_t *row = fb + (size_t)y * LCD_WIDTH;
    for (int x = x0; x <= x1; x++) {
      const float dx = x + 0.5f - cx;
      const float a = r - sqrtf(dx * dx + dy * dy) + 0.5f;
      if (a <= 0) continue;
      const int t = a >= 1 ? alpha : (int)(a * alpha);
      if (t <= 0) continue;
      row[x] = blend565(row[x], col, t);
    }
  }
}

// 剩余分钟（到达时刻 - 当前时间）；无 ETA 时 -1
static int dnGlassEtaMin() {
  if (tripEta <= 0) return -1;
  int Y, Mo, D, H, Mi, Se;
  clkNow(Y, Mo, D, H, Mi, Se);
  int emin = ((tripEta / 100) * 60 + (tripEta % 100)) - (H * 60 + Mi);
  if (emin < 0) emin += 1440;
  return emin;
}

// 外圈「路口倒计时」光条（musicA.150 重做）。
// musicA.149 的问题：半径 185、每 1.1° 打一个 r=3 的点（又细又虚），长度按 navDist/500 算、超过 500m 恒满，
// 所以实机上「看着不动」。现在：
//   · 半径 218、宽 5，抗锯齿弧 + 尾端圆头，颜色 (147,229,253)；
//   · 头端（红点）在右下 +118°、尾端左下 -100°，满长 218°（musicA.159 按真机照重定）；
//     进入 500m 倒计时后长度 = 218°×(1-进度) 平滑收短（复用 ringShown），视觉上是尾端向头端收；
//   · 一段 18° 的白色流光每 2.6s 从尾端流向头端，满长时也看得出在动；
//   · ≤120m 红点呼吸放大。
static void dnGlassCountRing() {
  // musicA.159：按真机参考照重定角度 —— 红点（头端）在右下 +118°，尾端在左下 -100°，满长 218°。
  // （.150 当时只复刻了参考稿左侧可见的那 125°，实机照片里弧是绕过顶部一直拉到右侧的。）
  // arcBand 的扇形裁剪要求单段跨度 <180°，所以 218° 弧在 A1-88° 处切成两段画，视觉上连续。
  const float R = 216.5f, A1 = 118.0f, MAXL = 218.0f;
  const float rIn = R - 2.5f, rOut = R + 2.5f;
  const uint16_t cBar = 0x973F;
  float len = MAXL;
  if (ringActive) { len = MAXL * (1.0f - ringShown); if (len < 2.0f) len = 0; }
  if (len > 0) {
    const float a0 = A1 - len;
    if (len <= 176.0f) {
      arcBand(a0, A1, rIn, rOut, cBar);
    } else {
      const float am = A1 - 88.0f;                  // 每段 <180°
      arcBand(a0, am + 1.5f, rIn, rOut, cBar);      // 两段重叠 1.5°：否则接缝处两边 AA 各画一半会留暗缝
      arcBand(am - 1.5f, A1, rIn, rOut, cBar);
    }
    const float a0r = a0 * PI / 180.0f;
    aaDisc(CX + R * sinf(a0r), CX - R * cosf(a0r), 2.5f, cBar);     // 尾端圆头
    // 流光
    const float hs = 18.0f;
    const float ph = (float)(millis() % 2600) / 2600.0f;   // .159 弧长了 1.74×，线速度保持原样
    const float c = a0 - hs + (len + 2 * hs) * ph;
    static const int ha[5] = {30, 90, 170, 90, 30};
    for (int k = 0; k < 5; k++) {
      float s0 = c - hs / 2 + hs * k / 5, s1 = s0 + hs / 5;
      if (s0 < a0) s0 = a0;
      if (s1 > A1) s1 = A1;
      if (s1 - s0 > 0.05f) arcBand(s0, s1, rIn, rOut, 0xFFFF, ha[k]);
    }
  }
  const float a1r = A1 * PI / 180.0f;
  float dr = 4.5f;
  if (navDist > 0 && navDist <= 120)
    dr = 4.5f + 1.8f * (0.5f + 0.5f * sinf((float)(millis() % 900) * (2 * PI / 900.0f)));
  aaDisc(CX + R * sinf(a1r), CX - R * cosf(a1r), dr, 0xEA89);       // 头端红点 (230,83,77)
}

// musicA.159：这里原来还有一条「总行程」右弧（musicA.151 加的：右上 +21.5° 铺 125°、底弧 + 绿进度 + 白点）。
// 参考稿/真机照里右侧并没有这条弧，而且它和左侧倒计弧在视觉上连成一条，容易被读成「倒计距离延伸到了右边」。
// 行程信息底部胶囊（xx 分 · x.x km）已经给了，弧本身冗余；顺带省掉 3 条 arcBand 的绘制开销。

// 车速：半透明深青圆 + 1.5px 浅灰描边；"58"（cap 24）上半、"km/h"（x 高 10）下半。
// musicA.149 文字溢出圆外的根因：数字档 cap 26、单位档 cap 19，塞不进 r29 的圆。
static void dnGlassSpeedBadge(uint16_t ink, uint16_t sub, bool over, int ccx, int ccy) {
  // musicA.152：参考稿是「浅灰细描边 + 深青半透明填充」，原来 33.5 描边被 32.5 填充盖住只剩 1px。
  aaDisc(ccx, ccy, 33.5f, nightOn ? 0xF79E : 0xC618);               // 浅灰描边
  // musicA.153：参考稿车速圆内部「顶亮底暗」渐变（顶 0x74B2 / 底 0x0AAB），原来纯色显扁平。
  dnGradDisc(ccx, ccy, 30.5f, nightOn ? 0x2B58 : 0x3BEE, nightOn ? 0x0C67 : 0x12CB, 215);
  const String spd = String(max(0, speedKmh));
  drawGlyphShadowS(SM_SLOT_SPEED, GA_B, spd, ccx, ccy - 22, GA_CENTER, 1, 3, 3, 170);
  drawGlyphText(GA_B, spd, ccx, ccy - 22, over ? 0xF9C5 : ink, GA_CENTER);
  drawGlyphText(GA_F, "km/h", ccx, ccy + 5, sub, GA_CENTER);
}

// 右侧两枚圆徽：上=前方电子眼距离（musciA.154 改，原剩余分钟与底部胶囊重复），下=限速（白盘 + 红环 + 黑字）
static void dnGlassRightBadges(int bx) {
  // musicA.154：红徽改显示「距前方测速点还有多少米」。camDist/camLimit 由 iOS #k 每 0.2s 下发，
  // 经过电子眼后 iOS 发 #k 清空 → hasCam=false，此时显示 "--" 占位，避免空圆。
  {
    aaDisc(bx, 228, 28.0f, 0xDBD0);
    dnGradDisc(bx, 228, 25.5f, 0x4862, 0x2821, 255);      // 顶亮底暗深红渐变（参考稿采样）
    if (hasCam && camDist > 0) {
      if (camDist >= 1000) {
        // 4 位米数会横向顶到描边（GA_B 4 字符 = 44px > 圆内径），改显 km 小数（GA_F 支持 '.'）
        const String kms = String(camDist / 1000.0, 1);
        drawGlyphShadowS(SM_SLOT_SPEED, GA_F, kms, bx, 213, GA_CENTER, 1, 2, 2, 150);
        drawGlyphText(GA_F, kms, bx, 213, 0xFFFF, GA_CENTER);
        drawGlyphText(GA_F, "km", bx, 233, 0xEF9D, GA_CENTER);
      } else {
        drawGlyphShadowS(SM_SLOT_SPEED, GA_B, String(camDist), bx, 212, GA_CENTER, 1, 2, 2, 150);
        drawGlyphText(GA_B, String(camDist), bx, 212, 0xFFFF, GA_CENTER);
        drawGlyphText(GA_F, "m", bx, 233, 0xEF9D, GA_CENTER); // 整体上移，底部不再压描边
      }
    } else {
      drawGlyphText(GA_F, "--", bx, 216, 0xEF9D, GA_CENTER);
    }
  }
  if (hasCam && camLimit > 0) {
    aaDisc(380, 298, 22.5f, 0xD227);
    aaDisc(380, 298, 19.5f, 0xFFFF);
    drawGlyphText(GA_B, String(camLimit), 380, 288, 0x0000, GA_CENTER);
  }
}

// 左下指南针徽：深色圆 + 浅灰描边 + 红白指针（随地图转向），下面挂 "N" 小牌（取代原来顶部的 N 标记）
static void dnGlassCompass(float hdg) {
  const float cx = 92, cy = 296;
  aaDisc(cx, cy, 17.0f, 0xC618);
  aaDisc(cx, cy, 15.5f, 0x1165);
  const float a = hdg * PI / 180.0f;
  const float nx = -sinf(a), ny = -cosf(a), px = -ny, py = nx;
  const float L = 10.5f, Wd = 3.0f, T = 8.5f;
  gfx->fillTriangle(cx + nx * L, cy + ny * L, cx + px * Wd, cy + py * Wd, cx - px * Wd, cy - py * Wd, 0xD227);
  gfx->fillTriangle(cx - nx * T, cy - ny * T, cx + px * Wd, cy + py * Wd, cx - px * Wd, cy - py * Wd, 0xEF9D);
  gfx->fillCircle(rnd(cx), rnd(cy), 2, 0xEF9D);
  dnGlassPill(rnd(cx) - 12, 313, 24, 16, 8, 0x0988, 230);
  gfx->setTextSize(1);
  gfx->setTextColor(0xFFFF);
  gfx->setCursor(rnd(cx) - 3, 317);
  gfx->print("N");
}

// 底部「14分 · 6.2km」：剩余分钟（到达时刻减当前时间）+ 剩余里程
static String dnGlassFooterText() {
  int emin = dnGlassEtaMin();
  String s;
  if (emin >= 0 && tripRemainM > 0)      s = String(emin) + "# * " + distText(tripRemainM);   // '#' 字形就是「分」
  else if (tripRemainM > 0)              s = distText(tripRemainM);
  else                                   s = "--";
  return gaAscii(s);
}

// 底部：深青半透明胶囊 x143..323 / y393..423（musicA.159：整体下移 12px 对齐真机照，原 381..411）
static void dnGlassFooter(uint16_t ink) {
  String t = dnGlassFooterText();
  dnGlassPill(CX - 91, 392, 182, 32, 16, 0x4A52, 90);                 // 浅色细描边
  // musicA.153：参考稿胶囊内部深青渐变（顶 0x0738 / 底 0x012E）
  dnGradPill(CX - 90, 393, 180, 30, 15, nightOn ? 0x0145 : 0x01C8, nightOn ? 0x00C3 : 0x0166, 225);
  drawGlyphShadowS(GA_SM_TIME, GA_F, t, CX, 399, GA_CENTER, 1, 2, 2, 120);
  drawGlyphText(GA_F, t, CX, 399, ink, GA_CENTER);
}

// 样式16 车道（参考稿）：推荐车道 = 深色圆角方块 (11,50,68) + 白色方向箭头；
// 非推荐车道不画底、箭头浅灰。格宽 41、高 42、y 168。与 drawLanesAt 同一套抗锯齿字形缓存，但去掉车道间的虚线。
static void dnGlassLanes(float cx, float y0, float cell, float h) {
  if (laneN <= 0) return;
  if (!laneCol) {
    laneCol = (uint16_t *)ps_malloc(LANE_BUF_W * LANE_BUF_H * 2);
    if (!laneCol) laneCol = (uint16_t *)malloc(LANE_BUF_W * LANE_BUF_H * 2);
    laneA = (uint8_t *)ps_malloc(LANE_BUF_W * LANE_BUF_H);
    if (!laneA) laneA = (uint8_t *)malloc(LANE_BUF_W * LANE_BUF_H);
    if (!laneCol || !laneA) return;
  }
  float lw = fminf(cell, 200.0f / laneN);
  float x0 = cx - lw * laneN / 2;
  for (int i = 0; i < laneN; i++)
    if (laneFr[i] != 0xF) {
      const int px = rnd(x0 + lw * i + 2), py = (int)y0, pw = rnd(lw - 4), ph = (int)h;
      if (nightOn) dnGlassPill(px - 1, py - 1, pw + 2, ph + 2, 10, 0xADF8, 210);   // 夜景：浅灰蓝描边 (170,190,195)
      dnGradPill(px, py, pw, ph, 9, nightOn ? 0x09E8 : 0x12CC, nightOn ? 0x0146 : 0x0988, 255); // musicA.153 顶亮底暗渐变
    }
  primN = 0; recIcon = true;
  for (int i = 0; i < laneN; i++) {
    float lx = x0 + lw * (i + 0.5f);
    bool rec = laneFr[i] != 0xF;
    drawLaneGlyph(lx, y0 + 7, 28, laneFlags(laneBg[i]), rec ? 0xFFFF : 0xEF9E);   // 参考稿：非推荐车道是浅灰白细箭头
    if (rec) drawLaneGlyph(lx, y0 + 7, 28, laneFlags(laneFr[i]), 0xFFFF);
  }
  recIcon = false;
  uint32_t hs = 2166136261u;
  const uint8_t *bytes = (const uint8_t *)prims;
  for (size_t i = 0; i < sizeof(Prim) * primN; i++) { hs ^= bytes[i]; hs *= 16777619u; }
  hs ^= primN * 7919u;
  if (hs != laneHash) { laneRender(); laneHash = hs; }
  primN = 0;
  uint16_t *fb = gfx->getFramebuffer();
  if (!fb) return;
  for (int y = 0; y < laneBH; y++) {
    uint16_t *dst = fb + (size_t)(laneBY + y) * LCD_WIDTH + laneBX;
    const uint16_t *sc = laneCol + y * LANE_BUF_W;
    const uint8_t *sa = laneA + y * LANE_BUF_W;
    for (int x = 0; x < laneBW; x++) {
      uint8_t a = sa[x];
      if (!a) continue;
      dst[x] = a == 255 ? sc[x] : blend565(dst[x], sc[x], a);
    }
  }
}

// musicA.157：分段计时宏（微秒），配合 MapFrame.h 的 hudTAcc
#define HUDT_MARK(i) do { uint32_t _hn = micros(); hudTAcc[i] += (uint32_t)(_hn - _hudTk); _hudTk = _hn; } while (0)
static void drawDayNightGlassHud() {
  uint32_t _hudTk = micros();
  hudTFrames++;
  const bool night = nightOn;
  const uint16_t ink   = 0xFFFF;
  const uint16_t sub   = night ? 0xC618 : 0xEF9D;        // 单位 / km/h：浅灰白
  const bool over = hasCam && camLimit > 0 && speedKmh > camLimit;
  const bool cross = crossReady();

  int yy, mm, dd, hh, mi, ss;
  clkNow(yy, mm, dd, hh, mi, ss);
  char timeBuf[8];
  snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", hh, mi);
  const String ts = String(timeBuf);

  String d = distText(navDist), num = d, unit = "";
  if (d.endsWith("km"))      { num = d.substring(0, d.length() - 2); unit = "km"; }
  else if (d.endsWith("m"))  { num = d.substring(0, d.length() - 1); unit = "m"; }

  if (cross) {
    // 路口放大图态：浮窗固定在 (97,96)-(369,246)，HUD 全部让到浮窗上/下
    drawGlyphShadowS(GA_SM_TIME, GA_M, ts, DN_HUD_CX, 12, GA_CENTER, 1, 2, 2, 120);
    drawGlyphText(GA_M, ts, DN_HUD_CX, 12, ink, GA_CENTER);
    // musicA.152：路口放大图时箭头 + 距离作为一组整体在顶部居中（原来偏左压在浮窗左上角）。
    const int aw = 46, nw = gaWidth(GA_A, num), uw = gaWidth(GA_F, unit);
    const int total = aw + 8 + nw + 4 + uw;
    const int gx0 = DN_HUD_CX - total / 2;
    drawGlassArrowAt(navDir, ink, gx0 + aw / 2, 46, 90, 46, 3, 130);
    const int nx0 = gx0 + aw + 8;
    drawGlyphShadowS(GA_SM_NUM, GA_A, num, nx0, 56, GA_LEFT, 1, 2, 2, 140);
    drawGlyphText(GA_A, num, nx0, 56, ink, GA_LEFT);
    drawGlyphText(GA_F, unit, nx0 + nw + 4, 64, sub, GA_LEFT);
    dnGlassSpeedBadge(ink, sub, over, 62, 256);
    dnGlassRightBadges(402);
  } else {
    // ---- 常态 ----
    // 时间：M 档（cap 15），顶 33、按 HUD 竖列居中
    // musicA.150：参考稿时间「16:18」字面 x219..258 / y34..47（cap 15），HUD 竖列中心 x≈238
    drawGlyphShadowS(GA_SM_TIME, GA_M, ts, DN_HUD_CX, 33, GA_CENTER, 1, 3, 3, 165);
    drawGlyphText(GA_M, ts, DN_HUD_CX, 33, ink, GA_CENTER);
    HUDT_MARK(0);

    // 大距离：X 档（Bahnschrift Bold Condensed，cap 89、格宽 41），字面左 212、顶 71；影向右下 (2,4)、羽化 3
    const int nx = 207, ny = 71;
    int nw = gaWidth(GA_X, num);
    if (nw > 4 * 41 + 20) nw = 4 * 41 + 20;
    drawGlyphShadowS(GA_SM_NUM, GA_X, num, nx, ny, GA_LEFT, 2, 5, 3, 185);
    HUDT_MARK(1);
    // 单位：S 档（"m" x 高 22），跟在数字后 6px，基线 160（"m" 的 ink 顶在档位顶 +12）
    const int ux = nx + nw + 6, uy = 160 - 33;
    drawGlyphShadowS(GA_SM_UNIT, GA_S, unit, ux, uy, GA_LEFT, 1, 3, 2, 120);
    drawGlyphText(GA_S, unit, ux, uy, sub, GA_LEFT);
    drawGlyphText(GA_X, num, nx, ny, ink, GA_LEFT);
    HUDT_MARK(2);

    // 转向箭头：细笔画 + 大圆角肘 + V 形头，框 x117..195 / y64..161，真羽化 + 软投影
    drawGlassArrowAt(navDir, ink, 156, 65.5f, 160.5f, 82, 4, 150);
    HUDT_MARK(3);

    dnGlassSpeedBadge(ink, sub, over, 76, 222);
    HUDT_MARK(4);
    dnGlassRightBadges(395);
    HUDT_MARK(5);
  }

  dnGlassFooter(ink);                                   // 先画胶囊，路名压在胶囊上沿之上（参考稿 z 序）
  // 中文路名（iOS 下发位图，粗体）：中心 y364（musicA.159 随胶囊一起下移 12px 对齐真机照），白字 + 右下软投影
  if (imgs[0].show && imgs[0].showW) drawTextImgGlass(0, 364 - imgs[0].showH / 2, ink, 2, 3, 170, DN_HUD_CX);
  HUDT_MARK(6);
  dnGlassCompass(viewHdg);

  // 车道推荐：y 168..209、格宽 41、组中心 x 240.5；musicA.151 常显（之前只在 ringActive 时画，实机整段消失）
  if (!cross && laneN > 0) dnGlassLanes(240.5f, 168, 41, 42);
  HUDT_MARK(7);

  dnGlassCountRing();
  HUDT_MARK(8);
  needFlush = true;
}

// 按当前全部状态画一整帧导航画面
void drawNavFrame() {
  onWaitScreen = false;                                 // v3.6 离开首页，猫猫不再动
  // ui.7 / musicA.132：矢量简图 + 矢量3D鸟瞰 走独立排版，必须早于 mapModeActive() 判定。
  if (isVectorHeroTheme()) { drawVectorHeroFrame(); return; }
  // ui.6：第 6 种「大字转向」走独立排版，必须早于 mapModeActive() 判定。
  if (themeIdx == ST_BIGTURN) { drawBigTurnFrame(); return; }
  if (mapModeActive()) {
    drawMapFrame();
    if (themeIdx == ST_DAYNIGHT_GLASS) drawDayNightGlassHud();   // musicA.150：按参考稿复刻；路口态也画，HUD 自己避让浮窗
    return;
  }      // v2.0 小地图
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
