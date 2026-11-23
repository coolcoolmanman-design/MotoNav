#pragma once
// 矢量简图（id 13）扩展：昼夜透视倾斜 + 近路口自动放大 + 路名行。
// musicA.131：GTA / Leonida 配色（粉虚线路线、陆地清带）；几何与 musicA.130 tilt 相同。
// 由 NavigationPage.h 在「矢量简图」段之前包含，仅编译一次。
// 依赖（均已在更早的头文件里定义）：mapPolyline、routePt、routeX/routeY/routeN、rbBox、
//   RB_STEP、tmpX/tmpY、CX、MAP_PY。
//
// 开关（可在 FirmwareConfig.h 里先 #define 覆盖）：
#ifndef VH_TILT_NIGHT
#define VH_TILT_NIGHT 1            // 1 = 夜间走透视；0 = 夜间保持平面
#endif
#ifndef VH_TILT_DAY
#define VH_TILT_DAY 1              // 1 = 白天走透视；0 = 白天保持平面
#endif
#ifndef VH_APPROACH_ZOOM
#define VH_APPROACH_ZOOM 1         // 1 = 靠近路口时按距离放大（昼夜都生效）
#endif
#define VH_ZOOM_NEAR_M   300       // 距路口多少米内开始放大
#define VH_ZOOM_MAX      1.20f     // 到路口时的最大缩放（原上限 0.85）
#define VH_TILT_PY       320       // 透视模式下车头的屏幕 y（平面模式仍是 MAP_PY）
#define VH_TILT_HORIZON  120       // 无穷远处的屏幕 y（地平线）
#define VH_TILT_D        220.0f    // 透视距离常数（像素）：越小越陡
// musicA.132 矢量3D鸟瞰：≈55°（atan((PY-HORIZON)/D)）；顶缘更早淡出
#define VH_BIRD_PY       318
#define VH_BIRD_HORIZON  96
#define VH_BIRD_D        140.0f
#define VH_BLDG_FILL_DAY 0xCE79    // #CCCCCC 不透明浅灰楼块（musicA.133 对齐确认稿）
#define VH_BLDG_EDGE_DAY 0x8410    // #808080 楼块描边
#define VH_BLDG_FILL_NIGHT 0x3186
#define VH_BLDG_EDGE_NIGHT 0x7BEF
#define VH_BIRD_ROAD_DAY 0xF79E    // #F2F2F2
#define VH_BIRD_BAND_DAY 0xFFFF    // #FFFFFF
#define VH_BIRD_ROAD_NIGHT 0x7413
#define VH_BIRD_BAND_NIGHT 0x1905
#define VH_BIRD_ROUTE    0x233C    // #2266E3 实心导航蓝（视频 HUD，非粉虚线）
#define VH_BIRD_CHEV_OUT 0x1B3A    // 蓝描边
#define VH_BIRD_CHEV_IN  0xFFFF    // 白填

// GTA / Leonida：粉路线；路带用陆地色清掉原蓝带；路网灰
#define VH_TILT_ACC      0xF991    // #FC3088
#define VH_TILT_BAND     0x1905    // #1C222C 夜陆地
#define VH_TILT_ROAD     0x7413    // #708098
#define VH_TILT_ACC_DAY  0xF991    // #FC3088
#define VH_TILT_BAND_DAY 0xEF5E    // #E8E8F0 昼陆地
#define VH_TILT_ROAD_DAY 0x9D15    // #98A0AC
#define VH_DEST          0xF991    // 粉目的地盘
#define VH_RETICLE_DAY   0x36DC    // #34D8E4
#define VH_RETICLE_NIGHT 0x46FD    // #40DCEC
#define VH_DASH_ON       18.0f     // 粉虚线块长度（屏像素）
#define VH_DASH_OFF      12.0f

static bool vhTilt = false;        // 每帧由 drawVectorHeroFrame 设置
static bool vhBird = false;        // musicA.132：矢量3D鸟瞰（更陡 tilt + 挤出楼）

