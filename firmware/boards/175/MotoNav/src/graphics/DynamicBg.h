#pragma once
// ================= musicA.37 预设动态背景（正式功能） =================
// 8 风格（0 静态 / 1..5 辉光类 / 6 像素夜景 / 7 像素白天），程序化渲染：零 BLE 传输、零文件存储。
// 466×466 逐像素浮点算一帧太重，这里用 117×117 低分辨率网格（每格 4×4 像素）
// + Q15 定点 + 256 项正弦表，每帧只算 1.37 万个网格点，10fps 下 CPU 占用很低。
// 辉光/极光/涟漪本来就是大面积缓变，4×4 放大后观感与全分辨率一致；星尘的锐利星点
// 在网格渲染之后直接叠到全分辨率缓冲上。style 6/7 改用 78×78 / 6px 调色板网格，
// 同样一帧约 22 万次写，CPU 与辉光类相当（见下方 dynBgRenderPixelCity）。
// FO-220：style 6 的暖黄满圆改成冷白弦月；style 7 是同构图的白天主题（太阳 / 飘云 /
// 玻璃反光 / 暖灰路面），两版共用同一 LCG 序列与相位，楼群轮廓、道路、骑手逐帧对应。
// 昼夜：App 的 #n,1/0 改全局 nightOn，本模块每次渲染都读它，不新增定时器。
// 漂移：T = millis() × dynBgSpeed/10（默认 ×1.8），只缩放时间、不改幅度。
extern bool nightOn;                     // MapState.h 稍后定义（同一编译单元，先 extern 声明）

#define DBG_CELL 4
#define DBG_GW   117
#define DBG_GH   117

static int16_t dbgSinTab[256];
static bool    dbgSinReady = false;

