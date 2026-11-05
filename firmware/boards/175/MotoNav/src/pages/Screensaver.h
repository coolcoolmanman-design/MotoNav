#pragma once
// musicA.23 待机屏保：程序化眼睛动画（呼吸 / 随机眨眼 / 扫视 / 2.5D 轻微头转）
// musicA.24：亮度改为跟随用户设定（45%，40~120 夹紧）+ 320ms 渐变；加四层 IMU 互动
//            ①重力朝向跟随 ②敲击/猛晃惊跳 ③陀螺惯性滞后 ④睡意演化（犯困→半睡→打盹）
//
// 为什么不用图片序列帧：466×466 一帧 RGB565 就是 434KB，几十帧要几十 MB flash，
// 而且解码加整屏推送只能跑到约 12fps。改成纯代码绘制后不占存储空间，
// 配合 DisplayPipeline 的局部推送（flushRect）可以稳定在 30fps 以上。
//
// 三个必须守住的约束：
//   1. 每帧只刷「眼睛活动区」一个脏矩形，其余区域保持纯黑不动（这是帧率的全部来源）；
//   2. AMOLED 防烧屏：整体画面做极缓慢的圆周漂移 + 屏保亮度降到待机亮度的 45%；
//   3. 屏保期间触摸只做唤醒，绝不产生滑动手势（touchPoll 里提前返回）。
//
// 依赖：gfx / panel / C_BLACK / C_WHITE / CX（DeviceContext.h）、flushSync / flushRect
//       （DisplayPipeline.h）、drawWaiting / WAIT_* （HomePage.h）、
//       lastBrightness / screenOff（PowerKey.h）、imuOK / imuSample / imuAX..（Imu.h）。
//       须在 PowerKey.h 之后、IncomingCall.h 之前包含。

#define SAVER_IDLE_MS    60000UL    // 待机页静置多久进入屏保（DEBUG 时临时改小）
#define SAVER_DIM_PCT    45         // 屏保亮度 = 待机亮度 × 45%
#define SAVER_DIM_MIN    40         // 下限（再暗白眼睛也看得见）
#define SAVER_DIM_MAX    120        // 上限（夜间不至于刺眼）
#define SAVER_FRAME_MS   33         // 目标 30fps

#define SV_BRIGHT_MS     320        // 亮度渐变时长

// 眼睛几何（像素，针对 466×466）
#define SV_EYE_W     92             // 单眼基准宽
#define SV_EYE_H     112            // 单眼基准高
#define SV_EYE_GAP   172            // 两眼中心距
#define SV_EYE_CY    216            // 两眼中心 y

// 脏矩形：覆盖眼睛 + 呼吸缩放 + 惊跳放大 + 漂移 + 微扫视的全部活动范围
// 左右 74..390（宽 316），上下 134..298（高 164）→ 316×164×2 ≈ 104KB/帧
#define SV_BOX_X     74
#define SV_BOX_Y     132
#define SV_BOX_W     318
#define SV_BOX_H     168

// ---------------------------- IMU 互动参数 ----------------------------
#define SV_IMU_MS        50         // IMU 采样周期 = 20Hz（I2C 12 字节约 0.3ms，可忽略）
#define SV_TILT_DEAD     0.045f     // 倾角死区（重力偏移 g，约 2.6°）——水平放置时不抖
#define SV_TILT_FULL     0.42f      // 满偏（约 25°），超过不再加量
#define SV_TILT_LP       0.28f      // 倾角低通（20Hz 采样 → 约 1.6Hz），滤掉路面/发动机振动
#define SV_IMU_SX        1.0f       // 横轴方向（实机若反了改成 -1.0f）
#define SV_IMU_SY        1.0f       // 纵轴方向
#define SV_MOVE_G        0.22f      // 判"有人在动"的动态 g 阈值（重置睡意计时）
#define SV_SHOCK_G       0.62f      // 敲击/猛晃 ⇒ 惊跳（动态 g）
#define SV_SHOCK_DPS     200.0f     // 甩动角速度 ⇒ 惊跳（dps）
#define SV_SPIN_K        0.00005f   // 陀螺惯性滞后的注入系数