static inline float vhTiltD() { return vhBird ? VH_BIRD_D : VH_TILT_D; }
static inline float vhTiltPy() { return vhBird ? (float)VH_BIRD_PY : (float)VH_TILT_PY; }
static inline float vhTiltHorizon() { return vhBird ? (float)VH_BIRD_HORIZON : (float)VH_TILT_HORIZON; }

static inline uint16_t vhTiltAcc()  { return nightOn ? VH_TILT_ACC  : VH_TILT_ACC_DAY; }
static inline uint16_t vhTiltBand() {
  if (vhBird) return nightOn ? VH_BIRD_BAND_NIGHT : VH_BIRD_BAND_DAY;
  return nightOn ? VH_TILT_BAND : VH_TILT_BAND_DAY;
}
static inline uint16_t vhTiltRoad() {
  if (vhBird) return nightOn ? VH_BIRD_ROAD_NIGHT : VH_BIRD_ROAD_DAY;
  return nightOn ? VH_TILT_ROAD : VH_TILT_ROAD_DAY;
}
static inline uint16_t vhReticle()  { return nightOn ? VH_RETICLE_NIGHT : VH_RETICLE_DAY; }
static inline uint16_t vhBldgFill() { return nightOn ? VH_BLDG_FILL_NIGHT : VH_BLDG_FILL_DAY; }
static inline uint16_t vhBldgEdge() { return nightOn ? VH_BLDG_EDGE_NIGHT : VH_BLDG_EDGE_DAY; }

// 平面屏幕坐标（车在 (CX, MAP_PY)、车头朝上）→ 透视屏幕坐标。返回该点的线宽缩放 k。
static inline float vhTiltPt(float &x, float &y) {
  float u = (float)MAP_PY - y;
  const float d = vhTiltD();
  const float uMin = -0.6f * d;
  if (u < uMin) u = uMin;
  const float k = d / (d + u);
  x = CX + (x - CX) * k;
  y = vhTiltPy() - (vhTiltPy() - vhTiltHorizon()) * (u / (u + d));
  return k;
}

static void vhPolyTilt(float *xs, float *ys, int n, float w, uint16_t c, float minW) {
  if (n < 2) return;
  float k0 = vhTiltPt(xs[0], ys[0]);
  for (int i = 1; i < n; i++) {
    const float k1 = vhTiltPt(xs[i], ys[i]);
    const bool beyond = ys[i - 1] < vhTiltHorizon() + 4 && ys[i] < vhTiltHorizon() + 4;
    if (!beyond) {
      float ww = w * 0.5f * (k0 + k1);
      if (ww < minW) ww = minW;
      mapPolyline(xs + i - 1, ys + i - 1, 2, ww, c, true);
    }
    k0 = k1;
  }
}

// 粉虚线块：沿折线累计屏像素，on/off 交替画短段（可选透视）。
static void vhPolyDashed(float *xs, float *ys, int n, float w, uint16_t c, bool tilt) {
  if (n < 2) return;
  float phase = 0;
  for (int i = 1; i < n; i++) {
    float x0 = xs[i - 1], y0 = ys[i - 1], x1 = xs[i], y1 = ys[i];
    float dx = x1 - x0, dy = y1 - y0;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 0.5f) continue;
    float ux = dx / len, uy = dy / len;
    float t = 0;
    while (t < len - 0.01f) {
      float cycle = VH_DASH_ON + VH_DASH_OFF;
      float mod = fmodf(phase, cycle);
      if (mod < 0) mod += cycle;
      bool on = mod < VH_DASH_ON;
      float slot = on ? (VH_DASH_ON - mod) : (cycle - mod);
      if (slot > len - t) slot = len - t;
      if (on && slot > 1.2f) {
        float ax = x0 + ux * t, ay = y0 + uy * t;
        float bx = x0 + ux * (t + slot), by = y0 + uy * (t + slot);
        float px[2] = {ax, bx}, py[2] = {ay, by};
        if (tilt) vhPolyTilt(px, py, 2, w, c, 2.0f);
        else mapPolyline(px, py, 2, w, c, true);
      }
      t += slot;
      phase += slot;
    }
  }
}

