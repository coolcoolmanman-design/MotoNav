#pragma once
// musicA.23 待机屏保：程序化眼睛动画（呼吸 / 随机眨眼 / 扫视 / 2.5D 轻微头转）
// musicA.24：亮度改为跟随用户设定（45%，40~120 夹紧）+ 320ms 渐变；加四层 IMU 互动
//            ①重力朝向跟随 ②敲击/猛晃惊跳 ③陀螺惯性滞后 ④睡意演化（犯困→半睡→打盹）
// FO-221：表情包扩充。参考 xAI《Designing Grok Bot》六态动效与 nasawz/GrokBot
//            （25 眼型 / 39 状态 / 阻尼弹簧形变 / 球面转头），把原来「4 档睡意压扁同一
//            对胶囊眼」扩成 16 种参数化眼型 + 9 种装饰 + 8 组动作，全部纯代码绘制。
//            刻意不加 App 特效开关：仍只用现有 #SV 总开关，选上屏保即自动获得整套表情。
//
// 为什么不用图片序列帧：466×466 一帧 RGB565 就是 434KB，几十帧要几十 MB flash，
// 而且解码加整屏推送只能跑到约 12fps。改成纯代码绘制后不占存储空间，
// 配合 DisplayPipeline 的局部推送（flushRect）可以稳定在 30fps 以上。
//
// 三个必须守住的约束：
//   1. 每帧只刷「眼睛活动区」一个脏矩形，其余区域保持纯黑不动（这是帧率的全部来源）；
//      所有新增装饰与尾迹也全部收在 74..392 / 132..300 内，不扩大脏矩形；
//   2. AMOLED 防烧屏：整体画面做极缓慢的圆周漂移 + 屏保亮度降到待机亮度的 45%；
//      不加彩色大色块 blob 身体（长期点亮易烧屏、也破坏亮度对比），装饰只用中灰小面积；
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

// 脏矩形：覆盖眼睛 + 呼吸缩放 + 惊跳放大 + 漂移 + 微扫视 + 全部装饰的活动范围
// 左右 74..392（宽 318），上下 132..300（高 168）→ 318×168×2 ≈ 107KB/帧
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

// ---------------------------- FO-221 表情 / 装饰枚举 ----------------------------
// 眼型：全部参数化，不存位图。设计稿里的 5~9° 逐眼旋转在固件里折成「两眼高低差」
// （歪头），视觉等价但零额外绘制开销，也不破坏「眨眼时底边固定」的语义。
enum { SVK_CAPSULE = 0, SVK_LINE = 1, SVK_ARC = 2, SVK_HEART = 3, SVK_SPIRAL = 4 };
enum { SVD_NONE = 0, SVD_ZZZ = 1, SVD_LOVE = 2, SVD_SWEAT = 3, SVD_BROW = 4,
       SVD_RAYS = 5, SVD_DOTS = 6, SVD_BANG = 7, SVD_ORBIT = 8, SVD_TRAIL = 9 };
enum { SVE_CALM = 0, SVE_CURIOUS, SVE_HAPPY, SVE_WINK, SVE_PROUD, SVE_EXCITED,
       SVE_SURPRISED, SVE_CONFUSED, SVE_SUSPICIOUS, SVE_LOVE, SVE_DIZZY,
       SVE_DROWSY, SVE_HALF, SVE_SLEEP, SVE_SHOCK, SVE_WAITING };