bool          svEatNext   = false;   // 唤醒时吃掉的那次触摸（touchPoll 消费）
bool          saverEnabled = true;   // musicA.25：屏保总开关（App 的「屏幕保护」开关，存 NVS "saver"）
bool          svBlinking  = false;
bool          svWinking   = false;   // 单眼眨
unsigned long svActAt     = 0;       // 最近一次用户活动时刻（空闲计时的基准）
unsigned long svBlinkAt   = 0, svNextBlink   = 0;
unsigned long svNextSaccade = 0, svNextMicro = 0;
unsigned long svWinkAt    = 0;
unsigned long svFrameAt   = 0;
unsigned long svFpsAt     = 0;       // 帧率统计起点
uint32_t      svFpsCnt    = 0;       // 实际画了多少帧
uint8_t       svFpsReport = 0;       // 已打印过几次帧率（只报前三次）
float         svYaw = 0, svPitch = 0;        // 当前头转（-1..1）
float         svYawT = 0, svPitchT = 0;      // 目标头转
float         svMicroX = 0, svMicroY = 0;    // 微扫视叠加

// 睡意状态：0 清醒 1 犯困 2 半睡 3 打盹
uint8_t       svState     = 0;
unsigned long svMoveAt    = 0;       // 最近一次 IMU 检测到"动"的时刻

// 重力跟随 / 惯性滞后
float         svG0X = 0, svG0Y = 0;  // 进入屏保时的重力基准（相对基准做差，安装角度无关）
float         svTiltX = 0, svTiltY = 0;  // 平滑后的倾角分量（g）
float         svImuW = 0;            // 重力跟随权重 0..1（死区内为 0，越大越由倾斜主导）
float         svSpin = 0;            // 陀螺惯性滞后叠加（-0.22..0.22）
unsigned long svImuAt = 0;
unsigned long svShockAt = 0, svShockCool = 0;

// 亮度渐变（persist 到退出后继续由主循环推进）
int           svBrightFrom = 200, svBrightTo = 200, svBrightCur = 200;
unsigned long svBrightAt = 0;

// 五个观察方向：正前 / 右上 / 左上 / 右下 / 左下
static const float SV_DIRS[5][2] = {
  { 0.00f,  0.00f }, { 0.85f, -0.25f }, { -0.75f, -0.20f },
  { 0.45f,  0.55f }, { -0.50f, 0.45f },
};

static inline float svRand01() { return (float)random(0, 10000) / 10000.0f; }
static inline float svRandRange(float lo, float hi) { return lo + (hi - lo) * svRand01(); }

// ---------------------------- 亮度渐变 ----------------------------
static void svBrightBegin(int to) {
  svBrightFrom = svBrightCur;
  svBrightTo   = to;
  svBrightAt   = millis();
}

// 主循环与屏保每帧都要调（淡入 320ms，退出后淡回用户亮度）
void svBrightRampTick() {
  if (svBrightCur == svBrightTo) return;
  if (screenOff) { svBrightCur = svBrightTo; return; }   // 息屏期间不写亮度，只记账
  unsigned long e = millis() - svBrightAt;
  int v;
  if (e >= SV_BRIGHT_MS) v = svBrightTo;
  else v = svBrightFrom + (int)((long)(svBrightTo - svBrightFrom) * (long)e / (long)SV_BRIGHT_MS);
  if (v != svBrightCur) { svBrightCur = v; panel->setBrightness(v); }
}

// 单只眼睛：竖胶囊。眨眼时底边固定、高度收窄，观感接近上眼皮落下。
// 外圈先铺 1px 中灰做羽化，降低纯黑纯白交界处的锯齿感。
static void svDrawEye(float cx, float cy, float w, float h, float hFull) {
  if (h < 7) h = 7;
  float baseY = cy + hFull * 0.5f;              // 底边不随眨眼移动
  int iw = (int)(w + 0.5f), ih = (int)(h + 0.5f);
  int ix = (int)(cx - w * 0.5f + 0.5f), iy = (int)(baseY - h + 0.5f);
  int r = iw / 2; if (r > ih / 2) r = ih / 2;
  gfx->fillRoundRect(ix - 1, iy - 1, iw + 2, ih + 2, r + 1, 0x8C71);
  gfx->fillRoundRect(ix, iy, iw, ih, r, C_WHITE);
}

