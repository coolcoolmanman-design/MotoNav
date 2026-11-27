#pragma once
// 路线接收、进度和路线绘制。由 MotoNav.ino 按依赖顺序包含，仅编译一次。
void parseRoute(const uint8_t *p, uint32_t len) {
  routeSegmentCache.clear();
  routeN = 0;
  if (len < 12) return;
  int frame = rdu16(p);
  int32_t x = rd32(p + 2), y = rd32(p + 6);
  int n = rdu16(p + 10);
  if (n > ROUTE_PTS) n = ROUTE_PTS;
  if (n < 1 || 12 + (uint32_t)(n - 1) * 4 > len) return;
  routeX[0] = x; routeY[0] = y;
  for (int i = 1; i < n; i++) {
    x += rd16(p + 12 + 4 * (i - 1)); y += rd16(p + 14 + 4 * (i - 1));
    routeX[i] = x; routeY[i] = y;
  }
  // 每 32 个点一组算外框（相邻组共用端点，保证线段完整）
  for (int b = 0; b * RB_STEP < n - 1; b++) {
    int s = b * RB_STEP, e = min(n - 1, s + RB_STEP);
    int32_t x0 = INT32_MAX, y0 = INT32_MAX, x1 = INT32_MIN, y1 = INT32_MIN;
    for (int i = s; i <= e; i++) {
      x0 = min(x0, routeX[i]); x1 = max(x1, routeX[i]);
      y0 = min(y0, routeY[i]); y1 = max(y1, routeY[i]);
    }
    rbBox[4 * b] = x0; rbBox[4 * b + 1] = y0; rbBox[4 * b + 2] = x1; rbBox[4 * b + 3] = y1;
  }
  routeN = n; routeFrame = frame;
  routeProg = -1; routeSearchAt = 0;                  // v3.0 新路线：重新找车的位置
  if (routeCum) {                                     // v5.0 路程表（路况颜色用）
    routeCum[0] = 0;
    for (int i = 1; i < n; i++) {
      float dx = routeX[i] - routeX[i - 1], dy = routeY[i] - routeY[i - 1];
      routeCum[i] = routeCum[i - 1] + sqrtf(dx * dx + dy * dy);
    }
  }
  if (routeCum && routeCurv) {                        // v6.1 弯急程度：前后各 20 米之间转了多少度
    const float WIN = 20;
    int j0 = 0, j1 = 0;
    for (int i = 0; i < n; i++) {
      while (j0 < i && routeCum[i] - routeCum[j0 + 1] >= WIN) j0++;
      if (j1 < i) j1 = i;
      while (j1 < n - 1 && routeCum[j1] - routeCum[i] < WIN) j1++;
      uint8_t cls = 0;
      if (j0 < i && j1 > i) {
        float h0 = atan2f(routeY[i] - routeY[j0], routeX[i] - routeX[j0]);
        float h1 = atan2f(routeY[j1] - routeY[i], routeX[j1] - routeX[i]);
        float d = fabsf(h1 - h0);
        if (d > PI) d = 2 * PI - d;
        float deg = d * 180 / PI;
        cls = deg < 20 ? 0 : (deg < 50 ? 1 : 2);
      }
      routeCurv[i] = cls;
    }
  }
  // v3.2 路线点后面可选：红绿灯个数(2) + 每个灯 x(4) y(4) 所在段号(2) 段内比例(1，0~255)
  lightN = 0;
  uint32_t pos = 12 + (uint32_t)(n - 1) * 4;
  if (pos + 2 <= len) {
    int ln = rdu16(p + pos); pos += 2;
    for (int i = 0; i < ln && lightN < LIGHT_MAX && pos + 11 <= len; i++, pos += 11) {
      lightX[lightN] = rd32(p + pos); lightY[lightN] = rd32(p + pos + 4);
      lightU[lightN] = rdu16(p + pos + 8) + p[pos + 10] / 255.0f;
      lightN++;
    }
  }
}

// ---- v3.0 走过的路线 ----
static inline float segLen(int k) {
  return routeSegmentCache.length(k,routeX,routeY);
}

// 路线上的点：u = 段号 + 段内比例
void routePt(float u, float &x, float &y) {
  int k = (int)u;
  if (k >= routeN - 1) { x = routeX[routeN - 1]; y = routeY[routeN - 1]; return; }
  float t = u - k;
  x = routeX[k] + (routeX[k + 1] - routeX[k]) * t;
  y = routeY[k] + (routeY[k + 1] - routeY[k]) * t;
}

