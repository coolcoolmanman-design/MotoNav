#pragma once
// 地图模式判断、抗锯齿线条和多边形。由 MotoNav.ino 按依赖顺序包含，仅编译一次。
// 当前位姿+缩放下，视口圆是否仍落在栅格瓦片矩形内（含 MAP_PY 前视偏置）。
// musicA.11：sticky 上一张瓦片时若驶出方块边界，圆屏顶部会出现「平切」建筑轮廓截断。
static inline bool rasterCoversView(float cx, float cy, float sc) {
  if (!rasterValid || rasMpp < 0.01f || rasW < 8 || rasH < 8) return false;
  if (sc < 0.05f) sc = 0.55f;
  float viewR = (CX + fabsf((float)MAP_PY - (float)CX) + 40.0f) / sc;
  float x0 = (float)rasX0;
  float x1 = (float)rasX0 + (float)rasW * rasMpp;
  float y1 = (float)rasY0;
  float y0 = (float)rasY0 - (float)rasH * rasMpp;
  return (cx - viewR >= x0 && cx + viewR <= x1 &&
          cy - viewR >= y0 && cy + viewR <= y1);
}

// 现在是否显示小地图
// musicA.10：允许 sticky 上一张有效底图，避免 pose 抢跑 ACK 掉回大箭头。
// musicA.11：sticky 仅当视口仍被瓦片覆盖；否则宁可短时箭头，也不露方瓦平切边。
bool mapModeActive() {
  // ui.6：大字转向是独立排版，不依赖底图；提前返回也避免主循环按地图页每帧刷。
  if (themeIdx == ST_BIGTURN || isVectorHeroTheme()) return false;
  if (!inNav) return false;
  // musicA.26：路口图自带全部信息，不依赖底图/位置/小地图开关——
  // 只要导航中且有图就必须画，否则底图没到、关了地图、丢 GPS 时会被连带藏掉。
  if (crossReady()) return true;
  if (!mapOn || posAt == 0) return false;
  if (styleIdx() == ST_DRAGON) return true;
  if (rasterValid && rasterFrame == posFrame) return true;
  if (mapValid && mapFrame == posFrame) return true;
  float sc = viewScale > 0.05f ? viewScale : 0.55f;
  // sticky 仅限仍盖住视口的栅格；矢量错帧不 sticky，避免奶油底+平切轮廓。
  if (rasterValid && rasterCoversView(posX, posY, sc)) return true;
  // musicA.150：样式16 有瓦片就进地图页（越界由底图渲染补边渐隐），不再因为驶出瓦片掉回大箭头页
  if (styleIdx() == ST_DAYNIGHT_GLASS && rasterValid && millis() - rasterAt < 21600000UL) return true;
  return false;
}

// ---- 画图工具 ----

// 粗折线（圆角连接）；屏幕外的线段直接跳过
// v2.2：只在方向变化明显的拐点补圆（直线上的点不画圆，省很多时间）；caps = 两端也画圆头
struct VicePlane { float nx, ny, c, support; uint8_t angle; };

static inline void viceBlendPixel(uint16_t *row, int x, uint16_t color, float coverage) {
  int alpha = (int)(coverage * 256.0f + 0.5f);
  if (alpha <= 0) return;
  row[x] = alpha >= 256 ? color : blendF(row[x], color, alpha);
}