static void dbgInitSin() {
  if (dbgSinReady) return;
  for (int i = 0; i < 256; i++)
    dbgSinTab[i] = (int16_t)lroundf(32767.0f * sinf(i * 6.2831853f / 256.0f));
  dbgSinReady = true;
}
// Q15 正弦，参数用 0..255 表示一圈（负数会自动按位与回绕）
static inline int32_t dbgSinQ(int32_t a) { return dbgSinTab[(uint32_t)a & 255]; }
// t(ms) / 周期(ms) → 0..255 的相位
static inline uint32_t dbgPhase(uint32_t t, uint32_t periodMs) {
  return (uint32_t)(((uint64_t)(t % periodMs) * 256ULL) / periodMs);
}
static inline uint16_t dbgRGB(int r, int g, int b) {
  if (r < 0) r = 0; else if (r > 255) r = 255;
  if (g < 0) g = 0; else if (g > 255) g = 255;
  if (b < 0) b = 0; else if (b > 255) b = 255;
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

// ================= FO-217 / FO-220：style 6「像素夜景」/ 7「像素白天」 =================
// 确认稿 pixel-city-daynight-compare.png 的固件实现：78×78 调色板网格 × 6px 放大到 466²，
// 零资源、零 BLE。动画只靠相位：路面虚线滚动 / 骑手 bob / 轮辐转 / 星窗闪烁 /
// 日芒呼吸 / 云飘。两版共用 LCG 序列，楼群与骑手位置逐帧一一对应。
// 大时钟与三槽对应的网格保持低对比纯色，与 zbgRect 擦背景 + 直接叠字一致。
#define PX_CELL 6
#define PX_GW   78
#define PX_GH   78
static uint16_t pxGrid[PX_GW * PX_GH];        // 12.2 KiB BSS，仅 style 6/7 使用
static uint8_t  pxMask[PX_GW * PX_GH];        // 骑手 sprite 的 1 格轮廓掩膜
static bool     pxTrack = false;
static uint32_t pxSeed  = 7;

static inline uint32_t pxRand() {             // 每帧重置 seed → 楼群/星形不跳
  pxSeed = pxSeed * 1103515245u + 12345u;
  return (pxSeed >> 16) & 0x7FFF;
}
static inline void pxSet(int x, int y, uint16_t c) {
  if ((unsigned)x >= PX_GW || (unsigned)y >= PX_GH) return;
  pxGrid[y * PX_GW + x] = c;
  if (pxTrack) pxMask[y * PX_GW + x] = 1;
}
static inline void pxRect(int x, int y, int w, int h, uint16_t c) {
  for (int yy = y; yy < y + h; yy++)
    for (int xx = x; xx < x + w; xx++) pxSet(xx, yy, c);
}
static inline void pxDisc(int cx, int cy, int r, uint16_t c) {
  for (int y = cy - r; y <= cy + r; y++)
    for (int x = cx - r; x <= cx + r; x++)
      if ((x - cx) * (x - cx) + (y - cy) * (y - cy) <= r * r) pxSet(x, y, c);
}
// 浮点半径版（月/日的光盘），与效果图 disc() 的采样范围一致
static inline void pxDiscF(float cx, float cy, float r, uint16_t c) {
  for (int y = (int)(cy - r) - 1; y <= (int)(cy + r) + 1; y++)
    for (int x = (int)(cx - r) - 1; x <= (int)(cx + r) + 1; x++)
      if ((x - cx) * (x - cx) + (y - cy) * (y - cy) <= r * r) pxSet(x, y, c);
}
static inline void pxLine(int x0, int y0, int x1, int y1, uint16_t c, int ww = 1) {
  int dx = x1 - x0, dy = y1 - y0;
  int n = (abs(dx) > abs(dy) ? abs(dx) : abs(dy)) + 1;
  for (int i = 0; i <= n; i++)
    for (int k = 0; k < ww; k++)
      pxSet(x0 + dx * i / n, y0 + dy * i / n + k, c);
}

static void dynBgRenderPixelCity(uint32_t t, bool day) {
  uint32_t ph     = dbgPhase(t, 6400);
  int      frame  = (int)((t / 320) % 2);      // 骑手 2 帧 bob / 尾气
  int      scroll = (int)((t / 260) % 8);      // 路面虚线 8 相位
  int      spoke  = (int)((t / 400) % 4);      // 轮辐 4 相位
  int      cloud  = (int)((t / 900) % 11);     // 白天飘云 11 相位
  pxTrack = false;

  // ① 天空：4 段色阶 + 稀疏噪点（文字区低对比纯色）
  // 白天顶部仍保留中深蓝：固件待机页文字固定白色，通亮晴空会让大时钟读不清。
  static const uint8_t SKY_N[4][3] = {
    {0x08,0x0C,0x1C},{0x0D,0x14,0x30},{0x16,0x21,0x41},{0x1E,0x2A,0x4E}};
  static const uint8_t SKY_D[4][3] = {
    {0x1E,0x4A,0x74},{0x31,0x69,0x9A},{0x57,0x98,0xC4},{0x9B,0xCE,0xE7}};
  const uint8_t (*SKY)[3] = day ? SKY_D : SKY_N;
  uint16_t rowC[PX_GH], rowHi[PX_GH], rowLo[PX_GH];
  for (int y = 0; y < PX_GH; y++) {
    int R, G, B;
    if (y <= 2)       { R = SKY[0][0]; G = SKY[0][1]; B = SKY[0][2]; }
    else if (y >= 34) { R = SKY[3][0]; G = SKY[3][1]; B = SKY[3][2]; }
    else {
      int q = (y - 2) * 768 / 32, i = q >> 8, f = q & 255;
      if (i > 2) { i = 2; f = 255; }
      R = SKY[i][0] + ((SKY[i+1][0] - SKY[i][0]) * f >> 8);
      G = SKY[i][1] + ((SKY[i+1][1] - SKY[i][1]) * f >> 8);
      B = SKY[i][2] + ((SKY[i+1][2] - SKY[i][2]) * f >> 8);
    }
    rowC[y]  = dbgRGB(R, G, B);
    rowHi[y] = dbgRGB(R + 5, G + 5, B + 5);
    rowLo[y] = dbgRGB(R - 5, G - 5, B - 5);
  }
  const int noiseTh = day ? 18 : 30;
  for (int y = 0; y < PX_GH; y++)
    for (int x = 0; x < PX_GW; x++) {
      uint32_t h = (uint32_t)x * 73856093u ^ (uint32_t)y * 19349663u;
      uint16_t c = rowC[y];
      if ((h & 1023) < (uint32_t)noiseTh) c = (h & 1) ? rowHi[y] : rowLo[y];
      pxSet(x, y, c);
    }

  // ② 夜：星 + 冷白弦月   /   昼：暖黄太阳 + 8 向日芒 + 飘云（同一位置 47,27）
  const uint16_t STAR[3] = { dbgRGB(0x7F,0x92,0xBD), dbgRGB(0x9D,0xB4,0xDC),
                             dbgRGB(0xC8,0xD6,0xF2) };
  if (!day) {
    const uint16_t C_STARDIM = dbgRGB(0x3A,0x47,0x6B);
    pxSeed = 11;
    for (int k = 0; k < 42; k++) {
      int x = 2 + (int)(pxRand() % 74), y = 2 + (int)(pxRand() % 32);
      if (13 < x && x < 65 && 6 < y && y < 23) continue;    // 让开大时钟
      uint16_t c = STAR[pxRand() % 3];
      if ((pxRand() + (ph >> 4)) % 7 == 0) c = C_STARDIM;
      pxSet(x, y, c);
    }
    const int BX[5] = {10,32,70,6,72}, BY[5] = {6,4,8,30,30};
    for (int i = 0; i < 5; i++)
      pxSet(BX[i], BY[i], ((i + (int)(ph >> 5)) % 4 == 0) ? STAR[1] : STAR[2]);

    // 弦月：亮盘 - 右上偏移挖盘 → 留下左下 C 形；冷白取代原来的暖黄满圆
    const uint16_t MOON      = dbgRGB(0xE8,0xF1,0xFA);
    const uint16_t MOON_EDGE = dbgRGB(0xB9,0xCD,0xE4);
    const uint16_t MOON_PIT  = dbgRGB(0xC3,0xD5,0xE8);
    const uint16_t HALO      = dbgRGB(0x35,0x49,0x74);
    const float mx = 47.0f, my = 27.0f, mr = 4.6f, hx = mx + 2.6f, hy = my - 1.5f;
    for (int yy = (int)my - 7; yy <= (int)my + 7; yy++)
      for (int xx = (int)mx - 7; xx <= (int)mx + 7; xx++) {
        float d2 = (xx - mx) * (xx - mx) + (yy - my) * (yy - my);
        if (d2 <= (mr + 2.2f) * (mr + 2.2f) && d2 > (mr + 0.6f) * (mr + 0.6f)
            && ((xx * 7 + yy * 13) & 3) == 0) pxSet(xx, yy, HALO);
      }
    for (int yy = (int)my - 6; yy <= (int)my + 6; yy++)
      for (int xx = (int)mx - 6; xx <= (int)mx + 6; xx++) {
        if ((xx - mx) * (xx - mx) + (yy - my) * (yy - my) <= mr * mr)
          pxSet(xx, yy, MOON);
        if ((xx - hx) * (xx - hx) + (yy - hy) * (yy - hy) <= 4.3f * 4.3f) {
          int ry = yy; if (ry < 0) ry = 0; if (ry >= PX_GH) ry = PX_GH - 1;
          pxSet(xx, yy, rowC[ry]);
        }
      }
    static const int PIT[2][2] = {{44,28},{46,30}};
    for (int i = 0; i < 2; i++)
      pxSet(PIT[i][0], PIT[i][1],
            ((PIT[i][0] + PIT[i][1]) % 2) ? MOON_PIT : MOON_EDGE);
    pxSet(53, 24, STAR[2]);                 // 月牙旁两颗伴星
    pxSet(41, 22, STAR[1]);
  } else {
    // 飘云：全部避开大时钟 (gx13..65 / gy6..23) 与左右槽文字
    const uint16_t CLOUD    = dbgRGB(0xF2,0xFA,0xFC);
    const uint16_t CLOUD_SH = dbgRGB(0xC3,0xDE,0xEC);
    static const int CB[3][4] = {{36,3,7,1},{26,29,7,0},{36,34,5,0}};
    for (int i = 0; i < 3; i++) {
      int bx = CB[i][0], by = CB[i][1], w = CB[i][2];
      int x0 = bx + (CB[i][3] ? ((cloud / 2) - 2) : (cloud - 5));
      if (x0 < 0 || x0 >= PX_GW) continue;
      pxRect(x0, by, w, 2, CLOUD);
      pxRect(x0 + 1, by - 1, (w - 3 > 2 ? w - 3 : 2), 1, CLOUD);
      pxRect(x0 + 2, by - 2, 2, 1, CLOUD);
      pxRect(x0 + 1, by + 2, (w - 2 > 2 ? w - 2 : 2), 1, CLOUD_SH);
    }
    // 太阳：暖黄圆盘 + 8 向像素光芒（随相位轻微呼吸）
    const uint16_t SUN_EDGE = dbgRGB(0xFF,0xE3,0x9C);
    const uint16_t SUN_CORE = dbgRGB(0xFF,0xD0,0x5C);
    const uint16_t SUN_RAY  = dbgRGB(0xFF,0xEE,0xB8);
    const float sx = 47.0f, sy = 27.0f;
    int ray = 1 + ((dbgSinQ((int32_t)ph) > 0) ? 1 : 0);
    for (int ang = 0; ang < 360; ang += 45) {
      float a = (float)ang * 0.0174533f;
      for (int k = 1; k <= ray; k++)
        pxSet((int)lroundf(sx + cosf(a) * (6.2f + k)),
              (int)lroundf(sy + sinf(a) * (6.2f + k)), SUN_RAY);
    }
    pxDiscF(sx, sy, 5.2f, SUN_EDGE);
    pxDiscF(sx, sy, 4.0f, SUN_CORE);
    pxSet((int)sx - 2, (int)sy - 2, dbgRGB(0xFF,0xE8,0xA8));
    pxSet((int)sx + 1, (int)sy + 2, dbgRGB(0xF6,0xBB,0x4A));
  }

  // ③ 楼群 + 窗（夜：霓虹灯窗 / 昼：冷白玻璃反光 + 亮边）；两版共用同一 LCG 序列
  uint16_t WIN[3], COLE[2], CTOP = 0;
  if (day) {
    WIN[0] = dbgRGB(0xE9,0xF5,0xF8); WIN[1] = dbgRGB(0xB9,0xD7,0xE6);
    WIN[2] = dbgRGB(0xFF,0xFF,0xF2);
    COLE[0] = dbgRGB(0x6C,0x92,0xB0); COLE[1] = dbgRGB(0x3F,0x63,0x82);
    CTOP = dbgRGB(0xA8,0xC8,0xDA);
  } else {
    WIN[0] = dbgRGB(0xE8,0xBD,0x68); WIN[1] = dbgRGB(0x4F,0xBF,0xAE);
    WIN[2] = dbgRGB(0xD9,0x7A,0xA4);
    COLE[0] = dbgRGB(0x16,0x22,0x4A); COLE[1] = dbgRGB(0x0C,0x13,0x30);
  }
  pxSeed = 3;
  for (int layer = 0; layer < 2; layer++) {
    int base = layer ? 50 : 46;
    uint16_t col = COLE[layer];
    int wp = layer ? 9 : 13;
    int x = 0;
    while (x < PX_GW) {
      int w = 3 + (int)(pxRand() % 5), h = 4 + (int)(pxRand() % 6);
      pxRect(x, base - h, w, h, col);
      if (day) pxRect(x, base - h, w, 1, CTOP);
      for (int yy = (38 > base - h + 1 ? 38 : base - h + 1); yy < base - 1; yy++)
        for (int xx = x + 1; xx < x + w - 1; xx++)
          if ((int)(pxRand() % 100) < wp) {
            uint16_t wc = WIN[pxRand() % 3];
            if ((pxRand() + (ph >> 5)) % 11 == 0) wc = col;
            pxSet(xx, yy, wc);
          }
      x += w + (int)(pxRand() % 2);
    }
  }

  // ④ 招牌（夜：霓虹呼吸 / 昼：遮阳棚与广告牌）
  const int NX[3] = {10,35,64}, NY[3] = {41,39,42};
  uint16_t NC[3], ND[3];
  if (day) {
    NC[0] = dbgRGB(0xE8,0x7B,0x5E); NC[1] = dbgRGB(0x4E,0xAA,0x9C);
    NC[2] = dbgRGB(0x7C,0x89,0xC6);
    ND[0] = dbgRGB(0xA9,0x56,0x45); ND[1] = dbgRGB(0x37,0x7A,0x72);
    ND[2] = dbgRGB(0x56,0x5E,0x90);
  } else {
    NC[0] = dbgRGB(0xE0,0x45,0x7B); NC[1] = dbgRGB(0x38,0xD0,0xC0);
    NC[2] = dbgRGB(0x7A,0x6C,0xF0);
    ND[0] = dbgRGB(0x5A,0x22,0x3C); ND[1] = dbgRGB(0x1C,0x5A,0x55);
    ND[2] = dbgRGB(0x35,0x30,0x68);
  }
  for (int i = 0; i < 3; i++) {
    uint16_t c = (dbgSinQ((int32_t)ph + i * 80) > 0) ? NC[i] : ND[i];
    pxRect(NX[i], NY[i], 4, 2, c);
    pxRect(NX[i] + 1, NY[i] + 2, 2, 1, c);
  }

  // ⑤ 地面 / 道路 / 滚动虚线（全部收在 gy48..78，让开下方槽）；同一 8 相位
  if (day) {
    pxRect(0, 48, PX_GW, 2,  dbgRGB(0xC8,0xD3,0xD8));
    pxRect(0, 50, PX_GW, 8,  dbgRGB(0x93,0xA2,0xAC));
    for (int x = -8; x < PX_GW + 8; x += 8)
      pxRect(x + scroll + 1, 54, 4, 1, dbgRGB(0xF7,0xF6,0xE8));
    pxRect(0, 58, PX_GW, 8,  dbgRGB(0x82,0x92,0x9C));
    pxRect(0, 66, PX_GW, 12, dbgRGB(0x70,0x80,0x8B));
  } else {
    pxRect(0, 48, PX_GW, 2,  dbgRGB(0x2B,0x36,0x58));
    pxRect(0, 50, PX_GW, 8,  dbgRGB(0x1A,0x21,0x39));
    for (int x = -8; x < PX_GW + 8; x += 8)
      pxRect(x + scroll + 1, 54, 4, 1, dbgRGB(0xF0,0xBF,0x55));
    pxRect(0, 58, PX_GW, 8,  dbgRGB(0x11,0x18,0x30));
    pxRect(0, 66, PX_GW, 12, dbgRGB(0x0B,0x11,0x26));
  }

  // ⑥ 速度线
  uint16_t sp1 = day ? dbgRGB(0xFF,0xFF,0xF4) : dbgRGB(0x4D,0xE0,0xC8);
  uint16_t sp2 = day ? dbgRGB(0xFF,0xD9,0x92) : dbgRGB(0x6F,0x7A,0xE0);
  pxRect(2 + (int)((t / 700) % 3), 40, 8 + (int)((t / 500) % 3), 1, sp1);
  pxRect(2, 43, 5, 1, sp2);
  pxRect(2, 46, 10, 1, sp1);

  // ⑦ 骑手 sprite（36×22 格）+ 1 格深色轮廓；两版同一骨架、不同配色
  uint16_t C_TIRE, C_RIM, C_BODY, C_BODY2, C_BOX, C_BOX2, C_HELM, C_SUIT;
  uint16_t C_SKIN, C_PANT, C_DARK, C_PUFF, C_OUT, C_BOXHI, C_BODYHI, C_LAMP, C_HELMV;
  if (day) {
    C_TIRE = dbgRGB(0x3B,0x4A,0x42); C_RIM  = dbgRGB(0xEE,0xF4,0xE8);
    C_BODY = dbgRGB(0xD8,0x44,0x3C); C_BODY2 = dbgRGB(0xEF,0x6A,0x52);
    C_BOX  = dbgRGB(0x4E,0x6B,0x3A); C_BOX2  = dbgRGB(0x66,0x86,0x4A);
    C_HELM = dbgRGB(0x4C,0xBF,0x7A); C_SUIT  = dbgRGB(0x39,0x9A,0x61);
    C_SKIN = dbgRGB(0xE8,0xB4,0x8C); C_PANT  = dbgRGB(0xF4,0xE5,0xC8);
    C_DARK = dbgRGB(0x4D,0x5C,0x46); C_PUFF  = dbgRGB(0xC9,0xD6,0xB4);
    C_OUT  = dbgRGB(0x2B,0x3E,0x52);
    C_BOXHI = dbgRGB(0xE9,0xE2,0xC8); C_BODYHI = dbgRGB(0xF8,0xDD,0xD4);
    C_LAMP = dbgRGB(0xF6,0xE0,0x8A); C_HELMV = dbgRGB(0x28,0x54,0x3A);
  } else {
    C_TIRE = dbgRGB(0x22,0x2C,0x48); C_RIM  = dbgRGB(0xC3,0xD2,0xEE);
    C_BODY = dbgRGB(0xE0,0x47,0x8A); C_BODY2 = dbgRGB(0xFF,0x79,0xB4);
    C_BOX  = dbgRGB(0x2C,0x6A,0x63); C_BOX2  = dbgRGB(0x41,0x94,0x85);
    C_HELM = dbgRGB(0x43,0xD3,0xAD); C_SUIT  = dbgRGB(0x2D,0x9B,0x82);
    C_SKIN = dbgRGB(0xF0,0xC3,0x9C); C_PANT  = dbgRGB(0xE8,0xDC,0xC3);
    C_DARK = dbgRGB(0x11,0x1A,0x34); C_PUFF  = dbgRGB(0x46,0x56,0x7A);
    C_OUT  = dbgRGB(0x06,0x0B,0x1C);
    C_BOXHI = dbgRGB(0x8F,0xD8,0xC8); C_BODYHI = dbgRGB(0xFF,0xD0,0xE4);
    C_LAMP = dbgRGB(0xFF,0xD3,0x6A); C_HELMV = dbgRGB(0x16,0x30,0x2A);
  }
  const int ox = 21, oy = 33 + (frame ? -1 : 0);
  #define PR(a,b,w,h,c)    pxRect(ox+(a), oy+(b), (w), (h), (c))
  #define PD(a,b,r,c)      pxDisc(ox+(a), oy+(b), (r), (c))
  #define PL(a,b,c,d,e,ww) pxLine(ox+(a), oy+(b), ox+(c), oy+(d), (e), (ww))
  #define PP(a,b,c)        pxSet(ox+(a), oy+(b), (c))
  memset(pxMask, 0, sizeof(pxMask));
  pxTrack = true;
  for (int i = 0; i < 3; i++) PR(-1 - i * 2, 15 + (i % 2) + frame, 2, 1, C_PUFF);
  PR(2, 7, 9, 7, C_BOX);   PR(3, 8, 7, 5, C_BOX2);
  PR(3, 9, 7, 1, C_BOXHI); PR(2, 13, 9, 1, C_DARK);
  PR(9, 10, 5, 1, C_DARK); PR(12, 10, 8, 3, C_BODY2);
  PR(13, 11, 5, 1, C_BODYHI); PR(11, 13, 17, 3, C_BODY);
  PR(10, 16, 20, 1, C_DARK);
  PR(26, 8, 3, 7, C_BODY); PR(27, 9, 2, 2, C_BODY2);
  PR(29, 10, 2, 2, C_LAMP);
  PL(27, 8, 30, 7, C_DARK, 1); PL(28, 15, 30, 10, C_DARK, 1);
  static const int8_t SP[8][2] = {{3,0},{2,2},{0,3},{-2,2},{-3,0},{-2,-2},{0,-3},{2,-2}};
  for (int wi = 0; wi < 2; wi++) {
    int wx = wi ? 28 : 8;
    PD(wx, 18, 4, C_TIRE); PD(wx, 18, 3, C_RIM); PD(wx, 18, 1, C_DARK);
    for (int a = 0; a < 8; a++) {
      int k = (a + spoke) & 7;
      PP(wx + SP[k][0], 18 + SP[k][1], C_DARK);
    }
  }
  PD(19, 4, 3, C_HELM); PR(20, 3, 3, 2, C_HELMV);
  PL(17, 7, 19, 6, C_SUIT, 2);
  PR(14, 8, 5, 4, C_SUIT); PR(14, 8, 2, 3, C_HELM);
  PL(18, 9, 27, 9, C_SKIN, 1);
  PR(16, 12, 4, 2, C_PANT); PL(19, 13, 24, 16, C_PANT, 1);
  PR(23, 16, 3, 1, C_DARK);
  if (!day) PR(14, 8, 1, 3, dbgRGB(0x6F,0xF0,0xCB));
  PR(2, 7, 9, 1, day ? dbgRGB(0x9C,0xC6,0x76) : dbgRGB(0x5F,0xD6,0xBD));
  pxTrack = false;
  #undef PR
  #undef PD
  #undef PL
  #undef PP
  for (int y = 0; y < PX_GH; y++)
    for (int x = 0; x < PX_GW; x++) {
      if (!pxMask[y * PX_GW + x]) continue;
      if (x > 0       && !pxMask[y * PX_GW + x - 1]) pxSet(x - 1, y, C_OUT);
      if (x < PX_GW-1 && !pxMask[y * PX_GW + x + 1]) pxSet(x + 1, y, C_OUT);
      if (y > 0       && !pxMask[(y-1) * PX_GW + x]) pxSet(x, y - 1, C_OUT);
      if (y < PX_GH-1 && !pxMask[(y+1) * PX_GW + x]) pxSet(x, y + 1, C_OUT);
    }

  // ⑧ 6px 网格展开到 466×466 PSRAM 帧（左上裁 1px，与确认稿 crop 一致）
  for (int gy = 0; gy < PX_GH; gy++) {
    int Y0 = gy * PX_CELL - 1;
    for (int gx = 0; gx < PX_GW; gx++) {
      int X0 = gx * PX_CELL - 1;
      uint16_t c = pxGrid[gy * PX_GW + gx];
      for (int yy = 0; yy < PX_CELL; yy++) {
        int Y = Y0 + yy;
        if (Y < 0 || Y >= 466) continue;
        uint16_t *row = dynBgBuf + (size_t)Y * 466;
        for (int xx = 0; xx < PX_CELL; xx++) {
          int X = X0 + xx;
          if (X >= 0 && X < 466) row[X] = c;
        }
      }
    }
  }
}

// 把一帧渲染进 dynBgBuf。只在待机页 animateCat() 的一拍里调用；
// 上传自定义壁纸 / style 0 / 总开关关掉时直接返回，dynBgBuf 保留但不参与 bgSrc()。
void dynBgRenderFrame(unsigned long now) {
  if (!dynBgActive()) return;
  if (!dynBgBuf) {
    dynBgBuf = (uint16_t *)ps_malloc(466 * 466 * 2);
    if (!dynBgBuf) return;
  }
  dbgInitSin();
  uint32_t t = (uint32_t)(((uint64_t)now * (dynBgSpeed ? dynBgSpeed : 18)) / 10);
  // 加快后的周期（×1.8 时）：呼吸 3.2s / 极光 8.3s / 星尘 9.9s / 光扫 6.2s / 涟漪 3.5s
  uint32_t phBreath = dbgPhase(t, 5760), phAurora = dbgPhase(t, 14940),
           phDust   = dbgPhase(t, 17820), phSweep = dbgPhase(t, 11160),
           phRipple = dbgPhase(t, 6300);
  // musicA.40：辉光类待机动态背景始终深色（取消白天浅底 / 昼夜自动）。
  // FO-220：style 7 是独立的「像素白天」主题，由用户手动选择，不跟系统昼夜走。
  const bool night = true;
  (void)dynBgAuto; (void)nightOn;
  const int style = dynBgStyle;
  // FO-217 / FO-220：像素夜景 / 像素白天走独立调色板网格，不共用下面 117×117 的辉光公式
  if (style == 6 || style == 7) { dynBgRenderPixelCity(t, style == 7); return; }

  for (int gy = 0; gy < DBG_GH; gy++) {
    int dy = (gy - DBG_GH / 2) * 2;
    int dy2 = dy * dy;
    for (int gx = 0; gx < DBG_GW; gx++) {
      int dx = (gx - DBG_GW / 2) * 2;
      int32_t r2 = (int32_t)dx * dx + dy2;                 // 0..~27000
      int32_t fall = (32768 * 3000) / (3000 + r2);         // Q15 中心衰减
      int32_t I1 = 0, I2 = 0, I3 = 0;                      // 暖金 / 冷蓝 / 紫
      switch (style) {
        case 1: {                                          // 呼吸金纹：整片金色缓慢明暗 + 细纹
          int32_t b = 16384 + (dbgSinQ(phBreath) >> 1);
          int32_t tex = 16384 + (dbgSinQ(dx * 3 + dy * 2 + (int32_t)phBreath * 2) >> 1);
          I1 = (int32_t)(((int64_t)b * fall >> 15) * tex >> 15);
          I2 = I1 >> 2; I3 = I1 >> 3;
        } break;
        case 2: {                                          // 流动极光：三条相位错开的带状波
          int32_t w1 = dbgSinQ(dx * 2 + dy * 3 + (int32_t)phAurora);
          int32_t w2 = dbgSinQ(dx * 2 - dy * 2 + (int32_t)phAurora + 85);
          int32_t w3 = dbgSinQ(dy * 3 + (int32_t)phAurora + 170);
          I1 = (int32_t)(((w1 + 32768) >> 1) * (int64_t)fall >> 15);
          I2 = (int32_t)(((w2 + 32768) >> 1) * (int64_t)fall >> 15);
          I3 = (int32_t)(((w3 + 32768) >> 1) * (int64_t)fall >> 15);
        } break;
        case 3: {                                          // 星尘粒子：底子一层极缓辉光，星点在后面叠
          int32_t glow = 9000 + (dbgSinQ(phDust) >> 2);
          I1 = (int32_t)(glow * (int64_t)fall >> 15);
          I2 = I1; I3 = 0;
        } break;
        case 4: {                                          // 斜向光扫：45° 光带周期性扫过
          int32_t sweepPos = (int32_t)phSweep * 2 - 256;
          int32_t d = (dx + dy) - sweepPos;
          int32_t band = (32768 * 900) / (900 + d * d / 2);
          I1 = (int32_t)(band * (int64_t)fall >> 15);
          I2 = I1 >> 2; I3 = I1 >> 3;
        } break;
        case 5: {                                          // 同心涟漪：由内向外扩散的环
          int32_t w = dbgSinQ(r2 / 60 - (int32_t)phRipple * 2);
          I1 = (int32_t)(((w + 32768) >> 1) * (int64_t)fall >> 15);
          I2 = I1 >> 1; I3 = I1 >> 2;
        } break;
        default: break;
      }
      int32_t vig = (r2 > 12000) ? (r2 - 12000) / 220 : 0;   // 边缘暗角，贴合圆屏
      int R, G, B;
      if (night) {                                       // 深底 + screen 辉光
        R = 8  + ((I1 * 170 + I2 * 45  + I3 * 100) >> 15) - vig / 3;
        G = 11 + ((I1 * 115 + I2 * 95  + I3 * 55)  >> 15) - vig / 3;
        B = 18 + ((I1 * 45  + I2 * 175 + I3 * 165) >> 15) - vig / 2;
      } else {                                           // 浅底 + multiply 暗纹（额外提对比）
        R = 244 - ((I1 * 78 + I2 * 34 + I3 * 44) >> 15) - vig / 2;
        G = 242 - ((I1 * 56 + I2 * 50 + I3 * 38) >> 15) - vig / 2;
        B = 235 - ((I1 * 28 + I2 * 76 + I3 * 66) >> 15) - vig / 2;
      }
      uint16_t c = dbgRGB(R, G, B);
      uint16_t *row0 = dynBgBuf + (size_t)(gy * DBG_CELL) * 466 + gx * DBG_CELL;
      for (int yy = 0; yy < DBG_CELL; yy++) {
        int Y = gy * DBG_CELL + yy;
        if (Y >= 466) break;
        uint16_t *row = row0 + (size_t)yy * 466;
        int maxxx = gx * DBG_CELL + DBG_CELL <= 466 ? DBG_CELL : 466 - gx * DBG_CELL;
        for (int xx = 0; xx < maxxx; xx++) row[xx] = c;
      }
    }
  }

  if (style == 3) {                                     // 星尘：24 个缓慢漂移的亮点（全分辨率）
    for (int k = 0; k < 24; k++) {
      int seed = k * 97 + 13;
      int X = (seed * 37 % 396) + 35 + (int)(dbgSinQ((int32_t)phDust + k * 11) / 2000);
      int Y = (seed * 61 % 396) + 35 + (int)(dbgSinQ((int32_t)phDust + k * 11 + 64) / 2600);
      if (X < 3 || X > 462 || Y < 3 || Y > 462) continue;
      int b = 150 + (dbgSinQ((int32_t)phDust * 2 + k * 7) >> 9);
      uint16_t c = night ? dbgRGB(b, (b * 9) / 10, (b * 6) / 10)
                         : dbgRGB(150 - b / 3, 125 - b / 3, 70);
      for (int oy = -1; oy <= 1; oy++)
        for (int ox = -1; ox <= 1; ox++) {
          if (abs(ox) + abs(oy) > 1) continue;           // 十字形小星点
          dynBgBuf[(size_t)(Y + oy) * 466 + X + ox] = c;
        }
    }
  }
}