// 找车在路线上的位置：平时只在当前位置附近（往回 30 米、往前 300 米）找，
// 这样掉头、绕回同一条路时不会跳到路线后面去；还没对上时才搜整条路线（最多每秒一次）
void updateRouteProgress(float cx, float cy) {
  if (routeN < 2) return;
  int k0, k1;
  if (routeProg < 0) {
    if (millis() - routeSearchAt < 1000 && routeSearchAt) return;
    routeSearchAt = millis();
    k0 = 0; k1 = routeN - 2;
  } else {
    int base = min((int)routeProg, routeN - 2);
    float acc = 0;
    k0 = base; while (k0 > 0 && acc < 30) { acc += segLen(k0 - 1); k0--; }
    acc = 0;
    k1 = base; while (k1 < routeN - 2 && acc < 300) { acc += segLen(k1); k1++; }
  }
  float best = 1e18f, bestU = -1;
  for (int k = k0; k <= k1; k++) {
    float ax = routeX[k], ay = routeY[k];
    float vx = routeX[k + 1] - ax, vy = routeY[k + 1] - ay;
    float L2 = vx * vx + vy * vy;
    float t = L2 > 0 ? ((cx - ax) * vx + (cy - ay) * vy) / L2 : 0;
    t = constrain(t, 0.0f, 1.0f);
    float ex = ax + vx * t - cx, ey = ay + vy * t - cy;
    float d2 = ex * ex + ey * ey;
    if (d2 < best) { best = d2; bestU = k + t; }
  }
  if (best > 50.0f * 50.0f) return;                   // 离路线超过 50 米（偏航中）：先不动
  routeProg = bestU;
}

void __attribute__((optimize("O3"))) mapPolyline(const float *xs, const float *ys, int n, float w, uint16_t c, bool caps);

// 画路线的一段（u 从 a 到 b），先画整段描边再画内芯
// v6.1 弯道上色：内芯按每段的弯急程度分成几截，各用绿 / 黄 / 红画
const uint16_t CURVE_COL[3] = {0x3EEB, 0xFE45, 0xF9E6};
// musicA.137：弯道上色也按地网边缘截断（与 drawRouteRange 同规则）
static void mapPolylineGroundCulled(const float *xs, const float *ys, int n, float w, uint16_t c) {
  if (n < 2) return;
  if (!t3dActive) { mapPolyline(xs, ys, n, w, c, true); return; }
  const float yEdge = t3dGroundEdgeY();
  for (int i = 1; i < n; i++) {
    float x0 = xs[i - 1], y0 = ys[i - 1], x1 = xs[i], y1 = ys[i];
    const bool aOk = y0 >= yEdge, bOk = y1 >= yEdge;
    if (!aOk && !bOk) continue;
    if (!aOk || !bOk) {
      float t = (yEdge - y0) / (y1 - y0);
      if (t < 0) t = 0; if (t > 1) t = 1;
      float xi = x0 + (x1 - x0) * t;
      if (!aOk) { x0 = xi; y0 = yEdge; }
      else      { x1 = xi; y1 = yEdge; }
    }
    float px[2] = { x0, x1 }, py[2] = { y0, y1 };
    float ww = fmaxf(1.5f, w * 0.5f * (t3dWidthK(y0) + t3dWidthK(y1)));
    mapPolyline(px, py, 2, ww, c, true);
  }
}

