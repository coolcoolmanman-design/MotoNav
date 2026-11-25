#pragma once
// musicA.130：3D 鸟瞰的位图地面 + 楼块拉伸。由 RasterRenderer.h 在 drawRasterMap 之前包含。
// 每个屏幕像素独立算出颜色（只读位图、只写自己），所以可以放心按行分给两个核心。
//
// 地面：每一行屏幕对应地面上的一条横线，行内的步长是常数（和平面版一样只是多了 1/k），
//       所以每帧只需要给 466 行各算一次起点和步长。
// 楼块：位图里的楼块是「脚印」。一座高 H 像素的楼，屏幕上占据脚印整体向上平移 0..H 的区域：
//       - 平移 H 处 = 屋顶；其余 = 正面墙。
//       对屏幕像素 P，往下 H 行（该行地面点）若是楼块脚印，P 就是这座楼的屋顶；往下 H/2 行是脚印则是墙。
//       最近的楼优先（下面的行离相机更近），所以先查高楼再查矮楼。
struct T3dRow { int32_t U0, V0, DU, DV; uint16_t fog; int16_t hTall, hLow; uint8_t ok; };
static T3dRow t3dRows[LCD_HEIGHT];
// musicA.143：两个核心的分界行。[0, t3dMid) 由主核心（核心 1）画，[t3dMid, LCD_HEIGHT) 由核心 0 上的第二个任务画。
// 原来按行数对半劈（parallelRows），但近处带楼块的行（下半）比远处雾 / 天空行（上半）贵得多，而且核心 0 还要推屏 + 蓝牙，
// 于是核心 0 成了瓶颈。这里：第一帧按每行开销估一个分界，之后每帧用「上一帧两个核心各自的实际用时」反馈微调，
// 自动把活儿往空闲的核心挪（核心 0 被推屏抢占变慢时，主核心会自动多分到行）。
static int t3dMid = LCD_HEIGHT / 2;
static bool t3dMidInit = false;
static volatile uint32_t t3dTMain = 0, t3dTHelp = 0;   // 上一帧两个核心各自画底图用了多少微秒
static struct { uint16_t haze, roofT, roofTe, wallT, roofL, roofLe, wallL; bool ext; } T3dSt;

#if T3D_OWN_PALETTE
#define T3D_D_GROUND 0xEF5D
#define T3D_D_ROAD   0xBE39
#define T3D_D_PARK   0xD71A
#define T3D_D_WATER  0xCEFD
// musicA.142：白天楼顶拉到纯白（L255）压在地面 0xEF5D（L236）之上，轮廓压到深灰 L106。
// 原 0xF7BF+blendF(...,70) 在停车压平成 2D 后轮廓≈L240，与地面 L236 无法区分。
#define T3D_D_ROOF   0xFFFF
#define T3D_D_ROOF_E 0x6B4D
#define T3D_D_WALL   0x9D16
#define T3D_N_GROUND 0x18E5
#define T3D_N_ROAD   0x3A0B
#define T3D_N_PARK   0x19C6
#define T3D_N_WATER  0x11A9
#define T3D_N_ROOF   0x530F
#define T3D_N_ROOF_E 0x39E7
#define T3D_N_WALL   0x2988
#endif

