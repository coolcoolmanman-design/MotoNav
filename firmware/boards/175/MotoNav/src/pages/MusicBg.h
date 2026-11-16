#pragma once
// ui.6 音乐封面「风格化背景」——固件端自算，零传输 / 零新增 bulk / 老 App 不用升级。
// 只在换封面或换档位时由 musicBgBuild() 重算一次 466² 缓存；
// drawMusicAmbient() 每帧只做一次 memcpy（原来逐像素刷纯色反而更慢）。
// 5 种播放样式共用；样式 4 极简仪表不再硬编码 0x1082 深底。
//
// include 位置：MotoNav.ino 中 BadgePage.h 之后、PageNavigation.h 之前。
// 只依赖 DeviceContext.h（gfx / CX / coverImg / musicBg*），
// 不引用 PageNavigation.h 的 chordHalf，也不读 MapState.h 的 nightOn。
//
// 档位（强度固定「标准」：压暗 40% / 去饱和 15% / 径向暗角 34%）：
//   0 关闭（原 coverAmbient 纯色）  1 放大虚化（默认）
//   2 色块马赛克（10×10 硬边）     3 双色海报（亮度 4 档映射）

#define MBG_MODE_MAX 3
#define MBG_GRID_MAX 64
static uint16_t mbgGrid[MBG_GRID_MAX * MBG_GRID_MAX];   // 8KB 内部 RAM

// FO-225c：封面流主题下 type 5 可能拿不到（HID/AMS 第三方播放器没有本地封面），
// 但只要 type 12 队列到位就用队列封做背景源，四个风格化档位不再全部失效。
// FO-225e：协议 slot 1 = 当前封、静止中央卡也是 slot 1。背景在 style 7 下固定绑
// slot 1（不跟 cfShift 跑，吸附到下一首时不会提前变成「下一张」），且优先于 type 5，
// 避免两条 bulk 到达顺序不同步时背景与中央卡错开一张。
static const uint16_t *mbgSrcPixels = nullptr;
static int mbgSrcW = 0, mbgSrcH = 0, mbgSrcSlot = -1;
static inline bool mbgPickSrc() {
  mbgSrcSlot = -1;
  if (musicStyle == 7 && cfSlotOk[1] && cfCovers[1]) {
    mbgSrcPixels = cfCovers[1]; mbgSrcW = CFQ_PX; mbgSrcH = CFQ_PX;
    mbgSrcSlot = 1; return true;                 // 封面流：严格与中央卡同源
  }
  if (coverValid && coverImg && coverW > 0 && coverH > 0) {
    mbgSrcPixels = coverImg; mbgSrcW = coverW; mbgSrcH = coverH; return true;
  }
  if (cfSlotOk[1] && cfCovers[1]) {
    mbgSrcPixels = cfCovers[1]; mbgSrcW = CFQ_PX; mbgSrcH = CFQ_PX;
    mbgSrcSlot = 1; return true;
  }
  return false;
}

static inline uint16_t mbg565(int r, int g, int b) {
  if (r < 0) r = 0; else if (r > 255) r = 255;
  if (g < 0) g = 0; else if (g > 255) g = 255;
  if (b < 0) b = 0; else if (b > 255) b = 255;
  return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

// 从 128² 封面按「中心 1.55× 裁切」采样一格；3×3 邻域均值 = 天然模糊，
// 保留大尺度明暗与色块（封面圆盘只占可视圆 23%，细节太细会被压成一片色）。
static uint16_t mbgCellMean(int gx, int gy, int gn) {
  float u = ((float)gx + 0.5f) / gn, v = ((float)gy + 0.5f) / gn;
  u = 0.5f + (u - 0.5f) / 1.55f;
  v = 0.5f + (v - 0.5f) / 1.55f;
  int x0 = (int)(u * mbgSrcW), y0 = (int)(v * mbgSrcH);
  long rs = 0, gs = 0, bs = 0; int n = 0;
  for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
    int x = x0 + dx, y = y0 + dy;
    if (x < 0) x = 0; if (x >= mbgSrcW) x = mbgSrcW - 1;
    if (y < 0) y = 0; if (y >= mbgSrcH) y = mbgSrcH - 1;
    uint16_t c = mbgSrcPixels[(size_t)y * mbgSrcW + x];
    rs += (c >> 11) & 31; gs += (c >> 5) & 63; bs += c & 31; n++;
  }
  return (uint16_t)(((rs / n) << 11) | ((gs / n) << 5) | (bs / n));
}

// 统一压暗 40% + 去饱和 15%（只在 ≤4096 格上做，浮点开销可忽略）
static uint16_t mbgTone(uint16_t c) {
  int r = ((c >> 11) & 31) * 255 / 31;
  int g = ((c >> 5) & 63) * 255 / 63;
  int b = (c & 31) * 255 / 31;
  int lum = (r * 77 + g * 150 + b * 29) / 256;
  r += (lum - r) * 15 / 100; g += (lum - g) * 15 / 100; b += (lum - b) * 15 / 100;
  return mbg565(r * 60 / 100, g * 60 / 100, b * 60 / 100);
}

