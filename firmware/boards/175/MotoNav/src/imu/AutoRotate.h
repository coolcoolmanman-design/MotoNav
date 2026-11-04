#pragma once
// v7.12.10-ui.2-zelda40.9-musicA.15 自适应旋转（屏幕始终"正立"）v2
//
// musicA.14 教训：atan2(-gy,-gx) 是绝对角度公式，隐含"IMU 轴与屏幕边对齐 + 正立
// 对应固定重力方向"两个假设。实际 QMI8658 贴装朝向任意（恒定偏角），实测画面跟着
// 转但永远差一个固定偏角，永远到不了正立。
//
// musicA.15 改为【相对相位锚定】：
//   φ = atan2(gy, gx)  为屏幕平面内重力矢量的相位。
//   设备绕屏幕法线转过 α（视觉顺时针）时，重力在机体系里的相位变化 Δφ = −α
//   （刚体装贴只可能是 proper rotation，贴装偏角是常数，在 Δφ 里自动抵消）。
//   所以：target = anchor + Δφ，其中 anchor = 开启/锚定那一刻的 screenDeg。
//   - 锚定瞬间画面零跳变（target == 当前角度）；
//   - 不依赖任何轴对齐假设，无需 AROT_SIGN / 贴装修正；
//   - 开关打开时用户正看着屏 → 锚点天然就是"正立"。
//   平躺（重力沿 Z，平面内分量 < 0.30g）时无法判定 → 冻结当前角度。
//
// 与手动圆屏方向 (#R) 的关系：自适应开启时忽略手动 #R（自动模式优先），
// 手动 #R 会自动关掉自适应，让手动角度固定下来。

bool autoRotateOn = false;
float autoRotSmooth = 0;     // 当前平滑角度（度，0~359）
float autoRotRef = 0;        // 锚定相位参考：target = φ − autoRotRef
bool autoRotInit = false;    // 是否已锚定（开机后等首个有效重力样本；#AR,1 立即重锚）
// 方向保险：理论推导 target = anchor + (φ − φ₀)。若实机出现"画面跟着设备同向转"
// （发散），把 AROT_DIR 改成 -1 即可，无需再查贴装。
static const float AROT_DIR = 1.0f;

// 屏幕平面内重力相位（度）
static float arotPhase() {
  return atan2f(imuGY, imuGX) * 180.0f / (float)M_PI;
}

// 以"当前画面角度"为锚锚定：target(φ) = screenDeg + AROT_DIR·(φ − φ_now)
void autoRotateAnchor() {
  float phi = arotPhase();
  autoRotRef = phi - AROT_DIR * (float)screenDeg;
  autoRotSmooth = (float)screenDeg;
  autoRotInit = true;
  Serial.printf("[AROT] anchor deg=%d phi=%.1f gh=%.2f\n", screenDeg, phi,
                sqrtf(imuGX * imuGX + imuGY * imuGY));
}

void autoRotateLoad() {
  Preferences pr;
  pr.begin("motonav", false);
  autoRotateOn = pr.getInt("arot", 0) != 0;
  pr.end();
  autoRotInit = false;                    // 开机后由 autoRotatePoll 用首个有效样本锚定
}

void autoRotateSet(bool on) {
  autoRotateOn = on;
  Preferences pr;
  pr.begin("motonav", false);
  pr.putInt("arot", on ? 1 : 0);
  pr.end();
  if (on) {
    // #AR,1 到达时用户正看着屏幕：立即以当前画面角度为"正立"锚点。
    // 重力估计尚未就绪/平躺则留给 autoRotatePoll 首个有效样本再锚。
    if (imuOK && imuGInit && sqrtf(imuGX * imuGX + imuGY * imuGY) >= 0.30f)
      autoRotateAnchor();
  } else {
    setScreenDeg(screenDeg, true);        // 关掉时把当前角度固化进 NVS
  }
}

// 每帧刷新重力估计（不输出动态 G）。与 G 页 imuPoll 共用 imuFinish 的重力低通。
// 放在主循环里，保证非 G 页时重力矢量也持续更新。
void imuPollGravity() {
  if (!imuOK) return;
  float g, dps;
  imuPoll(g, dps);
}

// 每帧调用：用最新重力估计更新 screenDeg（内存级，不每帧写 NVS）。
void autoRotatePoll() {
  if (!autoRotateOn || !imuOK || !imuGInit) return;
  // 屏幕平面内重力分量（X/Y），Z 为垂直屏幕方向
  float gx = imuGX, gy = imuGY, gz = imuGZ;
  float gh = sqrtf(gx * gx + gy * gy);
  // 平躺（重力大多沿 Z）→ 平面内分量太小，无法判定朝向，冻结当前角度
  if (gh < 0.30f) return;
  if (!autoRotInit) autoRotateAnchor();   // 首个有效样本：锚定当前画面角度（无跳变）
  float target = AROT_DIR * (arotPhase() - autoRotRef);
  if (target < 0) target += 360.0f;
  if (target >= 360.0f) target -= 360.0f;
  // 最短弧差做低通，避免越过 0/360 边界时跳变
  float diff = target - autoRotSmooth;
  if (diff > 180) diff -= 360; else if (diff < -180) diff += 360;
  autoRotSmooth += 0.15f * diff;
  if (autoRotSmooth < 0) autoRotSmooth += 360;
  else if (autoRotSmooth >= 360) autoRotSmooth -= 360;
  int deg = (int)lroundf(autoRotSmooth);
  deg = ((deg % 360) + 360) % 360;
  if (deg != screenDeg) {
    setScreenDeg(deg, false);
    needFlush = true;       // 触发整帧重推（fastPush 按新 rotFree 取样）
  }
}