void __attribute__((optimize("O3"))) viceAAQuad(const float *px, const float *py, uint16_t color) {
  VicePlane plane[4];
  float cx = 0, cy = 0, ymin = py[0], ymax = py[0];
  for (int i = 0; i < 4; i++) { cx += px[i]; cy += py[i]; ymin = fminf(ymin, py[i]); ymax = fmaxf(ymax, py[i]); }
  cx *= 0.25f; cy *= 0.25f;
  for (int i = 0; i < 4; i++) {
    int j = (i + 1) & 3;
    float dx = px[j] - px[i], dy = py[j] - py[i], length = sqrtf(dx * dx + dy * dy);
    if (length < 0.001f) return;
    float nx = -dy / length, ny = dx / length;
    if ((cx - px[i]) * nx + (cy - py[i]) * ny < 0) { nx = -nx; ny = -ny; }
    plane[i] = {nx, ny, -nx * px[i] - ny * py[i], (fabsf(nx) + fabsf(ny)) * 0.5f, viceCoverageAngle(nx, ny)};
  }
  uint16_t *fb = gfx->getFramebuffer();
  int ya = max(0, (int)floorf(ymin - 1)), yb = min(LCD_HEIGHT - 1, (int)ceilf(ymax + 1));
  for (int y = ya; y <= yb; y++) {
    float sy = y + 0.5f, left = 0.5f, right = LCD_WIDTH - 0.5f;
    float solidLeft = left, solidRight = right;
    bool outside = false, noSolid = false;
    float q[4];
    for (int i = 0; i < 4; i++) {
      q[i] = plane[i].ny * sy + plane[i].c;
      float nx = plane[i].nx, support = plane[i].support;
      if (fabsf(nx) < 0.00001f) {
        if (q[i] <= -support) outside = true;
        if (q[i] < support) noSolid = true;
      } else if (nx > 0) {
        left = fmaxf(left, (-support - q[i]) / nx);
        solidLeft = fmaxf(solidLeft, (support - q[i]) / nx);
      } else {
        right = fminf(right, (-support - q[i]) / nx);
        solidRight = fminf(solidRight, (support - q[i]) / nx);
      }
    }
    if (outside || left > right) continue;
    int xa = max(0, (int)ceilf(left - 0.5f)), xb = min(LCD_WIDTH - 1, (int)floorf(right - 0.5f));
    int ia = max(xa, (int)ceilf(solidLeft - 0.5f)), ib = min(xb, (int)floorf(solidRight - 0.5f));
    if (noSolid || ia > ib) { ia = xb + 1; ib = xb; }
    uint16_t *row = fb + y * LCD_WIDTH;
    for (int x = xa; x <= xb;) {
      if (x == ia && ia <= ib) {
        gfx->drawFastHLine(x, y, ib - ia + 1, color);
        x = ib + 1; continue;
      }
      float sx = x + 0.5f;
      int alpha = 256;
      for (int i = 0; i < 4; i++) {
        float distance = plane[i].nx * sx + q[i];
        if (distance <= -plane[i].support) { alpha = 0; break; }
        if (distance < plane[i].support) {
          int index = max(0, min(256, (int)floorf(128.5f + distance * (256.0f / 1.5f))));
          alpha = min(alpha, (int)viceCoverage[plane[i].angle][index]);
        }
      }
      if (alpha) row[x] = alpha >= 256 ? color : blendF(row[x], color, alpha);
      x++;
    }
  }
}

void __attribute__((optimize("O3"))) viceAACircle(float cx, float cy, float radius, uint16_t color) {
  uint16_t *fb = gfx->getFramebuffer();
  int ya = max(0, (int)floorf(cy - radius - 1)), yb = min(LCD_HEIGHT - 1, (int)ceilf(cy + radius + 1));
  float outer = radius + 0.5f, inner = fmaxf(0, radius - 0.5f);
  for (int y = ya; y <= yb; y++) {
    float dy = y + 0.5f - cy;
    if (fabsf(dy) >= outer) continue;
    float extent = sqrtf(outer * outer - dy * dy);
    int xa = max(0, (int)ceilf(cx - extent - 0.5f)), xb = min(LCD_WIDTH - 1, (int)floorf(cx + extent - 0.5f));
    int ia = xb + 1, ib = xb;
    if (fabsf(dy) < inner) {
      float core = sqrtf(inner * inner - dy * dy);
      ia = max(xa, (int)ceilf(cx - core - 0.5f)); ib = min(xb, (int)floorf(cx + core - 0.5f));
    }
    uint16_t *row = fb + y * LCD_WIDTH;
    for (int x = xa; x <= xb;) {
      if (x == ia && ia <= ib) {
        gfx->drawFastHLine(x, y, ib - ia + 1, color);
        x = ib + 1; continue;
      }
      float dx = x + 0.5f - cx;
      viceBlendPixel(row, x, color, outer - sqrtf(dx * dx + dy * dy));
      x++;
    }
  }
}