struct SvExpr {
  int16_t w, h;         // 单眼基准宽 / 高
  int8_t  rot;          // 歪头量（度，折算成两眼高低差）
  int8_t  dyL, dyR;     // 左右眼 y 偏移
  uint8_t kind, deco;   // 眼型 / 装饰
  uint8_t scale;        // 整体缩放百分比（100 = 1.00）
  uint8_t lid;          // 上眼睑压下比例（0..42%）
};
static const SvExpr SV_EXPRS[16] = {
  { 92,112,  0,  0,  0, SVK_CAPSULE, SVD_NONE,  100,  0},  //  0 平静   idle
  { 96,118, -7, -6,  8, SVK_CAPSULE, SVD_NONE,  104,  0},  //  1 好奇   扫视 / 新通知
  { 92, 62,  0,  0,  0, SVK_ARC,     SVD_NONE,  100,  0},  //  2 开心   连上手机 / 到站
  { 92,112,  0,  0,  0, SVK_CAPSULE, SVD_NONE,  100,  0},  //  3 眨眼   左眼横线（特判）
  { 88, 74,  9,  0,  0, SVK_CAPSULE, SVD_BROW,  100,  0},  //  4 得意   里程达成
  {108,128,  0,  0,  0, SVK_CAPSULE, SVD_RAYS,  110,  0},  //  5 兴奋   音乐 / 来电
  {116,124,  0,  0,  0, SVK_CAPSULE, SVD_SWEAT, 112,  0},  //  6 震惊   急刹 / 敲击
  { 92,108, -5,-14, 12, SVK_CAPSULE, SVD_BROW,  100,  0},  //  7 疑惑   导航重算
  { 96, 44, -6,  0,  0, SVK_CAPSULE, SVD_BROW,  100, 30},  //  8 怀疑   超速 / 低电量
  { 88, 88,  0,  0,  0, SVK_HEART,   SVD_LOVE,  100,  0},  //  9 爱心眼 充电中
  { 92, 92,  0,  0,  0, SVK_SPIRAL,  SVD_NONE,  100,  0},  // 10 晕眩   连续甩动
  { 92, 74,  0,  8,  8, SVK_CAPSULE, SVD_NONE,   92, 22},  // 11 困倦   静置 90 秒
  { 92, 26,  0, 12, 12, SVK_CAPSULE, SVD_NONE,   90, 42},  // 12 半睡   静置 3 分钟
  { 92, 12,  0, 16, 16, SVK_LINE,    SVD_ZZZ,    88,  0},  // 13 睡熟   静置 5 分钟
  {118,134,  0, -4, -4, SVK_CAPSULE, SVD_RAYS,  114,  0},  // 14 惊跳   IMU 猛晃
  { 96, 18,  7,  6,  6, SVK_LINE,    SVD_NONE,  100,  0},  // 15 等待   无手机连接
};
// 清醒时的表情池：9~16s 自动漂移一张脸，避免长时间定格
static const uint8_t SV_POOL[8] = { 0, 1, 2, 4, 5, 7, 9, 15 };

// 装饰色：中灰 / 淡粉 / 淡青，面积都很小，45% 亮度下仍是点缀而非大色块
#define SV_EDGE_C   0x8C71         // 1px 羽化外圈（与原版一致）
#define SV_DIM_C    0x6BF2
#define SV_ACCENT_C 0xFBD2
#define SV_CYAN_C   0x6EB9

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

// FO-221：表情池 / 弹簧形变 / 8 组动效调度
uint8_t       svExpr = 0, svExprFrom = 0, svExprTo = 0;
float         svMorph = 1.0f, svMorphV = 0.0f;
unsigned long svNextExpr = 0;
uint8_t       svMotion = 0;          // 0 无 / 1 球面转头 / 2 三点脉动 / 3 轨道环绕 / 4 惊跳拖尾
unsigned long svMotionAt = 0, svNextMotion = 0;
unsigned long svDizzyAt = 0;         // 连续甩动 → 晕眩表情的截止依据

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

// ---------------------------- 基础绘制 ----------------------------
// 任意方向的粗线（两端圆头），用于眉毛 / 闪线 / Z 字斜笔；自己实现，不依赖 include 顺序
static void svThick(float x0, float y0, float x1, float y1, float w, uint16_t c) {
  float dx = x1 - x0, dy = y1 - y0;
  float len = sqrtf(dx * dx + dy * dy);
  int r = (int)(w * 0.5f + 0.5f);
  if (len < 0.5f || r < 1) { gfx->fillCircle((int)x0, (int)y0, r > 0 ? r : 1, c); return; }
  float nx = -dy / len * w * 0.5f, ny = dx / len * w * 0.5f;
  gfx->fillTriangle((int)(x0 + nx), (int)(y0 + ny), (int)(x0 - nx), (int)(y0 - ny),
                    (int)(x1 + nx), (int)(y1 + ny), c);
  gfx->fillTriangle((int)(x0 - nx), (int)(y0 - ny), (int)(x1 + nx), (int)(y1 + ny),
                    (int)(x1 - nx), (int)(y1 - ny), c);
  gfx->fillCircle((int)x0, (int)y0, r, c);
  gfx->fillCircle((int)x1, (int)y1, r, c);
}

static void svHeart(int x, int y, int r, uint16_t c) {
  int rr = r / 2;
  gfx->fillCircle(x - rr, y - rr / 2, rr, c);
  gfx->fillCircle(x + rr, y - rr / 2, rr, c);
  gfx->fillTriangle(x - r, y - r / 8, x + r, y - r / 8, x, y + r, c);
}