// 屏幕行 → 位图采样起点/步长；同时准备本帧的颜色。必须在 RM.pal 填好之后调用。
static void t3dPrepare(float cx, float cy, float sc, float cs, float sn, bool buildings) {
#if T3D_OWN_PALETTE
  if (!rasHasPal) {
    RM.pal[0] = nightOn ? T3D_N_GROUND : T3D_D_GROUND;
    RM.pal[1] = nightOn ? T3D_N_ROAD   : T3D_D_ROAD;
    RM.pal[2] = nightOn ? T3D_N_PARK   : T3D_D_PARK;
    RM.pal[3] = nightOn ? T3D_N_WATER  : T3D_D_WATER;
    // musicA.142：停车平视（blend<0.15）时楼块不再挤出，走 idx 4..7 直接取 pal，
    // 这里必须给出与地面明确分离的楼顶/轮廓，否则轮廓≈地面亮度，看着「建筑没有轮廓」。
    RM.pal[4] = RM.pal[6] = nightOn ? T3D_N_ROOF   : T3D_D_ROOF;
    RM.pal[5] = RM.pal[7] = nightOn ? T3D_N_ROOF_E : T3D_D_ROOF_E;
  }
#endif
  const uint16_t roof = nightOn ? T3D_N_ROOF : T3D_D_ROOF, wall = nightOn ? T3D_N_WALL : T3D_D_WALL;
  // musicA.136：画质固定「均衡」密楼（对齐 musicA.134）；不再按 TQ 关掉挤出 / 远场清零
  // musicA.139：等红灯平视化时楼块随倾斜系数压平；blend<0.15 直接关楼块探测（省性能且视觉已平）
  const float blend = t3dDrawnBlend();
  T3dSt.ext = buildings && T3D_QUALITY > 0 && !rasHasPal && blend >= 0.15f;
  T3dSt.haze  = RM.pal[0];
  T3dSt.roofL = roof;                                     // 矮楼：屋顶 = 基础色
  T3dSt.roofT = nightOn ? blendF(roof, C_WHITE, 40) : roof;   // 高楼：夜间略亮；白天已是最亮
  T3dSt.roofLe = nightOn ? T3D_N_ROOF_E : T3D_D_ROOF_E;   // musicA.142：轮廓不再按 roof 混黑，直接给深灰
  T3dSt.roofTe = blendF(T3dSt.roofT, C_BLACK, 70);
  T3dSt.wallL = blendF(wall, C_BLACK, 25);                // 矮楼墙：略深
  T3dSt.wallT = blendF(wall, C_BLACK, 60);                // 高楼墙：更深，层次更明显
  const float inv = 1.0f / sc, mpp = rasMpp;
  const float span = t3dSpan();                         // musicA.139：随倾斜系数（停车→∞ 退化平面）
  const float py = t3dPy();
  for (int y = 0; y < LCD_HEIGHT; y++) {
    T3dRow &r = t3dRows[y];
    float s = (py - (y + 0.5f)) / span;                 // = u/(u+D)
    if (s >= 0.97f) { r.ok = 0; continue; }             // 地平线以上：只填雾色
    if (s < -1.5f) s = -1.5f;
    const float k = 1.0f - s;
    const float u = t3dD() * s / k;                     // 前方平面像素距离（musicA.139 随倾斜系数）
    const float ry = u * inv;                           // 前方米数
    const float lat = inv / k;                          // 每个屏幕像素对应的横向米数
    const float rx0 = 0.5f * lat;                       // x = CX 这一列的像素中心
    const float dx0 = rx0 * cs + ry * sn, dy0 = -rx0 * sn + ry * cs;
    r.U0 = (int32_t)(((cx + dx0 - rasX0) / mpp) * 65536.0f);
    r.V0 = (int32_t)(((rasY0 - (cy + dy0)) / mpp) * 65536.0f);
    r.DU = (int32_t)(cs * lat / mpp * 65536.0f);
    r.DV = (int32_t)(sn * lat / mpp * 65536.0f);
    float f = (s - 0.50f) / (0.97f - 0.50f);            // 远处渐隐到雾色（也盖住远处最近邻采样的闪烁）
    r.fog = f <= 0 ? 0 : (uint16_t)(f >= 1 ? 256 : f * 256);
    const float hk = sc * k * T3D_VK * blend;           // 此行 1 米楼高对应的屏幕像素（musicA.139 随倾斜压平）
    int ht = (int)(T3D_H_TALL_M * hk + 0.5f), hl = (int)(T3D_H_LOW_M * hk + 0.5f);
    r.hTall = (int16_t)(ht > 48 ? 48 : ht); r.hLow = (int16_t)(hl > 48 ? 48 : hl);
    r.ok = 1;
  }
  // ---- musicA.143：分核 ----
  if (!t3dMidInit || !t3dTMain || !t3dTHelp) {          // 第一帧 / 没有可用的计时：按每行估算的开销切
    float w[LCD_HEIGHT], wsum = 0;
    for (int y = 0; y < LCD_HEIGHT; y++) {
      if (fabsf(y + 0.5f - CX) >= 212) w[y] = 0.15f * LCD_WIDTH;                  // 圆外行：只填背景
      else {
        const int wd = mapCircleRows.right[y] - mapCircleRows.left[y] + 1;
        w[y] = t3dRows[y].ok ? wd * (T3dSt.ext ? 2.0f : 1.0f) : 0.1f * wd;       // 雾 / 天空行几乎不花时间
      }
      wsum += w[y];
    }
    float acc = 0; int mid = LCD_HEIGHT / 2;
    for (int y = 0; y < LCD_HEIGHT; y++) { acc += w[y]; if (acc >= wsum * T3D_MAIN_SHARE) { mid = y + 1; break; } }
    t3dMid = mid; t3dMidInit = true;
  } else {                                              // 反馈：主核心慢 → 分界上移（主核心少画），反之下移
    const float e = ((float)t3dTMain - (float)t3dTHelp) / (float)(t3dTMain + t3dTHelp);   // -1 .. 1
    t3dMid -= (int)lroundf(e * 20.0f);
  }
  t3dTMain = t3dTHelp = 0;
  if (t3dMid < 60) t3dMid = 60;
  if (t3dMid > LCD_HEIGHT - 60) t3dMid = LCD_HEIGHT - 60;
  t3dMid &= ~1;                                         // rasterRows3D 两行一组，分界必须是偶数行
}

