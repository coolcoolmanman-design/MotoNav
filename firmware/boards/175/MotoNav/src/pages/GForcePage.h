#pragma once
// v7.12.9-ui.6 上划：G-FORCE 速度页（像素风，对应效果图 G 页定稿）。
// 数据来源：车速优先用手机 #v 推送的 speedKmh，没有时用模拟值；
// G 值固件无 IMU，暂为模拟数据（页面标注 SIM），后续可换手机 #G 通道。
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

float gCur = 0, gPeak = 0;                  // 当前 / 峰值 G（模拟）
float gHist[60]; int gHistN = 0;            // 轨迹环形缓冲（500ms 一个采样，约 30 秒）
unsigned long gOpenedAt = 0;                // RIDE 计时起点
int gMaxSpd = 0;                            // 本次会话最高车速

// 模拟数据：多正弦叠加，平滑无随机抖动
float gfSimG(float t) {
  float v = 0.92f + 0.34f * sinf(t / 3.1f) + 0.17f * sinf(t / 1.27f + 1.3f) + 0.08f * sinf(t / 0.47f);
  return constrain(v, 0.0f, 1.6f);
}
float gfSimSpd(float t) {
  float v = 64.0f + 40.0f * sinf(t / 7.3f) + 14.0f * sinf(t / 2.1f + 2.0f);
  return constrain(v, 0.0f, 180.0f);
}

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
  float t = millis() / 1000.0f;
  bool sim = speedKmh < 0;
  int spd = sim ? (int)lroundf(gfSimSpd(t)) : speedKmh;
  if (spd > gMaxSpd) gMaxSpd = spd;
  gCur = gfSimG(t);                          // TODO: 换手机 #G 通道
  if (gCur > gPeak) gPeak = gCur;
  gHist[gHistN % 60] = gCur; gHistN++;

  gfx->fillScreen(GF_BG);
  drawStatus();

  // 顶部标题 + 超大 G 值
  printCentered("G-FORCE", CX, 72, 2, GF_CY);
  printCentered(String(gCur, 2), CX, 92, 10, C_WHITE);
  printCentered("G", 372, 118, 5, GF_CY);
  printCentered("SIM", 372, 162, 2, C_ORANGE);      // G 值为模拟数据

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

  // 速度数字 + 单位
  char sb[8]; snprintf(sb, sizeof(sb), "%03d", spd);
  printCentered(String(sb), CX, 256, 7, C_WHITE);
  printCentered("KM/H", CX, 316, 2, GF_CY);

  // 左栏：PEAK G / RIDE
  printCentered("PEAK G", 92, 184, 2, GF_PK);
  printCentered(String(gPeak, 2), 92, 202, 4, C_WHITE);
  int el = (millis() - gOpenedAt) / 1000;
  char rb[12]; snprintf(rb, sizeof(rb), "RIDE %02d:%02d", el / 60 % 100, el % 60);
  printCentered(String(rb), 92, 246, 1, GF_DIM);

  // 右栏：MAX 车速 / BATT
  printCentered("MAX", 374, 184, 2, GF_PK);
  printCentered(String(gMaxSpd), 374, 202, 4, C_WHITE);
  printCentered("BATT", 374, 246, 1, GF_DIM);
  if (batPct >= 0) printCentered(String(batPct) + "%", 374, 260, 4, GF_CY);

  // 底部示波器式 G 值轨迹（最近 60 个采样）+ 两端括号
  gfx->fillRect(95, 362, 2, 24, 0x8A2A6E);
  gfx->fillRect(371, 362, 2, 24, 0x8A2A6E);
  int n = gHistN < 60 ? gHistN : 60;
  int lx = -1, ly = -1;
  for (int i = 0; i < n; i++) {
    float v = gHist[(gHistN - n + i) % 60];
    int x = 98 + (int)((370.0f - 98.0f) * i / 59.0f);
    int y = 374 - (int)constrain(v * 16.0f, 0.0f, 26.0f);
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