// 单只眼睛。hFull 是未眨眼的完整高度：眨眼时底边固定、只收窄上沿，观感接近上眼皮落下。
// 外圈先铺 1px 中灰做羽化，降低纯黑纯白交界处的锯齿感。
static void svDrawEye(float cx, float cy, float w, float h, float hFull,
                      uint8_t kind, uint8_t lid, uint16_t col) {
  if (h < 7) h = 7;
  bool bottomFixed = (kind == SVK_CAPSULE || kind == SVK_LINE);
  float baseY = bottomFixed ? (cy + hFull * 0.5f) : (cy + h * 0.5f);
  int iw = (int)(w + 0.5f), ih = (int)(h + 0.5f);
  int ix = (int)(cx - w * 0.5f + 0.5f), iy = (int)(baseY - h + 0.5f);
  // 极端组合（惊跳放大 + 满偏头转 + 漂移）可能越出脏矩形：把整只眼推进框内，
  // 保证局部推送不会把眼尾切掉，也不需要扩大脏矩形。
  if (ix < SV_BOX_X + 1) ix = SV_BOX_X + 1;
  if (ix + iw > SV_BOX_X + SV_BOX_W - 1) ix = SV_BOX_X + SV_BOX_W - 1 - iw;
  if (iy < SV_BOX_Y + 1) iy = SV_BOX_Y + 1;
  if (iy + ih > SV_BOX_Y + SV_BOX_H - 1) iy = SV_BOX_Y + SV_BOX_H - 1 - ih;
  float dcx = ix + iw * 0.5f, dcy = iy + ih * 0.5f;
  int r = iw / 2; if (r > ih / 2) r = ih / 2;

  if (kind == SVK_HEART) {
    int rr = (iw < ih ? iw : ih) / 2;
    gfx->fillCircle(ix + rr + 1, iy + rr, rr + 1, SV_EDGE_C);
    gfx->fillCircle(ix + iw - rr - 1, iy + rr, rr + 1, SV_EDGE_C);
    gfx->fillCircle(ix + rr, iy + rr, rr, col);
    gfx->fillCircle(ix + iw - rr, iy + rr, rr, col);
    gfx->fillTriangle(ix, iy + rr, ix + iw, iy + rr, ix + iw / 2, iy + ih, col);
    return;
  }
  if (kind == SVK_SPIRAL) {
    for (int s = 0; s < 3; s++) {
      float rr = w * (0.46f - s * 0.15f);
      for (float a = s * 0.7f; a < 5.4f + s * 0.7f; a += 0.13f)
        gfx->fillCircle((int)(dcx + cosf(a) * rr), (int)(dcy + sinf(a) * rr * 0.95f),
                        (int)(h * 0.055f) + 1, col);
    }
    return;
  }

  gfx->fillRoundRect(ix - 1, iy - 1, iw + 2, ih + 2, r + 1, SV_EDGE_C);
  gfx->fillRoundRect(ix, iy, iw, ih, r, col);
  if (kind == SVK_ARC) {                 // 笑眼：从上方压掉一块，留下上弯月牙
    int cut = (int)(h * 0.62f);
    gfx->fillRoundRect(ix - 2, iy - cut, iw + 4, ih, r + 1, C_BLACK);
  }
  if (lid)                              // 上眼睑：困倦 / 半睡 / 怀疑
    gfx->fillRect(ix - 2, iy - 1, iw + 4, (int)(h * lid / 100.0f) + 1, C_BLACK);
}