static inline uint8_t t3dIdx(int ui, int vi) {          // 0xFF = 越出位图
  if ((uint32_t)ui >= (uint32_t)rasW || (uint32_t)vi >= (uint32_t)rasH) return 0xFF;
  uint32_t k = ((uint32_t)((vi >> 4) * rasBW + (ui >> 4)) << 8) | ((vi & 15) << 4) | (ui & 15);
  if (rasterViceValid) return (rasterVice[k] >> viceBaseShift) & 3;      // 罪恶都市平滑位图：只取基础 4 色（无斜边平滑、无楼块）
  uint8_t b = rasterBuf[k >> 1];
  return (k & 1) ? (b >> 4) : (b & 15);
}
// 同一片区域（16×16 位图像素）共用一档楼高：返回 true = 高楼
static inline bool t3dTall(int ui, int vi) {
  uint32_t h = (uint32_t)(ui >> 4) * 73856093u ^ (uint32_t)(vi >> 4) * 19349663u;
  h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
  return (h & 3) == 0;                                  // 约 1/4 高楼
}

// musicA.138：楼块探测抽成独立函数，供 2×2 块缓存调用。
// 高楼 → 矮楼；每档先查屋顶（往下 H 行），再查墙（往下 H/2 行）。
static inline uint16_t t3dBuildingAt(int y, int x, const T3dRow &r, bool &done) {
  done = false;
  if (!T3dSt.ext) return 0;
  const int dx = x - CX;
  for (int t = 0; t < (T3D_QUALITY >= 2 ? 2 : 1); t++) {
    const bool tallTier = (t == 0);
    for (int w = 0; w < 2 && !done; w++) {
      const int h = tallTier ? r.hTall : r.hLow;
      const int j = (w == 0) ? h : (h >> 1);
      if (j < 1) continue;
      const int yb = y + j;
      if (yb >= LCD_HEIGHT) continue;
      const T3dRow &rb = t3dRows[yb];
      if (!rb.ok) continue;
      const int bu = (rb.U0 + dx * rb.DU) >> 16, bv = (rb.V0 + dx * rb.DV) >> 16;
      const uint8_t idx = t3dIdx(bu, bv);
      if (idx < 4 || idx > 7) continue;
      if (T3D_QUALITY >= 2 && t3dTall(bu, bv) != tallTier) continue;   // 这座楼不是这一档
      const bool edge = idx & 1;
      done = true;
      if (w == 0) return tallTier ? (edge ? T3dSt.roofTe : T3dSt.roofT) : (edge ? T3dSt.roofLe : T3dSt.roofL);
      return tallTier ? T3dSt.wallT : T3dSt.wallL;
    }
  }
  return 0;
}

