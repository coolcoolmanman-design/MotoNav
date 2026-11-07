#pragma once
// ================= musicA.37 预设动态背景（正式功能） =================
// 6 风格 × 昼夜两套，程序化渲染：零 BLE 传输、零文件存储。
// 466×466 逐像素浮点算一帧太重，这里用 117×117 低分辨率网格（每格 4×4 像素）
// + Q15 定点 + 256 项正弦表，每帧只算 1.37 万个网格点，10fps 下 CPU 占用很低。
// 辉光/极光/涟漪本来就是大面积缓变，4×4 放大后观感与全分辨率一致；星尘的锐利星点
// 在网格渲染之后直接叠到全分辨率缓冲上。
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
  // musicA.40：待机动态背景始终深色（取消白天浅底 / 昼夜自动）。
  const bool night = true;
  (void)dynBgAuto; (void)nightOn;
  const int style = dynBgStyle;

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