// 9 种装饰 / 动效笔触，全部落在脏矩形 74..392 / 132..300 内。
// t 是 0..1 的相位；脸中心固定 233,216（不叠漂移，漂移只作用于眼睛本身）。
static void svDeco(uint8_t id, float t) {
  const int cx = 233, cy = 216;
  switch (id) {
    case SVD_ZZZ: {                    // 睡熟：三个递减的 Z
      static const int Z[3][3] = {{329,158,9},{349,146,7},{365,138,5}};
      for (int i = 0; i < 3; i++) {
        int x = Z[i][0], y = Z[i][1], r = Z[i][2];
        gfx->fillRect(x - r, y - r, 2 * r, 2, SV_DIM_C);
        gfx->fillRect(x - r, y + r - 2, 2 * r, 2, SV_DIM_C);
        for (int k = -r; k <= r; k += 2)
          gfx->fillRect(x + k - 1, y - k - 1, 3, 3, SV_DIM_C);
      }
    } break;
    case SVD_LOVE:                     // 爱心眼：三颗小心
      svHeart(309, 150, 8, SV_ACCENT_C);
      svHeart(343, 142, 6, SV_ACCENT_C);
      svHeart(365, 155, 5, SV_ACCENT_C);
      break;
    case SVD_SWEAT:                    // 震惊：一滴汗
      gfx->fillCircle(345, 158, 7, SV_CYAN_C);
      gfx->fillTriangle(345, 140, 338, 155, 352, 155, SV_CYAN_C);
      break;
    case SVD_BROW:                     // 得意 / 疑惑 / 怀疑：两条斜眉
      svThick(117, 146, 177, 154, 6, SV_DIM_C);
      svThick(289, 154, 349, 146, 6, SV_DIM_C);
      break;
    case SVD_RAYS:                     // 兴奋 / 惊跳：两眼外侧的短闪线，不穿过眼球
      for (int s = -1; s <= 1; s += 2)
        for (int i = 0; i < 3; i++) {
          int ln = (i == 1) ? 22 : 16, y = cy + (i == 0 ? -62 : (i == 1 ? -30 : 2));
          int x = cx + s * 150;
          gfx->fillRect(x - 2, y - ln, 4, ln, SV_DIM_C);
        }
      break;
    case SVD_DOTS:                     // 三点脉动（思考 / 等待），眼睛退场
      for (int i = 0; i < 3; i++) {
        float s = sinf(t * 3.14159f - i * 0.9f);
        int r = (int)(9 + 10 * (s > 0 ? s : 0));
        gfx->fillCircle(cx - 54 + i * 54, cy, r, C_WHITE);
      }
      break;
    case SVD_BANG:                     // 惊跳：感叹号
      gfx->fillRoundRect(cx - 13, 146, 26, 92, 13, C_WHITE);
      gfx->fillCircle(cx, 270, 13, C_WHITE);
      break;
    case SVD_ORBIT: {                  // 轨道环绕：扁椭圆 + 一颗亮点
      for (float a = 0; a < 6.283f; a += 0.09f)
        gfx->fillCircle((int)(cx + cosf(a) * 116), (int)(cy + 71 + sinf(a) * 12),
                        2, SV_DIM_C);
      float a2 = t * 6.283f;
      gfx->fillCircle((int)(cx + cosf(a2) * 116), (int)(cy + 71 + sinf(a2) * 12),
                      9, SV_ACCENT_C);
    } break;
    case SVD_TRAIL: {                  // 球面转头 / 惊跳的头顶尾迹
      static const uint16_t TC[3] = { SV_CYAN_C, SV_ACCENT_C, SV_DIM_C };
      for (int i = 0; i < 3; i++) {
        float rr = 96.0f + i * 15.0f;
        for (float a = 3.42f; a < 6.0f; a += 0.07f)
          gfx->fillCircle((int)(cx + cosf(a) * rr),
                          (int)(cy - 62 - i * 3 + sinf(a) * 18), 2, TC[i]);
      }
    } break;
    default: break;
  }
}

// ---------------------------- 表情切换（阻尼弹簧） ----------------------------
static void svSetExpr(uint8_t e) {
  if (e == svExprTo) return;
  svExprFrom = svExpr;
  svExprTo = e;
  svMorph = 0.0f;                      // ω≈7 / ζ≈0.6：先挤压再带轻微过冲弹出
  svMorphV = 0.0f;
  Serial.printf("[SAVER] expr=%u -> %u\n", (unsigned)svExprFrom, (unsigned)e);
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
  if (fabsf(svSpin) > 0.16f) svDizzyAt = now;   // 连续甩动 → 晕眩表情

  // ④ 有人动过 ⇒ 重算睡意计时
  if (g > SV_MOVE_G || dps > 30.0f || mag > 0.14f) svMoveAt = now;

  // ② 敲击/猛晃 ⇒ 惊跳：瞬间睁大 + 双眨 + 眼神甩一下 + 惊跳拖尾动效
  if (now - svShockCool > 1200 && (g > SV_SHOCK_G || dps > SV_SHOCK_DPS)) {
    svShockCool = now;
    svShockAt   = now;
    svMoveAt    = now;
    if (svState != 0) { svState = 0; Serial.println("[SAVER] state=0 (shock)"); }
    svBlinking  = true;
    svBlinkAt   = now;
    svNextBlink = now + 300;                    // 惊醒后连眨第二下
    svSpin += (svRand01() < 0.5f ? -0.07f : 0.07f);
    svMotion = 4; svMotionAt = now;            // ⑥ 惊跳 + 拖尾
    Serial.printf("[SAVER] shock g=%.2f dps=%.0f\n", g, dps);
  }
}