void musicBgBuild() {
  musicBgFailed = false;
  if (!mbgPickSrc()) return;
  Serial.printf("[MBG] build mode=%d src=%dx%d style=%d slot=%d\n",
                (int)musicBgMode, mbgSrcW, mbgSrcH, (int)musicStyle, mbgSrcSlot);
  if (!musicBgBuf) musicBgBuf = (uint16_t *)ps_malloc((size_t)LCD_WIDTH * LCD_HEIGHT * 2);
  if (!musicBgBuf) { musicBgFailed = true; return; }   // 回退纯色，绝不让音乐页崩

  const uint8_t mode = musicBgMode;
  const int gn = (mode == 1) ? 32 : (mode == 2) ? 10 : 64;

  // ① 降采样到网格 + 色调处理
  for (int gy = 0; gy < gn; gy++) for (int gx = 0; gx < gn; gx++) {
    uint16_t c = mbgCellMean(gx, gy, gn);
    if (mode == 3) {
      // 双色海报：亮度 4 档 → 暗底 / 封面本色 / 提亮 / 高光
      int r = ((c >> 11) & 31) * 255 / 31, g = ((c >> 5) & 63) * 255 / 63, b = (c & 31) * 255 / 31;
      int l = (r * 77 + g * 150 + b * 29) / 256;
      uint16_t pal[4] = { mbg565(12, 16, 26), c,
                          mbg565((r + 255) / 2, (g + 255) / 2, (b + 255) / 2),
                          mbg565(235, 238, 245) };
      c = pal[l < 60 ? 0 : l < 120 ? 1 : l < 190 ? 2 : 3];
    }
    mbgGrid[gy * gn + gx] = mbgTone(c);
  }

  // ② 网格放大到整屏 + 径向暗角 + 圆外清零（一次算完，之后每帧只 memcpy）
  const int R2 = 233 * 233;
  for (int y = 0; y < LCD_HEIGHT; y++) {
    int dy = y - CX;
    float fy = ((float)y + 0.5f) / LCD_HEIGHT * gn - 0.5f;
    int gy0 = (int)floorf(fy), gy1 = gy0 + 1; float wy = fy - gy0;
    if (gy0 < 0) { gy0 = 0; wy = 0; } if (gy1 >= gn) { gy1 = gn - 1; wy = 0; }
    int vy = (int)(((float)y + 0.5f) / LCD_HEIGHT * gn); if (vy >= gn) vy = gn - 1;
    uint16_t *row = musicBgBuf + (size_t)y * LCD_WIDTH;
    for (int x = 0; x < LCD_WIDTH; x++) {
      int dx = x - CX;
      int r2 = dx * dx + dy * dy;
      if (r2 > R2) { row[x] = 0; continue; }
      uint16_t c;
      if (mode == 1) {
        // 放大虚化：32×32 网格双线性插值 → 平滑无方块
        float fx = ((float)x + 0.5f) / LCD_WIDTH * gn - 0.5f;
        int gx0 = (int)floorf(fx), gx1 = gx0 + 1; float wx = fx - gx0;
        if (gx0 < 0) { gx0 = 0; wx = 0; } if (gx1 >= gn) { gx1 = gn - 1; wx = 0; }
        uint16_t a = mbgGrid[gy0 * gn + gx0], b2 = mbgGrid[gy0 * gn + gx1];
        uint16_t d = mbgGrid[gy1 * gn + gx0], e = mbgGrid[gy1 * gn + gx1];
        int ar = (a >> 11) & 31, br = (b2 >> 11) & 31, dr = (d >> 11) & 31, er = (e >> 11) & 31;
        int ag = (a >> 5) & 63, bg = (b2 >> 5) & 63, dg = (d >> 5) & 63, eg = (e >> 5) & 63;
        int ab = a & 31, bb = b2 & 31, db = d & 31, eb = e & 31;
        int rr = (int)((1 - wy) * ((1 - wx) * ar + wx * br) + wy * ((1 - wx) * dr + wx * er) + 0.5f);
        int rg = (int)((1 - wy) * ((1 - wx) * ag + wx * bg) + wy * ((1 - wx) * dg + wx * eg) + 0.5f);
        int rb = (int)((1 - wy) * ((1 - wx) * ab + wx * bb) + wy * ((1 - wx) * db + wx * eb) + 0.5f);
        if (rr > 31) rr = 31; if (rg > 63) rg = 63; if (rb > 31) rb = 31;
        c = (uint16_t)((rr << 11) | (rg << 5) | rb);
      } else {
        // 色块马赛克 / 双色海报：硬边放大
        int vx = (int)(((float)x + 0.5f) / LCD_WIDTH * gn); if (vx >= gn) vx = gn - 1;
        c = mbgGrid[vy * gn + vx];
      }
      // 径向暗角
      int keep = 255 - (int)((long)r2 * 88 / R2);
      if (keep < 0) keep = 0;
      if (keep < 255) {
        int r = (((c >> 11) & 31) * keep + 127) / 255;
        int g = (((c >> 5) & 63) * keep + 127) / 255;
        int b = ((c & 31) * keep + 127) / 255;
        c = (uint16_t)((r << 11) | (g << 5) | b);
      }
      row[x] = c;
    }
  }
}

// 每帧只做一次拷贝；false = 调用方退回 coverAmbient 纯色氛围底
bool musicBgBlit(uint16_t *fb) {
  if (musicBgMode == 0 || !fb) return false;
  const bool haveSrc = mbgPickSrc();
  if (musicBgDirty && haveSrc) { musicBgBuild(); musicBgDirty = false; }
  if (!musicBgBuf || musicBgFailed) return false;
  memcpy(fb, musicBgBuf, (size_t)LCD_WIDTH * LCD_HEIGHT * 2);
  return true;
}