static void vhRangeTilt(float a, float b, float cx, float cy, float sc, float cs, float sn, float viewR,
                        float w0, float w1, uint16_t c0, uint16_t c1) {
  if (b - a < 0.001f) return;
  for (int pass = 0; pass < 2; pass++) {
    for (int bi = 0; bi * RB_STEP < routeN - 1; bi++) {
      int s = bi * RB_STEP, e = min(routeN - 1, s + RB_STEP);
      if (e <= a || s >= b) continue;
      const int32_t *bb = rbBox + 4 * bi;
      if (bb[2] < cx - viewR || bb[0] > cx + viewR || bb[3] < cy - viewR || bb[1] > cy + viewR) continue;
      float ua = fmaxf(a, (float)s), ub = fminf(b, (float)e);
      int m = 0;
      float x, y;
      routePt(ua, x, y);
      tmpX[m] = CX + sc * ((x - cx) * cs - (y - cy) * sn); tmpY[m] = MAP_PY - sc * ((x - cx) * sn + (y - cy) * cs); m++;
      for (int k = (int)floorf(ua) + 1; k < ub; k++) {
        float dx = routeX[k] - cx, dy = routeY[k] - cy;
        tmpX[m] = CX + sc * (dx * cs - dy * sn); tmpY[m] = MAP_PY - sc * (dx * sn + dy * cs); m++;
      }
      routePt(ub, x, y);
      tmpX[m] = CX + sc * ((x - cx) * cs - (y - cy) * sn); tmpY[m] = MAP_PY - sc * ((x - cx) * sn + (y - cy) * cs); m++;
      if (m >= 2) vhPolyTilt(tmpX, tmpY, m, pass == 0 ? w0 : w1, pass == 0 ? c0 : c1, pass == 0 ? 3.0f : 1.6f);
    }
  }
}

// GTA 粉虚线路线（可选先用陆地色清带）。pass0=清带可选，pass1=虚线。
static void vhRangeDashed(float a, float b, float cx, float cy, float sc, float cs, float sn, float viewR,
                          float wClear, float wDash, uint16_t clearC, uint16_t dashC, bool tilt) {
  if (b - a < 0.001f) return;
  for (int bi = 0; bi * RB_STEP < routeN - 1; bi++) {
    int s = bi * RB_STEP, e = min(routeN - 1, s + RB_STEP);
    if (e <= a || s >= b) continue;
    const int32_t *bb = rbBox + 4 * bi;
    if (bb[2] < cx - viewR || bb[0] > cx + viewR || bb[3] < cy - viewR || bb[1] > cy + viewR) continue;
    float ua = fmaxf(a, (float)s), ub = fminf(b, (float)e);
    int m = 0;
    float x, y;
    routePt(ua, x, y);
    tmpX[m] = CX + sc * ((x - cx) * cs - (y - cy) * sn); tmpY[m] = MAP_PY - sc * ((x - cx) * sn + (y - cy) * cs); m++;
    for (int k = (int)floorf(ua) + 1; k < ub; k++) {
      float dx = routeX[k] - cx, dy = routeY[k] - cy;
      tmpX[m] = CX + sc * (dx * cs - dy * sn); tmpY[m] = MAP_PY - sc * (dx * sn + dy * cs); m++;
    }
    routePt(ub, x, y);
    tmpX[m] = CX + sc * ((x - cx) * cs - (y - cy) * sn); tmpY[m] = MAP_PY - sc * ((x - cx) * sn + (y - cy) * cs); m++;
    if (m < 2) continue;
    if (wClear > 0.5f) {
      if (tilt) vhPolyTilt(tmpX, tmpY, m, wClear, clearC, 2.0f);
      else mapPolyline(tmpX, tmpY, m, wClear, clearC, true);
    }
    // 重新投影一次给虚线（tilt 会原地改坐标）
    m = 0;
    routePt(ua, x, y);
    tmpX[m] = CX + sc * ((x - cx) * cs - (y - cy) * sn); tmpY[m] = MAP_PY - sc * ((x - cx) * sn + (y - cy) * cs); m++;
    for (int k = (int)floorf(ua) + 1; k < ub; k++) {
      float dx = routeX[k] - cx, dy = routeY[k] - cy;
      tmpX[m] = CX + sc * (dx * cs - dy * sn); tmpY[m] = MAP_PY - sc * (dx * sn + dy * cs); m++;
    }
    routePt(ub, x, y);
    tmpX[m] = CX + sc * ((x - cx) * cs - (y - cy) * sn); tmpY[m] = MAP_PY - sc * ((x - cx) * sn + (y - cy) * cs); m++;
    vhPolyDashed(tmpX, tmpY, m, wDash, dashC, tilt);
  }
}