// ---------------------------- IMU 采样与事件 ----------------------------
static void svImuTick(unsigned long now) {
  if (now - svImuAt < SV_IMU_MS) return;
  svImuAt = now;

  // 动作检测同时服务"睡意计时"，先衰减惯性滞后
  svSpin *= 0.90f;

  if (!imuOK) { svImuW = 0; return; }
  float g = 0, dps = 0;
  if (!imuSample(g, dps)) return;

  // ① 重力朝向跟随：相对进入时的重力基准做差，跟安装角度无关
  float dx = (imuAX - svG0X) * SV_IMU_SX;
  float dy = (imuAY - svG0Y) * SV_IMU_SY;
  svTiltX += (dx - svTiltX) * SV_TILT_LP;
  svTiltY += (dy - svTiltY) * SV_TILT_LP;

  float mag = sqrtf(svTiltX * svTiltX + svTiltY * svTiltY);
  float w = (mag - SV_TILT_DEAD) / (SV_TILT_FULL - SV_TILT_DEAD);
  if (w < 0) w = 0; if (w > 1) w = 1;
  svImuW += (w - svImuW) * 0.25f;

  // ③ 陀螺惯性滞后：取横滚轴/俯仰轴里较大的角速度给眼神拖尾
  if (dps > 25.0f) {
    float sz = (fabsf(imuRZ) > fabsf(imuRX)) ? imuRZ : imuRX;
    svSpin -= sz * SV_SPIN_K;
    if (svSpin > 0.22f) svSpin = 0.22f;
    else if (svSpin < -0.22f) svSpin = -0.22f;
  }

  // ④ 有人动过 ⇒ 重算睡意计时
  if (g > SV_MOVE_G || dps > 30.0f || mag > 0.14f) svMoveAt = now;

  // ② 敲击/猛晃 ⇒ 惊跳：瞬间睁大 + 双眨 + 眼神甩一下
  if (now - svShockCool > 1200 && (g > SV_SHOCK_G || dps > SV_SHOCK_DPS)) {
    svShockCool = now;
    svShockAt   = now;
    svMoveAt    = now;
    if (svState != 0) { svState = 0; Serial.println("[SAVER] state=0 (shock)"); }
    svBlinking  = true;
    svBlinkAt   = now;
    svNextBlink = now + 300;                    // 惊醒后连眨第二下
    svSpin += (svRand01() < 0.5f ? -0.07f : 0.07f);
    Serial.printf("[SAVER] shock g=%.2f dps=%.0f\n", g, dps);
  }
}