void __attribute__((optimize("O3"))) mapPolyline(const float *xs, const float *ys, int n, float w, uint16_t c, bool caps) {
  bool vice = isViceLikeStyle();
  if (vice) initViceSampling();
  float r = w / 2;
  float pdx = 0, pdy = 0;
  bool havePrev = false;
  for (int i = 0; i + 1 < n; i++) {
    float x0 = xs[i], y0 = ys[i], x1 = xs[i + 1], y1 = ys[i + 1];
    if ((x0 < -w && x1 < -w) || (x0 > LCD_WIDTH + w && x1 > LCD_WIDTH + w) ||
        (y0 < -w && y1 < -w) || (y0 > LCD_HEIGHT + w && y1 > LCD_HEIGHT + w)) continue;
    float dx = x1 - x0, dy = y1 - y0;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 0.5f) continue;
    float ux = dx / len, uy = dy / len;
    float nx = -uy * r, ny = ux * r;
    if (vice) {
      float px[4] = {x0 + nx, x0 - nx, x1 - nx, x1 + nx};
      float py[4] = {y0 + ny, y0 - ny, y1 - ny, y1 + ny};
      viceAAQuad(px, py, c);
    } else {
      gfx->fillTriangle(x0 + nx, y0 + ny, x0 - nx, y0 - ny, x1 + nx, y1 + ny, c);
      gfx->fillTriangle(x0 - nx, y0 - ny, x1 - nx, y1 - ny, x1 + nx, y1 + ny, c);
    }
    if (r >= 1.5f) {
      if (havePrev ? (ux * pdx + uy * pdy < 0.97f) : caps) {
        if (vice) viceAACircle(x0, y0, r, c); else gfx->fillCircle(x0, y0, r, c);
      }
      if (caps && i + 2 == n) {
        if (vice) viceAACircle(x1, y1, r, c); else gfx->fillCircle(x1, y1, r, c);
      }
    }
    pdx = ux; pdy = uy; havePrev = true;
  }
}

// 实心多边形（扫描线，奇偶规则）
void mapFillPoly(const float *xs, const float *ys, int n, uint16_t c) {
  if (n < 3) return;
  float ymin = 1e9f, ymax = -1e9f, xmin = 1e9f, xmax = -1e9f;
  for (int i = 0; i < n; i++) {
    ymin = fminf(ymin, ys[i]); ymax = fmaxf(ymax, ys[i]);
    xmin = fminf(xmin, xs[i]); xmax = fmaxf(xmax, xs[i]);
  }
  if (ymax < 0 || ymin >= LCD_HEIGHT || xmax < 0 || xmin >= LCD_WIDTH) return;
  int y0 = max(0, (int)ceilf(ymin - 0.5f)), y1 = min(LCD_HEIGHT - 1, (int)floorf(ymax - 0.5f));
  static float xs_hit[256];
  for (int y = y0; y <= y1; y++) {
    float sy = y + 0.5f;
    int k = 0;
    for (int i = 0, j = n - 1; i < n; j = i++) {
      float ya = ys[j], yb = ys[i];
      if ((ya <= sy && yb > sy) || (yb <= sy && ya > sy)) {
        if (k < 256) xs_hit[k++] = xs[j] + (sy - ya) * (xs[i] - xs[j]) / (yb - ya);
      }
    }
    for (int a = 1; a < k; a++) {               // 插入排序（交点通常很少）
      float v = xs_hit[a]; int b = a - 1;
      while (b >= 0 && xs_hit[b] > v) { xs_hit[b + 1] = xs_hit[b]; b--; }
      xs_hit[b + 1] = v;
    }
    for (int a = 0; a + 1 < k; a += 2) {
      int xa = max(0, (int)ceilf(xs_hit[a] - 0.5f)), xb = min(LCD_WIDTH - 1, (int)floorf(xs_hit[a + 1] - 0.5f));
      if (xb >= xa) gfx->drawFastHLine(xa, y, xb - xa + 1, c);
    }
  }
}

