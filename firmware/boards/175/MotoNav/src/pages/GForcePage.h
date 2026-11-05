#pragma once
// v7.12.9-ui.6 上划：G-FORCE 速度页（像素风，对应效果图 G 页定稿）。
// 数据来源（ui.10）：手机 #G / #v 优先，其次板载 IMU，都没有显示 0——不用模拟数据。
// musicA.22：整合压弯倾角——左下 LEAN 实时值（带左右倾向三角）、右下 MAX LEAN 本次最大 +
//           左右弯次数、底部由单轨改为 G 值 / 倾角双轨；电量栏与顶部状态重复，已移除。
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

// ================= musicA.22 压弯倾角 =================
// 思路：不用 atan2(ax,az) 直接算——过弯时离心力会被加速度计当成重力分量，60km/h 半径 50m
// 就有约 0.57g 横向加速度，能把读数虚抬 30°。所以主值走陀螺积分（与安装方向无关），
// 只在"没有动态加速度"（合加速度≈1g）且慢速倾斜时，才让加速度计做慢修正把零漂拉回。
// 采样 20Hz 由主循环驱动（页面绘制只有 2Hz，用绘制节奏积分会严重失真）。
float leanDeg = 0;                          // 当前倾角（正=右倾，负=左倾）
float leanMax = 0;                          // 本次最大倾角（绝对值）
int   leanCntL = 0, leanCntR = 0;           // 左弯 / 右弯次数
bool  leanInTurn = false;                   // 弯中标志（>20° 进入，<10° 退出，防抖）
bool  leanReady = false;                    // 重力基准已校准
float leanHist[60]; int leanHistN = 0;      // 倾角轨迹（与 gHist 同节奏）
unsigned long leanSampleAt = 0;             // 主循环 20Hz 节流

static float leanZeroX = 0, leanZeroY = 0, leanZeroZ = 0;   // 静止时记住的重力矢量
static unsigned long leanStillAt = 0;                       // 静止计时
static unsigned long leanLastMs = 0;                        // 上次采样时刻（算 dt）
static int leanAxis = -1;                                   // 倾斜轴 0=X 1=Y 2=Z（首次倾斜时自动锁定）
#define LEAN_SIGN (+1.0f)                   // 实机若左右方向反了，把这里改成 -1.0f

void leanResetStats() {
  leanMax = 0; leanCntL = 0; leanCntR = 0;
  leanInTurn = false; leanHistN = 0;
}

