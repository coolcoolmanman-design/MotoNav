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

static inline uint16_t vhTiltAcc()  { return nightOn ? VH_TILT_ACC  : VH_TILT_ACC_DAY; }
static inline uint16_t vhTiltBand() { return nightOn ? VH_TILT_BAND : VH_TILT_BAND_DAY; }
static inline uint16_t vhTiltRoad() { return nightOn ? VH_TILT_ROAD : VH_TILT_ROAD_DAY; }
static inline uint16_t vhReticle()  { return nightOn ? VH_RETICLE_NIGHT : VH_RETICLE_DAY; }

// 平面屏幕坐标（车在 (CX, MAP_PY)、车头朝上）→ 透视屏幕坐标。返回该点的线宽缩放 k。
static inline float vhTiltPt(float &x, float &y) {
  float u = (float)MAP_PY - y;
  const float uMin = -0.6f * VH_TILT_D;
  if (u < uMin) u = uMin;
  const float k = VH_TILT_D / (VH_TILT_D + u);
  x = CX + (x - CX) * k;
  y = VH_TILT_PY - (float)(VH_TILT_PY - VH_TILT_HORIZON) * (u / (u + VH_TILT_D));
  return k;
}

static void vhPolyTilt(float *xs, float *ys, int n, float w, uint16_t c, float minW) {
  if (n < 2) return;
  float k0 = vhTiltPt(xs[0], ys[0]);
  for (int i = 1; i < n; i++) {
    const float k1 = vhTiltPt(xs[i], ys[i]);
    const bool beyond = ys[i - 1] < VH_TILT_HORIZON + 4 && ys[i] < VH_TILT_HORIZON + 4;
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