static inline float vhApproachScale(float base) {
#if VH_APPROACH_ZOOM
  if (navDir != "S" && navDir != "" && navDist >= 0 && navDist < VH_ZOOM_NEAR_M) {
    const float t = 1.0f - (float)navDist / (float)VH_ZOOM_NEAR_M;
    const float z = 0.55f + t * (VH_ZOOM_MAX - 0.55f);
    return z > base ? z : base;
  }
#endif
  return base;
}

// musicA.132：沿路网偏置画挤出楼块（灰填 + 灰描边）。无真实楼高数据，高度由哈希定 2–4× 车道感。
static void vhDrawExtrudedBox(float bx0, float by0, float bx1, float by1,
                              float cx, float cy, float sc, float cs, float sn, float hPx) {
  float wx[4] = {bx0, bx1, bx1, bx0};
  float wy[4] = {by0, by0, by1, by1};
  float baseX[4], baseY[4], roofX[4], roofY[4];
  float kAvg = 0;
  for (int i = 0; i < 4; i++) {
    float dx = wx[i] - cx, dy = wy[i] - cy;
    baseX[i] = CX + sc * (dx * cs - dy * sn);
    baseY[i] = MAP_PY - sc * (dx * sn + dy * cs);
    float k = vhTiltPt(baseX[i], baseY[i]);
    kAvg += k;
    roofX[i] = baseX[i];
    roofY[i] = baseY[i] - hPx * k;
  }
  kAvg *= 0.25f;
  if (kAvg < 0.10f) return;   // 太远淡掉
  // musicA.133：近处不透明浅灰；远处仍略溶，避免闪烁空洞
  const uint16_t fill = blend565(vhTiltBand(), vhBldgFill(), (uint8_t)constrain((int)(kAvg * 255), 160, 255));
  const uint16_t edge = vhBldgEdge();
  // 可见侧面（底部两角）
  for (int i = 0; i < 4; i++) {
    int j = (i + 1) & 3;
    float sx[4] = {baseX[i], baseX[j], roofX[j], roofX[i]};
    float sy[4] = {baseY[i], baseY[j], roofY[j], roofY[i]};
    // 只画朝下的侧面（平均 y 更大 = 更靠屏底 = 更近）
    if (0.5f * (baseY[i] + baseY[j]) > 0.5f * (roofY[i] + roofY[j]) + 2) {
      mapFillPoly(sx, sy, 4, blend565(fill, edge, 55));
      mapPolyline(sx, sy, 4, 1.2f, edge, true);
    }
  }
  mapFillPoly(roofX, roofY, 4, fill);
  { float rx[5] = {roofX[0], roofX[1], roofX[2], roofX[3], roofX[0]};
    float ry[5] = {roofY[0], roofY[1], roofY[2], roofY[3], roofY[0]};
    mapPolyline(rx, ry, 5, 1.4f, edge, false); }
}