static void svDrawFrame(unsigned long now) {
  // 头转缓动：每帧靠近目标 22%，30fps 下约 120ms 到位，像"看过去"而不是滑过去
  svYaw   += (svYawT   - svYaw)   * 0.22f;
  svPitch += (svPitchT - svPitch) * 0.22f;

  // ① 弹簧形变：切换表情时 0→1，带轻微过冲；跨过 0.5 才换脸，形成"挤压 → 弹出"
  const float dt = 0.033f;
  float acc = 49.0f * (1.0f - svMorph) - 8.5f * svMorphV;
  svMorphV += acc * dt;
  svMorph  += svMorphV * dt;
  if (svMorph > 1.30f) { svMorph = 1.30f; svMorphV = 0.0f; }
  if (svMorph < 0.0f)  { svMorph = 0.0f;  svMorphV = 0.0f; }
  if (svMorph >= 0.5f) svExpr = svExprTo;
  float morphScale = 1.0f + 0.30f * (svMorph - 1.0f);

  // 睡意：呼吸频率与点头幅度（眼型本身由表情表决定，不再二次压扁）
  float breathW = 0.001131f;          // 0.18Hz
  float nodAmp  = 0.00f;
  switch (svState) {
    case 1: breathW = 0.000817f; break;                    // 犯困 0.13Hz
    case 2: breathW = 0.000566f; break;                    // 半睡 0.09Hz
    case 3: breathW = 0.000440f; nodAmp = 0.85f; break;    // 打盹 0.07Hz + 点头
  }
  float breath = sinf((float)now * breathW) * 0.016f;

  // ② 眨眼：95ms 闭合 / 30ms 保持 / 155ms 睁开 = 280ms（42% 闭 / 58% 开）
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

  // ②a 单眼眨（只在清醒时，5% 概率，不打断正常眨眼）
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

  // ⑥ 惊跳：0~700ms 内眼睛放大，之后回落
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
  pit += nodAmp * sinf((float)now * 0.00042f);     // ⑦ 打盹点头（约 15 秒一次）

  // ③ 球面转头：cos 压缩眼距/眼宽，转到侧面时几乎隐藏，并叠头顶尾迹
  float turn = 0.0f;
  if (svMotion == 1) {
    float e = (float)(now - svMotionAt) / 1500.0f;
    if (e > 1.0f) e = 1.0f;
    turn = sinf(e * 3.14159f) * 1.35f;
  }
  float cosv = cosf(turn); if (cosv < 0.0f) cosv = 0.0f;
  float gapK = 0.35f + 0.65f * cosv;
  float wK   = 0.30f + 0.70f * cosv;

  const SvExpr &E = SV_EXPRS[svExpr];
  float sK = (float)E.scale / 100.0f;
  float gap   = SV_EYE_GAP * gapK * (1.0f - 0.085f * fabsf(yaw));
  float wFull = E.w * sK * (1.0f + breath) * morphScale * wK * (1.0f + 0.05f * shockK);
  float hFull = E.h * sK * (1.0f + breath) * morphScale * (1.0f + 0.08f * shockK);
  float ccx   = CX + yaw * 10.0f + sinf(turn) * 34.0f + driftX;
  float ccy   = SV_EYE_CY + pit * 8.0f + driftY;
  float kR = 1.0f + 0.045f * yaw;
  float kL = 1.0f - 0.045f * yaw;
  // 歪头：设计稿里的 5~9° 逐眼旋转折成两眼高低差
  float dyA = E.dyL + E.rot * 0.5f;
  float dyB = E.dyR - E.rot * 0.5f;

  float hL = hFull, hR = hFull;
  if (E.kind == SVK_CAPSULE || E.kind == SVK_LINE) {
    hL = hFull * (1.0f - 0.93f * blinkL);
    hR = hFull * (1.0f - 0.93f * blinkR);
  }

  gfx->fillRect(SV_BOX_X, SV_BOX_Y, SV_BOX_W, SV_BOX_H, C_BLACK);

  if (svMotion == 2) {
    // ④ 三点脉动：眼睛暂时退场，只留三点（思考 / 等待）
    svDeco(SVD_DOTS, (float)((now - svMotionAt) % 1400) / 1400.0f);
  } else {
    // 表情 3「眨眼」：左眼是横线、右眼仍是胶囊
    if (svExpr == SVE_WINK) {
      svDrawEye(ccx - gap * 0.5f, ccy + dyA, wFull * kL, 12.0f, 12.0f,
                SVK_LINE, 0, C_WHITE);
      svDrawEye(ccx + gap * 0.5f, ccy + dyB, wFull * kR, hR, hFull,
                SVK_CAPSULE, 0, C_WHITE);
    } else {
      svDrawEye(ccx - gap * 0.5f, ccy + dyA, wFull * kL, hL, hFull,
                E.kind, E.lid, C_WHITE);
      svDrawEye(ccx + gap * 0.5f, ccy + dyB, wFull * kR, hR, hFull,
                E.kind, E.lid, C_WHITE);
    }
    if (E.deco) svDeco(E.deco, (float)(now % 1400) / 1400.0f);
  }

  // ③/⑤/⑥ 动效笔触（画在眼睛之后，但都避开了眼球区域）
  if (svMotion == 1) svDeco(SVD_TRAIL, (float)((now - svMotionAt) % 1500) / 1500.0f);
  if (svMotion == 3) svDeco(SVD_ORBIT, (float)((now - svMotionAt) % 4000) / 4000.0f);
  if (svMotion == 4) {
    float e = (float)(now - svMotionAt) / 700.0f;
    if (e > 1.0f) e = 1.0f;
    if (e > 0.35f) svDeco(SVD_BANG, e);
    svDeco(SVD_TRAIL, e);
  }

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

  // ⑧ 表情决策：只用屏保里本来就能可靠读到的全局量，不新增协议、不加 App 开关。
  //    优先级：IMU 惊跳 > 连续甩动晕眩 > 睡意三档 > 无连接时加权「等待」> 表情池漂移
  uint8_t want;
  if (svShockAt && now - svShockAt < 700) want = SVE_SHOCK;
  else if (now - svDizzyAt < 1500) want = SVE_DIZZY;
  else if (svState == 1) want = SVE_DROWSY;
  else if (svState == 2) want = SVE_HALF;
  else if (svState == 3) want = SVE_SLEEP;
  else if (now >= svNextExpr) {
    if (!(bleConnected && bleAuthed && txSubscribed) && svRand01() < 0.45f)
      want = SVE_WAITING;
    else
      want = SV_POOL[random(0, 8)];
    svNextExpr = now + (unsigned long)svRandRange(9000, 16000);
  } else want = svExprTo;
  svSetExpr(want);

  // 动效调度：每 18~30s 随机抽一段（转头 / 三点脉动 / 轨道 / 惊跳拖尾），
  // 加上常驻的弹簧形变、眨眼、打盹点头、表情池漂移，共 8 组动作。
  if (svMotion == 0 && svState == 0 && now >= svNextMotion) {
    svMotion = 1 + (uint8_t)random(0, 4);
    svMotionAt = now;
    Serial.printf("[SAVER] motion=%u\n", (unsigned)svMotion);
  }
  if (svMotion) {
    unsigned long dur = (svMotion == 3) ? 4000UL : (svMotion == 4) ? 900UL : 1500UL;
    if (now - svMotionAt > dur) {
      svMotion = 0;
      svNextMotion = now + (unsigned long)svRandRange(18000, 30000);
    }
  }

  svDrawFrame(now);

  // 进入后前三次每 5 秒报一次实际帧率（验证局部推送是否真的生效，之后不再刷日志）
  if (svFpsReport < 3 && now - svFpsAt >= 5000) {
    Serial.printf("[SAVER] fps=%.1f state=%u expr=%u motion=%u tilt=(%.2f,%.2f) w=%.2f\n",
                  svFpsCnt * 1000.0f / (float)(now - svFpsAt), (unsigned)svState,
                  (unsigned)svExpr, (unsigned)svMotion,
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

  // FO-221 复位：从「平静」起步，7s 后开始表情池漂移，12s 后抽第一段动效
  svExpr = svExprFrom = svExprTo = SVE_CALM;
  svMorph = 1.0f; svMorphV = 0.0f;
  svNextExpr = now + 7000;
  svMotion = 0; svMotionAt = 0; svNextMotion = now + 12000;
  svDizzyAt = 0;

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
  Serial.printf("[SAVER] enter dim=%d (base %d) imu=%s expr=16 motion=8\n",
                dim, lastBrightness, imuOK ? "on" : "off");
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
