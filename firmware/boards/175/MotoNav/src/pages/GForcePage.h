#pragma once
// v7.12.9-ui.6 上划：G-FORCE 速度页（像素风，对应效果图 G 页定稿）。
// 数据来源（ui.10）：手机 #G / #v 优先，其次板载 IMU，都没有显示 0——不用模拟数据。
// 由 MotoNav.ino 在 PageNavigation.h 之前包含，仅编译一次。
void touchVectorToCanvas(float &dx, float &dy);   // 定义在 PageNavigation.h（后包含，前置声明）

#define GF_BG    0x0023   // 深蓝黑 #06061E
#define GF_CY    0x37FF   // #35E0FF
#define GF_CY_L  0xCFDF   // #C8F8FF
#define GF_PK    0xF9F9   // #FF3DC8
#define GF_NEON  0x29F5   // #2B3FAE
#define GF_NEON2 0x4B1D   // #4A63E8
#define GF_DIM   0x7C39   // #7A86C8
#define GF_EDGE  0x3C93   // 表圈主刻度 #3A4A9A
#define GF_EDGE2 0x230D   // 表圈小刻度 #26306E
#define GF_PANEL 0x08A8   // 按钮 底 #0D1440

// v7.12.9-ui.10 数据来源（不再用模拟数据）：手机 #G（3 秒内收到过，标 PHONE）优先，
// 其次板载 IMU（Imu.h 探测，标 SENSOR），都没有就显示 0（标 NONE）。
// 车速同理：手机 #v 推送的 speedKmh，没有就 0。
float gCur = 0, gPeak = 0;                  // 当前 / 峰值 G
float gRot = 0;                             // 旋转角速度模长（°/s，手机推或板载陀螺）
unsigned long gRxAt = 0;                    // 最近一次收到 #G 的时刻
bool gLive() { return gRxAt && millis() - gRxAt < 3000; }
float gHist[60]; int gHistN = 0;            // 轨迹环形缓冲（500ms 一个采样，约 30 秒）
unsigned long gOpenedAt = 0;                // RIDE 计时起点
int gMaxSpd = 0;                            // 本次会话最高车速

void gfPixelCircle(int cx, int cy, int r, uint16_t c) {
  for (int a = 0; a < 360; a += 3) {
    float rad = a * 0.0174533f;
    gfx->fillRect(cx + (int)lroundf(r * cosf(rad)) - 1, cy + (int)lroundf(r * sinf(rad)) - 1, 2, 2, c);
  }
}

void gfBtn(int x0, int w, uint16_t edge, const char *lab, const char *sub, uint16_t lc) {
  gfx->fillRect(x0, 404, w, 34, GF_PANEL);
  gfx->drawRect(x0, 404, w, 34, edge);
  printCentered(String(lab), x0 + w / 2, 408, 2, lc);
  printCentered(String(sub), x0 + w / 2, 426, 1, GF_DIM);
}