static void svDrawFrame(unsigned long now) {
  // 头转缓动：每帧靠近目标 22%，30fps 下约 120ms 到位，像"看过去"而不是滑过去
  svYaw   += (svYawT   - svYaw)   * 0.22f;
  svPitch += (svPitchT - svPitch) * 0.22f;

  // 睡意：呼吸频率 / 眨眼间隔 / 眼睛开合度（hK）随状态变化
  float breathW = 0.001131f;          // 0.18Hz
  float hK      = 1.00f;
  float nodAmp  = 0.00f;
  switch (svState) {
    case 1: breathW = 0.000817f; hK = 0.86f; break;   // 犯困 0.13Hz
    case 2: breathW = 0.000566f; hK = 0.60f; break;   // 半睡 0.09Hz
    case 3: breathW = 0.000440f; hK = 0.24f; nodAmp = 0.55f; break;  // 打盹 0.07Hz + 点头
  }
  float breath = sinf((float)now * breathW) * 0.016f;

  // 眨眼：95ms 闭合 / 30ms 保持 / 155ms 睁开 = 280ms
  float blinkK = 0.0f;
  if (svBlinking) {
    unsigned long e = now - svBlinkAt;
    const unsigned long CLOSE = 95, HOLD = 30, OPEN = 155;
    if (e < CLOSE) blinkK = (float)e / (float)CLOSE;
    else if (e < CLOSE + HOLD) blinkK = 1.0f;
    else if (e < CLOSE + HOLD + OPEN) blinkK = 1.0f - (float)(e - CLOSE - HOLD) / (float)OPEN;
    else {
      svBlinking = false; blinkK = 0.0f;
      // 越困眨眼越慢、间隔越长
      float lo = 3400, hi = 6200;
      if (svState == 1) { lo = 5200; hi = 8600; }
      else if (svState == 2) { lo = 6400; hi = 10000; }
      else if (svState == 3) { lo = 7600; hi = 12000; }
      svNextBlink = now + (unsigned long)svRandRange(lo, hi);
      if (svState == 0 && svRand01() < 0.12f) svNextBlink = now + 190;   // 清醒时偶尔连眨两下
    }
  }

  // ④a 单眼眨（只在清醒时，5% 概率，不打断正常眨眼）
  if (!svBlinking && !svWinking && svState == 0 && svRand01() < 0.0016f) {
    svWinking = true; svWinkAt = now;
  }
  float blinkR = blinkK, blinkL = blinkK;
  if (svWinking) {
    unsigned long e = now - svWinkAt;
    if (e < 300) {
      float k = (e < 110) ? (float)e / 110.0f
              : (e < 150) ? 1.0f
                          : 1.0f - (float)(e - 150) / 150.0f;
      if (k > blinkR) blinkR = k;
    } else svWinking = false;
  }

  // ② 惊跳：0~700ms 内眼睛放大，之后回落
  float shockK = 0.0f;
  if (svShockAt && now - svShockAt < 700)
    shockK = 1.0f - (float)(now - svShockAt) / 700.0f;

  // 防烧屏漂移：4px 半径、180 秒一圈，脏矩形已按这个范围留好边距
  float ph = (float)(now % 180000UL) * 0.0000349f;
  float driftX = cosf(ph) * 4.0f, driftY = sinf(ph) * 4.0f;

  // ① 重力跟随与扫视按权重混合：倾斜越大，眼神越由"重力"决定
  float yaw = svYaw, pit = svPitch;
  if (svImuW > 0.01f) {
    float iy = constrain(svTiltX / SV_TILT_FULL, -1.0f, 1.0f) * 0.85f;
    float ip = constrain(svTiltY / SV_TILT_FULL, -1.0f, 1.0f) * 0.70f;
    yaw = yaw * (1.0f - svImuW) + iy * svImuW;
    pit = pit * (1.0f - svImuW) + ip * svImuW;
  }
  yaw += svSpin;                                   // ③ 惯性拖尾
  pit += nodAmp * sinf((float)now * 0.00042f);     // 打盹点头（约 15 秒一次）

  float gap  = SV_EYE_GAP * (1.0f - 0.085f * fabsf(yaw));   // 转向时眼距压缩
  float wFull = SV_EYE_W * (1.0f + breath) * (1.0f + 0.08f * shockK);
  float hFull = SV_EYE_H * (1.0f + breath) * hK * (1.0f + 0.14f * shockK);
  float ccx  = CX + yaw * 10.0f + driftX;                   // 整脸随转向横移
  float ccy  = SV_EYE_CY + pit * 8.0f + driftY;
  float kR = 1.0f + 0.045f * yaw;                           // 近侧略大 / 远侧略小
  float kL = 1.0f - 0.045f * yaw;

  gfx->fillRect(SV_BOX_X, SV_BOX_Y, SV_BOX_W, SV_BOX_H, C_BLACK);
  svDrawEye(ccx - gap * 0.5f, ccy, wFull * kL, hFull * (1.0f - 0.93f * blinkL), hFull);
  svDrawEye(ccx + gap * 0.5f, ccy, wFull * kR, hFull * (1.0f - 0.93f * blinkR), hFull);
  flushRect(SV_BOX_X, SV_BOX_Y, SV_BOX_W, SV_BOX_H);
  svFpsCnt++;
}

