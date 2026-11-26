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

// ================= musicA.145 新增「昼夜通透」主题（id 16） =================
// 复用 MapFrame 的实时栅格/矢量地图、GPS 位姿、路线、红绿灯和终点数据；
// 仅在其上重排 HUD。日间青蓝通透，夜间深海蓝 + 金色路线。
// ---- musicA.147b：参考视频实测坐标（已按屏幕圆归一化到 466 屏）----
// 视频里屏幕圆只占裁剪图 400/466，直接套绝对坐标会整体偏下 45px；这里全部换算过：
// 时间中心 (224,87) / 大数字高 85、顶 70、中心 x 232 / 细箭头 x≈121、y72..177
// 车速徽章 圆心(52,239) r≈37（深蓝实心，无金环）/ 路名中心 387 / 底部行 418
// 左下行程小条 (82,427)-(112,411)。圆内 + 两两不重叠已由 verify_147b_layout.py 校验。
static void dnGlassSpeedBadge(float bx, float by, uint16_t ink, uint16_t sub, uint16_t base, bool over) {
  aaDisc(bx, by, 40, 0x0022);                          // 极暗外边，压住地图保证对比
  aaDisc(bx, by, 38, base);
  drawGlyphText(GA_A, String(max(0, speedKmh)), bx, by - 24, over ? C_RED : ink, GA_CENTER);
  drawGlyphText(GA_S, "km/h", bx, by + 12, sub, GA_CENTER);
}

// 底部「43 分 · 24.8 km」：剩余分钟（由到达时刻减当前时间）+ 剩余里程，无底衬
static String dnGlassFooterText() {
  int emin = -1;
  if (tripEta > 0) {
    int Y, Mo, D, H, Mi, Se;
    clkNow(Y, Mo, D, H, Mi, Se);
    emin = ((tripEta / 100) * 60 + (tripEta % 100)) - (H * 60 + Mi);
    if (emin < 0) emin += 1440;
  }
  String s;
  if (emin >= 0 && tripRemainM > 0)      s = String(emin) + "# * " + distText(tripRemainM);   // '#' 字形就是「分」
  else if (tripRemainM > 0)              s = distText(tripRemainM);
  else                                   s = "--";
  return gaAscii(s);
}

static void dnGlassFooter(float cx, int yTop, uint16_t ink) {
  drawGlyphText(GA_S, dnGlassFooterText(), cx, yTop, ink, GA_CENTER);
}

// 样式16 车道：参考图每条车道是一个深色圆角胶囊 + 白色方向箭头（裸箭头压在亮路网上看不见）
static void dnGlassLanes(float cx, float y0, float maxW, float h) {
  if (laneN <= 0) return;
  const float lw = fminf(36.0f * h / 32.0f, maxW / laneN);
  const float x0 = cx - lw * laneN / 2;
  for (int i = 0; i < laneN; i++)
    gfx->fillRoundRect((int)(x0 + lw * i + 2), (int)y0, (int)(lw - 4), (int)h, (int)(h * 0.34f), 0x0A14);
  drawLanesAt(cx, y0, maxW, h);
}

// 左下角行程小条（参考图同位置）：粉紫→青的短斜条，亮起长度 = 行程进度
static void dnGlassTripDash() {
  float f = tripFrac();
  if (f <= 0.01f) return;
  if (f > 1) f = 1;
  const float x0 = 103, y0 = 421, x1 = 127, y1 = 411;   // 参考图这条就压在圆边，允许贴边
  const int N = 26;
  int n = (int)(f * N + 0.5f);
  if (n < 2) n = 2;
  for (int i = 0; i < n; i++) {
    float t = (float)i / (N - 1);
    uint16_t c = blendF(0xF81F, 0x07FF, (int)(t * 255));   // 粉紫 → 青
    float px = x0 + (x1 - x0) * t;
    float py = y0 + (y1 - y0) * t;
    gfx->fillCircle((int)px, (int)py, 3, c);
  }
}