// 20Hz 采样：读 IMU → 积分倾角 → 判定弯道。主循环在 G 页可见时调用。
void leanSample() {
  if (!imuOK) return;
  float g, dps;
  if (!imuPoll(g, dps)) return;             // 顺带刷新 imuAX/imuRX 等原始量

  unsigned long now = millis();
  float dt = leanLastMs ? (now - leanLastMs) / 1000.0f : 0.05f;
  leanLastMs = now;
  if (dt > 0.30f) dt = 0.05f;               // 卡顿后不积一大步
  if (dt < 0.005f) return;

  float ax = imuAX, ay = imuAY, az = imuAZ;
  float am = sqrtf(ax * ax + ay * ay + az * az);
  if (am < 0.2f || am > 3.0f) return;       // 明显异常值直接丢

  // 静止 1 秒 → 记住重力基准（开机或第一次进 G 页时自动完成，与安装角度无关）
  if (!leanReady) {
    if (dps < 8.0f && fabsf(am - 1.0f) < 0.20f) {
      if (!leanStillAt) leanStillAt = now;
      if (now - leanStillAt > 1000) {
        leanZeroX = ax; leanZeroY = ay; leanZeroZ = az;
        leanReady = true;
      }
    } else {
      leanStillAt = 0;
    }
    return;
  }

  // 首次明显倾斜时锁定倾斜轴：绕"前进方向"的角速度模长最大，其余轴多是路面颠簸
  if (leanAxis < 0 && dps > 25.0f) {
    float mx = fabsf(imuRX), my = fabsf(imuRY), mz = fabsf(imuRZ);
    leanAxis = (mx >= my && mx >= mz) ? 0 : ((my >= mz) ? 1 : 2);
  }
  float rate = 0;
  if (leanAxis == 0)      rate = imuRX;
  else if (leanAxis == 1) rate = imuRY;
  else if (leanAxis == 2) rate = imuRZ;
  rate *= LEAN_SIGN;

  leanDeg += rate * dt;                     // 陀螺积分主导，不受离心力影响

  // 加速度计慢修正：只在 |a|≈1g（无横向加速度）且慢速倾斜时启用，避开过弯离心力污染
  float zm = sqrtf(leanZeroX * leanZeroX + leanZeroY * leanZeroY + leanZeroZ * leanZeroZ);
  if (zm > 0.2f) {
    float dot = (ax * leanZeroX + ay * leanZeroY + az * leanZeroZ) / (am * zm);
    dot = constrain(dot, -1.0f, 1.0f);
    float accAng = acosf(dot) * 57.2958f;
    if (fabsf(am - 1.0f) < 0.12f && fabsf(rate) < 12.0f) {
      float target = (leanDeg >= 0 ? accAng : -accAng);   // 只修正幅值，方向沿用陀螺
      leanDeg += 0.10f * (target - leanDeg);
    }
  }
  leanDeg = constrain(leanDeg, -85.0f, 85.0f);

  float ab = fabsf(leanDeg);
  if (ab > leanMax) leanMax = ab;
  if (!leanInTurn && ab > 20.0f) {          // 进入弯道：过 20° 记一次
    leanInTurn = true;
    if (leanDeg > 0) leanCntR++; else leanCntL++;
  } else if (leanInTurn && ab < 10.0f) {    // 回正到 10° 以下才算这个弯结束
    leanInTurn = false;
  }
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

// 角度数值 + 右上角小圆点当"°"（位图字体没有度数符号）
void gfAngle(float deg, int x, int y, int size, uint16_t c) {
  String s = String((int)lroundf(deg));
  printCentered(s, x, y, size, c);
  int w = s.length() * 6 * size;
  aaDisc(x + w / 2 + size * 2, y + size * 2, size, c);
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
  leanHist[leanHistN % 60] = leanReady ? leanDeg : 0; leanHistN++;

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

  // 左栏：PEAK G / LEAN 实时倾角（musicA.22：电量与顶部电池重复，改为倾角）
  printCentered("PEAK G", 92, 200, 2, GF_PK);
  printCentered(String(gPeak, 2), 92, 218, 4, C_WHITE);

  float ab = fabsf(leanDeg);
  bool leanHot = ab > 45.0f;                // 超过 45°：数字与三角一起转橙
  printCentered("LEAN", 92, 262, 1, GF_CY);
  if (leanReady) gfAngle(ab, 92, 276, 4, leanHot ? C_ORANGE : GF_CY);
  else           printCentered("--", 92, 276, 4, GF_DIM);
  if (leanReady && ab > 3.0f) {             // 倾向三角：只在真的有倾角时出现
    uint16_t tc = leanHot ? C_ORANGE : GF_CY;
    if (leanDeg < 0) gfx->fillTriangle(40, 284, 54, 292, 40, 300, tc);
    else             gfx->fillTriangle(132, 284, 118, 292, 132, 300, tc);
  }
  printCentered(String("ROT ") + String((int)lroundf(gRot)), 92, 322, 1, GF_DIM);

  // 右栏：MAX 车速（带 KM/H 小字）/ MAX LEAN 本次最大倾角 + 左右弯次数
  printCentered("MAX", 374, 200, 2, GF_PK);
  printCentered(String(gMaxSpd), 358, 218, 4, C_WHITE);
  gfx->setTextSize(1); gfx->setTextColor(GF_DIM); gfx->setCursor(396, 240); gfx->print("KM/H");

  printCentered("MAX LEAN", 374, 262, 1, GF_PK);
  if (leanReady) gfAngle(leanMax, 374, 276, 4, C_WHITE);
  else           printCentered("--", 374, 276, 4, GF_DIM);
  if (leanReady) printCentered(String("L ") + String(leanCntL) + "  R " + String(leanCntR),
                               374, 322, 1, GF_CY_L);

  // 底部双轨：上轨 G 值（粉，基线 376）/ 下轨倾角（青，中线 388，左弯向上、右弯向下）
  gfx->fillRect(95, 356, 2, 46, 0x8A2A6E);
  gfx->fillRect(369, 356, 2, 46, 0x8A2A6E);
  int n = gHistN < 60 ? gHistN : 60;
  int lx = -1, ly = -1;
  for (int i = 0; i < n; i++) {
    float v = gHist[(gHistN - n + i) % 60];
    int x = 98 + (int)((370.0f - 98.0f) * i / 59.0f);
    int y = 376 - (int)constrain(v * 20.0f, 0.0f, 18.0f);   // 动态 G 幅度小，放大轨迹
    if (lx >= 0) gfx->fillRect(min(lx, x), min(ly, y), abs(x - lx) + 2, abs(y - ly) + 2, GF_PK);
    lx = x; ly = y;
  }
  int pn = leanHistN < 60 ? leanHistN : 60;
  lx = -1; ly = -1;
  for (int i = 0; i < pn; i++) {
    float v = leanHist[(leanHistN - pn + i) % 60];
    int x = 98 + (int)((370.0f - 98.0f) * i / 59.0f);
    int y = 388 - (int)constrain(v * 0.22f, -9.0f, 9.0f);    // ±41° 满幅
    if (lx >= 0) gfx->fillRect(min(lx, x), min(ly, y), abs(x - lx) + 2, abs(y - ly) + 2, GF_CY);
    lx = x; ly = y;
  }

  // 底部双按钮：ZERO（清当前 G 与轨迹）/ RESET（清峰值、倾角统计与轨迹）；点按区域见 gTap
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
  if (x >= 144 && x <= 228) { gHistN = 0; leanHistN = 0; }        // ZERO：清轨迹
  else if (x >= 238 && x <= 322) {                                  // RESET：峰值 + 倾角统计 + 轨迹
    gPeak = 0; leanResetStats(); gHistN = 0;
  }
  else return;
  drawGForce();
}
