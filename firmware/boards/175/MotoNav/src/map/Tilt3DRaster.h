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
static struct { uint16_t haze, roofT, roofTe, wallT, roofL, roofLe, wallL; bool ext; } T3dSt;

#if T3D_OWN_PALETTE
#define T3D_D_GROUND 0xEF5D
#define T3D_D_ROAD   0xBE39
#define T3D_D_PARK   0xD71A
#define T3D_D_WATER  0xCEFD
#define T3D_D_ROOF   0xF7BF
#define T3D_D_WALL   0x9D16
#define T3D_N_GROUND 0x18E5
#define T3D_N_ROAD   0x3A0B
#define T3D_N_PARK   0x19C6
#define T3D_N_WATER  0x11A9
#define T3D_N_ROOF   0x530F
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
  }
#endif
  const uint16_t roof = nightOn ? 0x530F : 0xF7BF, wall = nightOn ? 0x2988 : 0x9D16;
  // musicA.135：t3dQuality 0=无挤出(流畅) / 1=单档密楼(默认) / 2=两档精细
  const int q = constrain(t3dQuality, 0, 2);
  T3dSt.ext = buildings && q > 0 && !rasHasPal;
  T3dSt.haze  = RM.pal[0];
  T3dSt.roofL = roof;                                     // 矮楼：屋顶 = 基础色
  T3dSt.roofT = nightOn ? blendF(roof, C_WHITE, 40) : roof;   // 高楼：夜间略亮；白天已是最亮
  T3dSt.roofLe = blendF(roof, C_BLACK, 70);               // 屋顶轮廓（位图索引 5/7）
  T3dSt.roofTe = blendF(T3dSt.roofT, C_BLACK, 70);
  T3dSt.wallL = blendF(wall, C_BLACK, 25);                // 矮楼墙：略深
  T3dSt.wallT = blendF(wall, C_BLACK, 60);                // 高楼墙：更深，层次更明显
  const float inv = 1.0f / sc, mpp = rasMpp;
  const float span = (float)(T3D_PY - T3D_HORIZON);
  // 流畅档：远场更早雾化，少算挤出；精细档保持原雾距
  const float fog0 = (q <= 0) ? 0.42f : (q == 1 ? 0.50f : 0.50f);
  const float fog1 = (q <= 0) ? 0.90f : 0.97f;
  const float vk = T3D_VK * (q >= 2 ? 1.0f : (q == 1 ? 1.0f : 0.0f));
  for (int y = 0; y < LCD_HEIGHT; y++) {
    T3dRow &r = t3dRows[y];
    float s = ((float)T3D_PY - (y + 0.5f)) / span;      // = u/(u+D)
    if (s >= fog1) { r.ok = 0; continue; }              // 地平线以上：只填雾色
    if (s < -1.5f) s = -1.5f;
    const float k = 1.0f - s;
    const float u = T3D_D * s / k;                      // 前方平面像素距离
    const float ry = u * inv;                           // 前方米数
    const float lat = inv / k;                          // 每个屏幕像素对应的横向米数
    const float rx0 = 0.5f * lat;                       // x = CX 这一列的像素中心
    const float dx0 = rx0 * cs + ry * sn, dy0 = -rx0 * sn + ry * cs;
    r.U0 = (int32_t)(((cx + dx0 - rasX0) / mpp) * 65536.0f);
    r.V0 = (int32_t)(((rasY0 - (cy + dy0)) / mpp) * 65536.0f);
    r.DU = (int32_t)(cs * lat / mpp * 65536.0f);
    r.DV = (int32_t)(sn * lat / mpp * 65536.0f);
    float f = (s - fog0) / (fog1 - fog0);               // 远处渐隐到雾色（也盖住远处最近邻采样的闪烁）
    r.fog = f <= 0 ? 0 : (uint16_t)(f >= 1 ? 256 : f * 256);
    // musicA.135：流畅档远场不挤出；均衡/精细保留密楼，远场高度随透视自然收敛
    const float hk = sc * k * vk;
    int ht = (int)(T3D_H_TALL_M * hk + 0.5f), hl = (int)(T3D_H_LOW_M * hk + 0.5f);
    if (q == 1 && s > 0.62f) { ht = 0; hl = 0; }        // 均衡：极远场跳过挤出（抬 FPS，近场仍密）
    r.hTall = (int16_t)(ht > 48 ? 48 : ht); r.hLow = (int16_t)(hl > 48 ? 48 : hl);
    r.ok = 1;
  }
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

void __attribute__((optimize("O3"))) rasterRows3D(int y0, int y1, int step) {
  uint16_t *fb = RM.fb;
  const uint16_t *pal = RM.pal;
  const bool ext = T3dSt.ext;
  for (int y = y0; y < y1; y += step) {
    if (y < 0 || y >= LCD_HEIGHT) continue;
    if (fabsf(y + 0.5f - CX) >= 212) continue;
    const int xa = mapCircleRows.left[y], xb = mapCircleRows.right[y];
    uint16_t *row = fb + y * LCD_WIDTH;
    const T3dRow &r = t3dRows[y];
    if (!r.ok) { for (int x = xa; x <= xb; x++) row[x] = T3dSt.haze; continue; }
    int32_t U = r.U0 + (xa - CX) * r.DU, V = r.V0 + (xa - CX) * r.DV;
    for (int x = xa; x <= xb; x++, U += r.DU, V += r.DV) {
      const int ui = U >> 16, vi = V >> 16;
      uint16_t color;
      bool done = false;
      if (ext) {
        const int dx = x - CX;
        // 高楼 → 矮楼；每档：屋顶（往下 H 行）、墙（往下 H/2 行）
        const int tiers = (t3dQuality >= 2) ? 2 : 1;
        for (int t = 0; t < tiers && !done; t++) {
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
            if (t3dQuality >= 2 && t3dTall(bu, bv) != tallTier) continue;   // 这座楼不是这一档
            const bool edge = idx & 1;
            if (w == 0) color = tallTier ? (edge ? T3dSt.roofTe : T3dSt.roofT) : (edge ? T3dSt.roofLe : T3dSt.roofL);
            else        color = tallTier ? T3dSt.wallT : T3dSt.wallL;
            done = true;
          }
        }
      }
      if (!done) {
        const uint8_t idx = t3dIdx(ui, vi);
        if (idx == 0xFF) color = T3dSt.haze;                                  // 越出位图：雾色（与自带配色一致）
        else if (ext && idx >= 4 && idx <= 7)                              // 自己就是脚印：正面墙
          color = (t3dQuality >= 2 && !t3dTall(ui, vi)) ? T3dSt.wallL : T3dSt.wallT;
        else color = pal[idx];
      }
      if (r.fog) color = blendF(color, T3dSt.haze, r.fog);
      row[x] = color;
    }
  }
}