static void drawDayNightGlassHud() {
  const bool night = nightOn;
  const uint16_t ink   = 0xFFFF;
  const uint16_t sub   = night ? 0xC618 : 0xEF9D;
  const uint16_t badge = night ? 0x0885 : 0x0A6E;        // 参考的深蓝实心徽章
  const bool over = hasCam && camLimit > 0 && speedKmh > camLimit;
  const bool cross = crossReady();

  int yy, mm, dd, hh, mi, ss;
  clkNow(yy, mm, dd, hh, mi, ss);
  char timeBuf[8];
  snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", hh, mi);

  String d = distText(navDist), num = d, unit = "";
  if (d.endsWith("km"))      { num = d.substring(0, d.length() - 2); unit = "km"; }
  else if (d.endsWith("m"))  { num = d.substring(0, d.length() - 1); unit = "m"; }

  if (cross) {
    // 路口放大图态：浮窗固定在 (97,96)-(369,246)。
    // HUD 全部让到浮窗上/下，绝不退回旧排版：顶部一行「时间 + 箭头 + 距离」，底部照旧。
    drawGlyphShadow(GA_S, String(timeBuf), CX, 14, GA_CENTER, 2, 2, 170);
    drawGlyphText(GA_S, String(timeBuf), CX, 14, ink, GA_CENTER);
    int cw = gaWidth(GA_A, num);
    drawThinArrowAt(navDir, ink, 150, 46, 86, 40);
    drawGlyphShadow(GA_A, num, 150 + 22 + cw / 2, 52, GA_CENTER, 2, 2, 190);
    drawGlyphText(GA_A, num, 150 + 22 + cw / 2, 52, ink, GA_CENTER);
    drawGlyphText(GA_S, unit, 150 + 22 + cw + 5, 58, sub, GA_LEFT);
    dnGlassSpeedBadge(56, 272, ink, sub, badge, over);      // 让到浮窗左下（浮窗左边界 97）
    drawTextImgHaloC(0, 366, 0xE8, ink, 0x0000, 2);
    dnGlassFooter(CX, 414, ink);
    needFlush = true;
    return;
  }

  // ---- 常态：严格对齐参考视频 ----
  // 时间：软投影（羽化 2px、向下偏 2px），压在亮色路网上也能读
  drawGlyphShadow(GA_A, String(timeBuf), 224, 24, GA_CENTER, 2, 2, 170);
  drawGlyphText(GA_A, String(timeBuf), 224, 24, ink, GA_CENTER);

  // 大距离：数字顶 70（X 档 capH=85），中心 x 232；超宽时降到 A 档避免出圆
  int nw = gaWidth(GA_X, num);
  if (nw > 230) {
    nw = gaWidth(GA_A, num);
    drawGlyphShadow(GA_A, num, 232, 110, GA_CENTER, 2, 2, 190);
    drawGlyphText(GA_A, num, 232, 110, ink, GA_CENTER);
    drawGlyphText(GA_S, unit, 232 + nw / 2 + 8, 120, sub, GA_LEFT);
  } else {
    drawGlyphShadow(GA_X, num, 232, 70, GA_CENTER, 2, 2, 190);   // 参考图这层暗影很明显
    drawGlyphText(GA_X, num, 232, 70, ink, GA_CENTER);
    drawGlyphText(GA_S, unit, 232 + nw / 2 + 10, 165, sub, GA_LEFT);
  }

  // 细笔画箭头：数字左侧（参考 x≈121，y 72..177）
  drawThinArrowAt(navDir, ink, 121, 72, 177, 76);

  // 左侧车速徽章（深蓝实心圆，白数字 + 小 km/h）
  dnGlassSpeedBadge(52, 239, ink, sub, badge, over);

  // 中文路名（iOS 下发位图，中心 y≈387，带深色描边）+ 底部剩余时间/里程
  drawTextImgHaloC(0, 366, 0xE8, ink, 0x0000, 2);
  drawGlyphShadow(GA_S, dnGlassFooterText(), CX, 414, GA_CENTER, 2, 2, 150);
  dnGlassFooter(CX, 414, ink);

  // 车道推荐：透视视角下车头已经到 330，车道必须上移到数字下方（参考图 y≈180、x≈CX+30）
  if (laneN > 0 && ringActive) dnGlassLanes(CX + 30, 188, 84, 24);

  dnGlassTripDash();
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
    if (themeIdx == ST_DAYNIGHT_GLASS) drawDayNightGlassHud();   // musicA.147b：路口态也画，HUD 自己避让浮窗
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