// musicA.138 提帧（视觉基本不变）：
//  ① 楼块颜色按 2×2 屏幕块共用一次探测（原来每像素 2 次 t3dIdx → 每 4 像素 2 次，-75%）；
//     楼块脚印来自 16×16 位图块，近处一个屏幕像素 < 1 位图像素，边缘差 ≤1px，远处由雾色遮盖。
//  ② 整行只遍历一次：圆外直接写 T3D_FILL，圆内写地图色，省掉 MapFrame 的第二次 466² fillScreen。
//  ③ fog 取极值时跳过 blendF。
// 两个核心各有自己的栈上缓存，不共享状态。
void __attribute__((optimize("O3"))) rasterRows3D(int y0, int y1, int step) {
  (void)step;
  const uint32_t t3dT0 = micros();                      // musicA.143：计时（y0 == 0 是主核心那一半）
  uint16_t *fb = RM.fb;
  const uint16_t *pal = RM.pal;
  const bool ext = T3dSt.ext;
  uint16_t bcol[(LCD_WIDTH >> 1) + 2];
  uint8_t  bdone[(LCD_WIDTH >> 1) + 2];
  for (int y = y0; y < y1; y += 2) {
    // 本组两行的圆内公共 x 范围 + 取样基准行（只用其中一行算楼块颜色）
    int xaLo = LCD_WIDTH, xbHi = -1, baseY = -1;
    for (int k = 0; k < 2; k++) {
      const int yy = y + k;
      if (yy < 0 || yy >= LCD_HEIGHT) continue;
      if (fabsf(yy + 0.5f - CX) >= 212) continue;
      if (baseY < 0) baseY = yy;
      if (mapCircleRows.left[yy] < xaLo) xaLo = mapCircleRows.left[yy];
      if (mapCircleRows.right[yy] > xbHi) xbHi = mapCircleRows.right[yy];
    }
    if (ext && xaLo <= xbHi) {
      const bool baseOK = baseY >= 0 && t3dRows[baseY].ok;
      for (int x = xaLo & ~1; x <= xbHi; x += 2) {
        bool d = false;
        bcol[x >> 1] = baseOK ? t3dBuildingAt(baseY, x, t3dRows[baseY], d) : 0;
        bdone[x >> 1] = (baseOK && d) ? 1 : 0;
      }
    }
    for (int k = 0; k < 2; k++) {
      const int yy = y + k;
      if (yy < 0 || yy >= LCD_HEIGHT) continue;
      uint16_t *row = fb + yy * LCD_WIDTH;
      if (fabsf(yy + 0.5f - CX) >= 212) {                  // 圆外行：整行填背景（替代 fillScreen）
        for (int x = 0; x < LCD_WIDTH; x++) row[x] = T3D_FILL;
        continue;
      }
      const int xa = mapCircleRows.left[yy], xb = mapCircleRows.right[yy];
      for (int x = 0; x < xa; x++) row[x] = T3D_FILL;      // 圆外像素：背景，一次写完
      for (int x = xb + 1; x < LCD_WIDTH; x++) row[x] = T3D_FILL;
      const T3dRow &r = t3dRows[yy];
      if (!r.ok) { for (int x = xa; x <= xb; x++) row[x] = T3dSt.haze; continue; }
      int32_t U = r.U0 + (xa - CX) * r.DU, V = r.V0 + (xa - CX) * r.DV;
      for (int x = xa; x <= xb; x++, U += r.DU, V += r.DV) {
        const int ui = U >> 16, vi = V >> 16;
        uint16_t color;
        bool done = false;
        if (ext && bdone[x >> 1]) { color = bcol[x >> 1]; done = true; }
        if (!done) {
          const uint8_t idx = t3dIdx(ui, vi);
          if (idx == 0xFF) color = T3dSt.haze;                                // 越出位图：雾色
          else if (ext && idx >= 4 && idx <= 7)                            // 自己就是脚印：正面墙
            color = (T3D_QUALITY >= 2 && !t3dTall(ui, vi)) ? T3dSt.wallL : T3dSt.wallT;
          else color = pal[idx];
        }
        if (r.fog) color = (r.fog >= 256) ? T3dSt.haze : blendF(color, T3dSt.haze, r.fog);
        row[x] = color;
      }
    }
  }
  { const uint32_t dt = micros() - t3dT0; if (y0 == 0) t3dTMain = dt; else t3dTHelp = dt; }
}