void drawRouteCurved(float a, float b, float cx, float cy, float sc, float cs, float sn, float viewR, float w1) {
  for (int bi = 0; bi * RB_STEP < routeN - 1; bi++) {
    int s = bi * RB_STEP, e = min(routeN - 1, s + RB_STEP);
    if (e <= a || s >= b) continue;
    const int32_t *bb = rbBox + 4 * bi;
    if (bb[2] < cx - viewR || bb[0] > cx + viewR || bb[3] < cy - viewR || bb[1] > cy + viewR) continue;
    float ua = fmaxf(a, (float)s), ub = fminf(b, (float)e);
    int m = 0, cls = -1;
    auto proj = [&](float x, float y) {
      tmpX[m] = CX + sc * ((x - cx) * cs - (y - cy) * sn); tmpY[m] = MAP_PY - sc * ((x - cx) * sn + (y - cy) * cs);
      t3dXY(tmpX[m], tmpY[m]); m++;                      // musicA.134 3D 鸟瞰
    };
    float x, y;
    routePt(ua, x, y); proj(x, y);
    cls = routeCurv[(int)floorf(ua)];
    for (int k = (int)floorf(ua) + 1; k < ub; k++) {
      proj((float)routeX[k], (float)routeY[k]);
      int c2 = routeCurv[k];
      if (c2 != cls) {                                   // 换颜色：先把这一截画掉，从这个点接着画
        if (m >= 2) mapPolylineGroundCulled(tmpX, tmpY, m, w1, CURVE_COL[cls]);
        tmpX[0] = tmpX[m - 1]; tmpY[0] = tmpY[m - 1]; m = 1;
        cls = c2;
      }
    }
    routePt(ub, x, y); proj(x, y);
    if (m >= 2) mapPolylineGroundCulled(tmpX, tmpY, m, w1, CURVE_COL[cls]);
  }
}

void drawRouteRange(float a, float b, float cx, float cy, float sc, float cs, float sn, float viewR,
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
      t3dArray(tmpX, tmpY, m);                           // musicA.134 3D 鸟瞰
      if (m < 2) continue;
      // musicA.137：透视下近粗远细；蓝线贴地画到地网/楼块平面尽头再停（插值截断，禁止天空横条/飘空）
      if (t3dActive) {
        const float baseW = pass == 0 ? w0 : w1;
        const uint16_t col = pass == 0 ? c0 : c1;
        const float yEdge = t3dGroundEdgeY();
        for (int i = 1; i < m; i++) {
          float x0 = tmpX[i - 1], y0 = tmpY[i - 1], x1 = tmpX[i], y1 = tmpY[i];
          const bool aOk = y0 >= yEdge, bOk = y1 >= yEdge;
          if (!aOk && !bOk) continue;                   // 整段在地网尽头之外 → 不画
          if (!aOk || !bOk) {                           // 跨过地网边缘：插值到边缘，只留贴地一侧
            float t = (yEdge - y0) / (y1 - y0);
            if (t < 0) t = 0; if (t > 1) t = 1;
            float xi = x0 + (x1 - x0) * t;
            if (!aOk) { x0 = xi; y0 = yEdge; }
            else      { x1 = xi; y1 = yEdge; }
          }
          float k = 0.5f * (t3dWidthK(y0) + t3dWidthK(y1));
          float ww = fmaxf(pass == 0 ? 2.0f : 1.5f, baseW * k);
          // musicA.142：高架/桥梁悬浮——落地投影 + 抬高桥面，蓝线整体浮到桥面高度。
          // musicA.144：加大抬升（28px/16px）并补「桥腹侧面」——抬升线与原地线之间填一条竖带，
          // 有了侧面才有体积感；只有 pass0（内芯）铺桥体，pass1（描边）只跟随抬高。
          const bool elev = elevCur > 0.4f;
          const float ev0 = elev ? elevCur * t3dWidthK(y0) : 0.0f;
          const float ev1 = elev ? elevCur * t3dWidthK(y1) : 0.0f;
          if (elev && (y0 - ev0 < yEdge || y1 - ev1 < yEdge)) continue;   // 桥面越过地网尽头：不画
          if (elev && pass == 0) {
            // musicA.156：参考图路线是平铺黄线、无悬浮阴影；柔化高架——去掉粗重纯色投影块，
            // 投影收窄且贴近地面色，桥腹侧面降到低对比，桥面保留但护栏细化。
            const uint16_t shC = nightOn ? 0x0628 : 0x74A6;   // 落地投影：贴近地面色（原 0x6B4D 太重）
            const uint16_t dkC = nightOn ? 0x4208 : 0xC69A;   // 桥面（比地面暗一档）
            const uint16_t sdC = nightOn ? 0x1C36 : 0x8CB6;   // 桥腹侧面：低对比（原 0x9CD3 偏亮）
            const float half = fmaxf(3.2f, ww * 1.25f);
            float sx[2] = { x0, x1 }, sy[2] = { y0 + 2.0f, y1 + 2.0f };
            mapPolyline(sx, sy, 2, half * 1.5f, shC, true);                 // 落地投影：收窄 + 只下移 2px
            float gx[4] = { x0 - half, x1 - half, x1 + half, x0 + half };
            float gy[4] = { y0 - ev0, y1 - ev1, y1, y0 };
            mapFillPoly(gx, gy, 4, sdC);                                    // 桥腹：抬升线与地面之间的侧面
            float bx[2] = { x0, x1 }, by[2] = { y0 - ev0, y1 - ev1 };
            mapPolyline(bx, by, 2, fmaxf(3.4f, ww * 2.4f), dkC, true);      // 桥面
            const uint16_t railC = nightOn ? 0x4A82 : 0xA8AE;               // 护栏：低对比细线
            float r1x[2] = { x0 - half, x1 - half }, r2x[2] = { x0 + half, x1 + half };
            mapPolyline(r1x, by, 2, 1.0f, railC, false);
            mapPolyline(r2x, by, 2, 1.0f, railC, false);
          }
          float px[2] = { x0, x1 }, py[2] = { y0 - ev0, y1 - ev1 };
          mapPolyline(px, py, 2, ww, col, true);
        }
      } else {
        mapPolyline(tmpX, tmpY, m, pass == 0 ? w0 : w1, pass == 0 ? c0 : c1, true);
      }
    }
  }
}