void drawGForce() {
  onWaitScreen = false;
  if (!gOpenedAt) gOpenedAt = millis();
  int spd = speedKmh >= 0 ? speedKmh : 0;    // ui.10 无手机车速就显示 0
  if (speedKmh >= 0 && spd > gMaxSpd) gMaxSpd = spd;
  bool live = gLive();
  bool sensor = false;                       // 手机优先；手机没数据才用板载 IMU
  if (!live && imuOK) { float dg; sensor = imuPoll(gCur, dg); if (sensor && dg >= 0) gRot = dg; }
  if (!live && !sensor) gCur = 0;            // 都没数据：显示 0
  if (gCur > gPeak) gPeak = gCur;
  gHist[gHistN % 60] = gCur; gHistN++;

  gfx->fillScreen(GF_BG);
  drawStatus();

  // 顶部标题 + 超大 G 值
  printCentered("G-FORCE", CX, 72, 2, GF_CY);
  printCentered(String(gCur, 2), CX, 92, 10, C_WHITE);
  printCentered("G", 372, 118, 5, GF_CY);
  printCentered(live ? "PHONE" : (sensor ? "SENSOR" : "NONE"), 372, 162, 2,
                live ? GF_CY : (sensor ? GF_CY_L : C_ORANGE));   // 数据来源标注

  // 外圈表圈刻度（整圈，让位上下页点）
  for (int a = 0; a < 360; a += 6) {
    int d = (a + 90) % 360;
    if ((d > 70 && d < 110) || (d > 250 && d < 290)) continue;
    bool maj = a % 30 == 0;
    float rad = a * 0.0174533f;
    int r1 = maj ? 222 : 219;
    for (int r = 213; r <= r1; r++)
      gfx->fillRect(CX + (int)lroundf(r * cosf(rad)) - 1, CX + (int)lroundf(r * sinf(rad)) - 1, 2, 2, maj ? GF_EDGE : GF_EDGE2);
  }

  // 中央速度表盘：三同心圆 + 青色速度进度弧（135° 顺时针起，270° 满量程 180km/h）
  const int dcy = 280;
  for (const int r : {40, 60, 78}) gfPixelCircle(CX, dcy, r, GF_NEON);
  int sweep = (int)(270.0f * constrain(spd / 180.0f, 0.0f, 1.0f));
  for (int a = 135; a <= 135 + sweep; a += 2) {
    float rad = a * 0.0174533f;
    gfx->fillRect(CX + (int)lroundf(78 * cosf(rad)) - 3, dcy + (int)lroundf(78 * sinf(rad)) - 3, 6, 6, GF_CY);
    gfx->fillRect(CX + (int)lroundf(78 * cosf(rad)) - 1, dcy + (int)lroundf(78 * sinf(rad)) - 1, 3, 3, GF_CY_L);
  }
  { float rad = 135 * 0.0174533f;            // 弧尾起点白色标记
    gfx->fillRect(CX + (int)lroundf(78 * cosf(rad)) - 4, dcy + (int)lroundf(78 * sinf(rad)) - 4, 8, 8, C_WHITE); }
  for (const int a : {45, 135, 225, 315}) {  // 对角 / 方位刻度
    float rad = a * 0.0174533f;
    gfx->fillRect(CX + (int)lroundf(84 * cosf(rad)) - 1, dcy + (int)lroundf(84 * sinf(rad)) - 2, 3, 5, GF_NEON2);
    gfx->fillRect(CX + (int)lroundf(84 * cosf(rad)) - 2, dcy + (int)lroundf(84 * sinf(rad)) - 1, 5, 3, GF_NEON2);
  }

  // 速度数字 + 单位（ui.9 不再补前导零）
  printCentered(String(spd), CX, 256, 7, C_WHITE);
  printCentered("KM/H", CX, 316, 2, GF_CY);

  // RIDE 计时：v7.12.10-ui.1 移到速度表盘正上方居中（大 G 值与表盘顶的空档）
  int el = (millis() - gOpenedAt) / 1000;
  char rb[12]; snprintf(rb, sizeof(rb), "RIDE %02d:%02d", el / 60 % 100, el % 60);
  printCentered(String(rb), CX, 184, 1, GF_DIM);

  // 左栏：PEAK G / BATT（v7.12.10-ui.1 框架下移 16px，电量移入左栏与右栏逐行镜像）
  printCentered("PEAK G", 92, 200, 2, GF_PK);
  printCentered(String(gPeak, 2), 92, 218, 4, C_WHITE);
  printCentered("BATT", 92, 262, 1, GF_DIM);
  if (batPct >= 0) printCentered(String(batPct) + "%", 92, 276, 4, GF_CY);

  // 右栏：MAX 车速 / ROT D/S（与左栏同行对齐：标签 262，数值 276）
  printCentered("MAX", 374, 200, 2, GF_PK);
  printCentered(String(gMaxSpd), 374, 218, 4, C_WHITE);
  printCentered("ROT D/S", 374, 262, 1, GF_DIM);
  printCentered((sensor || live) ? String((int)lroundf(gRot)) : String("--"), 374, 276, 4, GF_CY);

  // 底部示波器式 G 值轨迹（最近 60 个采样）+ 两端括号
  gfx->fillRect(95, 362, 2, 24, 0x8A2A6E);
  gfx->fillRect(371, 362, 2, 24, 0x8A2A6E);
  int n = gHistN < 60 ? gHistN : 60;
  int lx = -1, ly = -1;
  for (int i = 0; i < n; i++) {
    float v = gHist[(gHistN - n + i) % 60];
    int x = 98 + (int)((370.0f - 98.0f) * i / 59.0f);
    int y = 374 - (int)constrain(v * 32.0f, 0.0f, 26.0f);   // ui.11 动态 G 幅度小，放大轨迹
    if (lx >= 0) gfx->fillRect(min(lx, x), min(ly, y), abs(x - lx) + 2, abs(y - ly) + 2, GF_PK);
    lx = x; ly = y;
  }
  char eb[12]; snprintf(eb, sizeof(eb), "ELAPSED %02d:%02d", el / 60 % 100, el % 60);
  printCentered(String(eb), 168, 394, 1, GF_DIM);

  // 底部双按钮：ZERO（清当前 G 与轨迹）/ RESET（清峰值与轨迹）；点按区域见 gTap
  gfBtn(144, 84, GF_CY, "ZERO", "TAP", GF_CY);
  gfBtn(238, 84, GF_PK, "RESET", "PEAK", GF_PK);

  drawPageDots();
  needFlush = true;
}

// 点按：ZERO / RESET 按钮（触摸坐标换算同 musicButtonAt）
void gTap(int x, int y) {
  float dx = x - CX, dy = y - CX;            // 圆屏 y 圆心 = CX（同 ProgressRings 约定）
  touchVectorToCanvas(dx, dy);
  x = (int)lroundf(CX + dx); y = (int)lroundf(CX + dy);
  if (y < 404 || y > 438) return;
  if (x >= 144 && x <= 228) { gHistN = 0; }                  // ZERO：清轨迹
  else if (x >= 238 && x <= 322) { gPeak = 0; gHistN = 0; }  // RESET：清峰值 + 轨迹
  else return;
  drawGForce();
}