// 车头箭头（白色，深色描边）
void drawPlayer(float x, float y) {
  // v3.9 车头箭头缩小到原来的约 70%
  const float T[4][2] = {{0, -13}, {-10, 10.5f}, {0, 5}, {10, 10.5f}};
  for (int pass = 0; pass < 2; pass++) {
    float k = pass == 0 ? 1.35f : 1.0f;
    uint16_t c = pass == 0 ? C_BLACK : C_WHITE;
    float yo = pass == 0 ? 1.0f : 0;
    gfx->fillTriangle(x + T[0][0] * k, y + T[0][1] * k + yo, x + T[1][0] * k, y + T[1][1] * k + yo, x + T[2][0] * k, y + T[2][1] * k + yo, c);
    gfx->fillTriangle(x + T[0][0] * k, y + T[0][1] * k + yo, x + T[2][0] * k, y + T[2][1] * k + yo, x + T[3][0] * k, y + T[3][1] * k + yo, c);
  }
}

// ---- musicA.156 样式16：参考图车标 = 六边形盾牌（上宽下收、底有尖尾）+ 内嵌实心三角，昼夜反色 ----
// 参考采样：日景外框深蓝 0x024C/0x022C、内三角浅蓝 0xAF7F；夜景外框白 0xE7DF、内三角深青 0x00E4。
// 顶尖 (0,-16)、左右肩 (±12,-5)、左右下腰 (±9,8)、底尖 (0,15)；内三角 (0,-9)/(±6,5)。
// musicA.157：圆角六边形——把每个顶点沿两侧边各内缩 cut 后连成 12 边形。
// 之前用「顶点补圆」，圆会凸到顶点外面，实机上像长出六个包；切角是往里收，才像参考图的圆润盾牌。
static void dnChamferHex(float x, float y, const float *EX, const float *EY, float cut, uint16_t col) {
  float px[12], py[12];
  int n = 0;
  const float cx = (EX[0] + EX[1] + EX[2] + EX[3] + EX[4] + EX[5]) / 6.0f;
  const float cy = (EY[0] + EY[1] + EY[2] + EY[3] + EY[4] + EY[5]) / 6.0f;
  for (int i = 0; i < 6; i++) {
    const int pv = (i + 5) % 6, nx = (i + 1) % 6;
    float dx = EX[i] - EX[pv], dy = EY[i] - EY[pv];
    float l = sqrtf(dx * dx + dy * dy); if (l < 0.001f) l = 1;
    px[n] = x + EX[i] - dx / l * cut; py[n] = y + EY[i] - dy / l * cut; n++;
    dx = EX[nx] - EX[i]; dy = EY[nx] - EY[i];
    l = sqrtf(dx * dx + dy * dy); if (l < 0.001f) l = 1;
    px[n] = x + EX[i] + dx / l * cut; py[n] = y + EY[i] + dy / l * cut; n++;
  }
  (void)cx; (void)cy;
  for (int i = 1; i + 1 < n; i++)
    gfx->fillTriangle(px[0], py[0], px[i], py[i], px[i + 1], py[i + 1], col);
}