void screensaverTick() {
  unsigned long now = millis();
  svBrightRampTick();                     // 亮度的 320ms 渐变在这里推进
  if (now - svFrameAt < SAVER_FRAME_MS) return;
  svFrameAt = now;

  // ④ 睡意演化：90s 犯困 / 3min 半睡 / 5min 打盹，任何 IMU 动作都会把计时拉回 0
  uint8_t ns = 0;
  unsigned long idle = now - svMoveAt;
  if (idle > 300000UL) ns = 3;
  else if (idle > 180000UL) ns = 2;
  else if (idle > 90000UL) ns = 1;
  if (ns != svState) { svState = ns; Serial.printf("[SAVER] state=%u\n", (unsigned)ns); }

  svImuTick(now);

  if (now >= svNextSaccade) {                  // 扫视：换一个观察方向
    int i = random(0, 5);
    svYawT = SV_DIRS[i][0];
    svPitchT = SV_DIRS[i][1];
    svNextSaccade = now + (unsigned long)svRandRange(2500, 5000);
  }
  if (now >= svNextMicro) {                    // 微扫视：避免完全静止像死机
    svMicroX = svRandRange(-0.055f, 0.055f);
    svMicroY = svRandRange(-0.045f, 0.045f);
    svNextMicro = now + (unsigned long)svRandRange(800, 1700);
  }
  if (!svBlinking && now >= svNextBlink) { svBlinking = true; svBlinkAt = now; }

  svDrawFrame(now);

  // 进入后前三次每 5 秒报一次实际帧率（验证局部推送是否真的生效，之后不再刷日志）
  if (svFpsReport < 3 && now - svFpsAt >= 5000) {
    Serial.printf("[SAVER] fps=%.1f state=%u tilt=(%.2f,%.2f) w=%.2f\n",
                  svFpsCnt * 1000.0f / (float)(now - svFpsAt), (unsigned)svState,
                  svTiltX, svTiltY, svImuW);
    svFpsCnt = 0; svFpsAt = now; svFpsReport++;
  }
}

void saverEnter() {
  if (saverActive) return;
  saverActive = true;
  unsigned long now = millis();
  svActAt = now;
  svYaw = svYawT = 0.0f; svPitch = svPitchT = 0.0f;
  svMicroX = svMicroY = 0.0f;
  svBlinking = false; svWinking = false;
  svNextBlink = now + 1600;
  svNextSaccade = now + 2300;
  svNextMicro = now + 900;
  svFrameAt = 0;
  svFpsAt = now; svFpsCnt = 0; svFpsReport = 0;

  // musicA.24：亮度跟随用户设定（45%，夹在 40~120），并做 320ms 渐变
  int dim = (int)((long)lastBrightness * SAVER_DIM_PCT / 100);
  if (dim < SAVER_DIM_MIN) dim = SAVER_DIM_MIN;
  if (dim > SAVER_DIM_MAX) dim = SAVER_DIM_MAX;
  svBrightCur = screenOff ? 0 : lastBrightness;    // 若期间刚息屏过，按当前亮度起算
  svBrightBegin(screenOff ? 0 : dim);

  // IMU 状态复位：重力基准取当前姿态，睡意计时与惊跳清零
  svState = 0; svMoveAt = now; svImuAt = 0;
  svG0X = imuAX; svG0Y = imuAY;
  svTiltX = svTiltY = 0; svImuW = 0; svSpin = 0;
  svShockAt = 0; svShockCool = 0;

  gfx->fillScreen(C_BLACK);         // 整屏黑一次，让画布和实际屏幕对齐
  flushSync();
  Serial.printf("[SAVER] enter dim=%d (base %d) imu=%s\n", dim, lastBrightness, imuOK ? "on" : "off");
}

void saverExit(bool eatTouch) {
  if (!saverActive) return;
  saverActive = false;
  svActAt = millis();               // 重新开始空闲计时，避免退出后立刻又进屏保
  if (svFpsCnt && svFpsAt)          // 退出时报一次实际帧率，方便确认局部推送有没有生效
    Serial.printf("[SAVER] exit, avg %.1f fps (%u frames / %lu ms)\n",
                  svFpsCnt * 1000.0f / (float)(millis() - svFpsAt), (unsigned)svFpsCnt,
                  (unsigned long)(millis() - svFpsAt));
  else Serial.println("[SAVER] exit");
  // 亮度淡回用户设定（由主循环的 svBrightRampTick 推进，不阻塞 BLE）
  if (!screenOff) svBrightBegin(lastBrightness);
  else svBrightCur = svBrightTo = lastBrightness;
  svEatNext = eatTouch;
  drawWaiting(bleConnected && bleAuthed && txSubscribed && !appBye ? WAIT_READY : WAIT_NOLINK);
}

// 触摸唤醒：只退出屏保，不产生任何手势
void saverWake() { saverExit(true); }