// musicA.133：视频式蓝/白描边 chevron（非裸白三角）
static void vhDrawBirdChevron(float x, float y) {
  // 外轮廓（蓝）→ 内填（白）→ 中缝（蓝）
  const float T[4][2] = {{0, -16}, {-12, 13}, {0, 5}, {12, 13}};
  for (int pass = 0; pass < 2; pass++) {
    float k = pass == 0 ? 1.28f : 1.0f;
    uint16_t c = pass == 0 ? VH_BIRD_CHEV_OUT : VH_BIRD_CHEV_IN;
    float yo = pass == 0 ? 1.0f : 0;
    gfx->fillTriangle(x + T[0][0] * k, y + T[0][1] * k + yo,
                      x + T[1][0] * k, y + T[1][1] * k + yo,
                      x + T[2][0] * k, y + T[2][1] * k + yo, c);
    gfx->fillTriangle(x + T[0][0] * k, y + T[0][1] * k + yo,
                      x + T[2][0] * k, y + T[2][1] * k + yo,
                      x + T[3][0] * k, y + T[3][1] * k + yo, c);
  }
  // 中缝切出 chevron 感
  gfx->fillTriangle(x, y - 2, x - 5, y + 9, x + 5, y + 9, VH_BIRD_CHEV_OUT);
}

static void vhDrawBirdBuildings(float cx, float cy, float sc, float cs, float sn, float viewR) {
  if (!vhBird || !mapValid || mapFrame != posFrame || mapFeatN <= 0) return;
  const float lx = cx - mapOx, ly = cy - mapOy;
  int drawn = 0;
  const int maxB = 110;   // musicA.133：密实网格（拒稀疏闪烁）
  for (int i = 0; i < mapFeatN && drawn < maxB; i++) {
    const int16_t *bx = featBox + 4 * i;
    if (bx[2] < lx - viewR || bx[0] > lx + viewR || bx[3] < ly - viewR || bx[1] > ly + viewR) continue;
    int np = featNp[i];
    if (np < 2) continue;
    if ((featCls[i] & 0x80) != 0) continue;   // 面要素留给绿地水面
    int c = featCls[i] & 0x7F;
    if (c < 1 || c > 3) continue;
    if (np > POLY_PTS) np = POLY_PTS;
    const uint8_t *pp = mapShow + featOff[i];
    // 沿折线密铺：左右两侧都放，间距更短
    float acc = 0;
    for (int k = 1; k < np && drawn < maxB; k++) {
      float x0 = (float)rd16(pp + 4 * (k - 1)), y0 = (float)rd16(pp + 4 * (k - 1) + 2);
      float x1 = (float)rd16(pp + 4 * k), y1 = (float)rd16(pp + 4 * k + 2);
      float dx = x1 - x0, dy = y1 - y0;
      float len = sqrtf(dx * dx + dy * dy);
      if (len < 8.0f) continue;
      acc += len;
      if (acc < 26.0f) continue;
      acc = 0;
      float ux = dx / len, uy = dy / len;
      float px = -uy, py = ux;   // 垂直
      float midX = 0.5f * (x0 + x1) + mapOx, midY = 0.5f * (y0 + y1) + mapOy;
      for (int side = -1; side <= 1 && drawn < maxB; side += 2) {
        float off = 16.0f + (float)((i * 17 + k * 13 + (side > 0 ? 7 : 0)) % 12);
        float cxB = midX + px * off * side;
        float cyB = midY + py * off * side;
        float hw = 8.0f + (float)((i * 7 + k + side) % 9);
        float hh = 6.0f + (float)((i * 11 + k * 3) % 8);
        float hPx = 16.0f + (float)((i * 3 + k * 5) % 26);   // 屏像素高 ≈ 2–4× 车道感
        vhDrawExtrudedBox(cxB - hw, cyB - hh, cxB + hw, cyB + hh, cx, cy, sc, cs, sn, hPx);
        drawn++;
      }
    }
  }
}