void dnGlassPlayer(float x, float y) {
  // musicA.157：按 _refd466/_refn466 逐像素还原。
  // 形状 = 圆润宽六边形盾牌（顶点切角，不是尖角）+ 内层带底部凹口的导航箭头。
  // 色值实测：日 外框 (0,68,95) / 内芯 (158,232,253)；夜 外框 (240,250,252) / 内芯 (0,40,55)。
  // 旧版夜景外框写的 0xE7DF 解出来是 (224,252,120) 黄绿色，已修正为 0xF7DF。
  const uint16_t edge = nightOn ? 0xF7DF : 0x022B;
  const uint16_t core = nightOn ? 0x0146 : 0x9F5F;
  const uint16_t shC  = nightOn ? 0x0841 : 0x5C8C;   // 底投影
  // 外形（参考实拍放大）：顶部圆尖、下部左右两个翼尖 + 中间一条尖尾（三凸起）
  // 用「主体六边形 + 两翼 + 尖尾」三块同色填充拼出来；都是凸多边形，扇面填充无接缝。
  static const float UX[6] = {0, 13, 11, 0, -11, -13};
  static const float UY[6] = {-16, -6, 6, 9, 6, -6};
  dnChamferHex(x, y + 3, UX, UY, 3.0f, shC);      // 投影（整体下移 3px）
  dnChamferHex(x, y,     UX, UY, 3.0f, edge);     // 主体
  gfx->fillTriangle(x, y - 4, x + 13.5f, y + 1, x + 12.0f, y + 11, edge);   // 右翼
  gfx->fillTriangle(x, y - 4, x - 12.0f, y + 11, x - 13.5f, y + 1, edge);   // 左翼
  gfx->fillTriangle(x - 5.0f, y + 6, x + 5.0f, y + 6, x, y + 16, edge);     // 中间尖尾
  // 内层导航箭头：尖 (0,-10) / 左右底 (±9,5.5) / 深凹口 (0,-0.5)——参考图凹口很深，几乎到底
  gfx->fillTriangle(x, y - 10, x + 9.0f, y + 5.5f, x, y - 0.5f, core);
  gfx->fillTriangle(x, y - 10, x, y - 0.5f, x - 9.0f, y + 5.5f, core);
}

// musicA.150 样式16：底部渐变遮罩（参考稿路名/底部胶囊下方的地面压到近黑）
void dnGlassBottomScrim() {
  uint16_t *fb = gfx->getFramebuffer();
  if (!fb) return;
  const int y0 = 366, y1 = LCD_HEIGHT;
  for (int y = y0; y < y1; y++) {
    const int k = 255 - (150 * (y - y0) / (y1 - y0));   // musicA.151 减轻: 225->150
    uint16_t *row = fb + (size_t)y * LCD_WIDTH;
    for (int x = 0; x < LCD_WIDTH; x++) {
      const uint16_t c = row[x];
      row[x] = (uint16_t)(((((c >> 11) & 0x1F) * k) >> 8 << 11) | ((((c >> 5) & 0x3F) * k) >> 8 << 5) | (((c & 0x1F) * k) >> 8));
    }
  }
}

// ---- musicA.147c 样式16：顶部「深色渐变遮罩」（参考图文案里写的「深色渐变顶遮罩」）----
// 上方最浓、向下一路淡出，HUD 文字压在上面才不会和亮色路网打起来。
// 白天用深青蓝、夜间用近黑；按 4 行一条带算一次 alpha，省 3/4 的取色开销。
void dnGlassTopScrim() {
  uint16_t *fb = gfx->getFramebuffer();
  if (!fb) return;
  const bool night = nightOn; (void)night;
  // musicA.150 按参考稿重调：顶部近黑（≈95%），向下拉长到 y≈172 才完全透明，曲线用 0.6 次幂（比线性更「压得住」上半截）
  const int yEnd = 132;   // musicA.152 150->132：阴影区域再收，贴参考稿
  const int peak = 158;   // musicA.152 185->158                             // 顶部最大不透明度（0..255）
  static uint8_t dnScrimA[yEnd];
  static bool dnScrimInit = false;
  if (!dnScrimInit) {
    // musicA.152：指数 0.6→0.78，近顶端更柔和、末端不突然断掉
    for (int y = 0; y < yEnd; y++) dnScrimA[y] = (uint8_t)fminf(255.0f, peak * powf((float)(yEnd - y) / yEnd, 0.78f) + 0.5f);
    dnScrimInit = true;
  }
  for (int y = 0; y < yEnd; y++) {
    const int k = 255 - dnScrimA[y];                  // 剩余亮度（移位乘法，比 blend565 的三次除法快）
    uint16_t *row = fb + (size_t)y * LCD_WIDTH;
    for (int x = 0; x < LCD_WIDTH; x++) {
      const uint16_t c = row[x];
      row[x] = (uint16_t)(((((c >> 11) & 0x1F) * k) >> 8 << 11) | ((((c >> 5) & 0x3F) * k) >> 8 << 5) | (((c & 0x1F) * k) >> 8));
    }
  }
}