// v4.4 原路线：格式和导航路线一样（坐标系、起点、点数、差值），后面的红绿灯不管
void parseGhost(const uint8_t *p, uint32_t len) {
  ghostN = 0;
  if (!ghostX || len < 12) return;
  int frame = rdu16(p);
  int32_t x = rd32(p + 2), y = rd32(p + 6);
  int n = rdu16(p + 10);
  if (n > GHOST_PTS) n = GHOST_PTS;
  if (n < 2 || 12 + (uint32_t)(n - 1) * 4 > len) return;
  ghostX[0] = x; ghostY[0] = y;
  for (int i = 1; i < n; i++) {
    x += rd16(p + 12 + 4 * (i - 1)); y += rd16(p + 14 + 4 * (i - 1));
    ghostX[i] = x; ghostY[i] = y;
  }
  for (int b = 0; b * RB_STEP < n - 1; b++) {
    int s = b * RB_STEP, e = min(n - 1, s + RB_STEP);
    int32_t x0 = INT32_MAX, y0 = INT32_MAX, x1 = INT32_MIN, y1 = INT32_MIN;
    for (int i = s; i <= e; i++) {
      x0 = min(x0, ghostX[i]); x1 = max(x1, ghostX[i]);
      y0 = min(y0, ghostY[i]); y1 = max(y1, ghostY[i]);
    }
    gbBox[4 * b] = x0; gbBox[4 * b + 1] = y0; gbBox[4 * b + 2] = x1; gbBox[4 * b + 3] = y1;
  }
  ghostN = n; ghostFrame = frame;
}

// v4.4 画原路线：橙色细线，画在导航路线下面（两条重合的地方只看到导航路线，分开的地方才露出来）
void drawGhost(float cx, float cy, float sc, float cs, float sn, float viewR, float w) {
  if (ghostN < 2 || ghostFrame != posFrame) return;
  for (int bi = 0; bi * RB_STEP < ghostN - 1; bi++) {
    const int32_t *bb = gbBox + 4 * bi;
    if (bb[2] < cx - viewR || bb[0] > cx + viewR || bb[3] < cy - viewR || bb[1] > cy + viewR) continue;
    int s = bi * RB_STEP, e = min(ghostN - 1, s + RB_STEP), m = 0;
    for (int k = s; k <= e; k++) {
      float dx = ghostX[k] - cx, dy = ghostY[k] - cy;
      tmpX[m] = CX + sc * (dx * cs - dy * sn); tmpY[m] = MAP_PY - sc * (dx * sn + dy * cs); m++;
    }
    t3dArray(tmpX, tmpY, m);                             // musicA.134 3D 鸟瞰
    // musicA.139：原路线（橙线）也要按地网尽头裁切，否则会穿出地面尽头飘到雾色区
    if (m >= 2) {
      if (t3dActive) mapPolylineGroundCulled(tmpX, tmpY, m, w, styleIdx()==ST_MINIMAL ? 0x8410 : 0xFD20);
      else mapPolyline(tmpX, tmpY, m, w, styleIdx()==ST_MINIMAL ? 0x8410 : 0xFD20, true);
    }
  }
}
