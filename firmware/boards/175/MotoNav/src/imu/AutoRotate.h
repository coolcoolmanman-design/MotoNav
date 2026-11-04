#pragma once
// v7.12.10-ui.2-zelda40.9-musicA.14 自适应旋转（屏幕始终“正立”）
//
// 仅用六轴 IMU 的加速度（重力）分量，不依赖陀螺、无累积漂移：
//   - 设备竖持（像看手表/导航屏）时，重力主要落在屏幕平面 (X,Y) 内，
//     atan2 直接给出绝对偏航角 → 圆屏内容反向旋转，画面始终正立。
//   - 设备平躺（屏朝上）时重力≈沿 Z，平面内分量≈0，无法判定朝向 → 冻结当前角度。
// 圆屏是旋转对称外形，内容整体绕圆心转任意角度都不裁切、不留黑边，是做这件事最理想的外形。
//
// 与手动圆屏方向 (#R) 的关系：自适应开启时忽略手动 #R（自动模式优先），
// 手动 #R 会自动关掉自适应，让手动角度固定下来。

bool autoRotateOn = false;
float autoRotSmooth = 0;     // 当前平滑角度（度，0~359）
bool autoRotInit = false;    // 平滑器是否已初始化
// 朝向符号：不同面板/IMU 朝向装法下可能需翻成 -1。物理实测若画面上下颠倒，改这里即可。
static const float AROT_SIGN = 1.0f;

void autoRotateLoad() {
  Preferences pr;
  pr.begin("motonav", false);
  autoRotateOn = pr.getInt("arot", 0) != 0;
  pr.end();
  autoRotInit = false;
}

void autoRotateSet(bool on) {
  autoRotateOn = on;
  Preferences pr;
  pr.begin("motonav", false);
  pr.putInt("arot", on ? 1 : 0);
  pr.end();
  if (!on) { setScreenDeg(screenDeg, true); }   // 关掉时把当前角度固化进 NVS
  autoRotInit = false;
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
  // 屏幕平面内重力分量（X/Y），Z 为指向用户的方向
  float gx = imuGX, gy = imuGY, gz = imuGZ;
  float gh = sqrtf(gx * gx + gy * gy);
  // 平躺（重力大多沿 Z）→ 平面内分量太小，无法判定朝向，冻结当前角度
  if (gh < 0.30f) return;
  // 目标角：让画面顶部指向“与重力相反”的方向（即屏幕正立）。
  float target = AROT_SIGN * atan2f(-gy, -gx) * 180.0f / (float)M_PI;
  if (target < 0) target += 360.0f;
  if (target >= 360.0f) target -= 360.0f;
  if (!autoRotInit) { autoRotSmooth = target; autoRotInit = true; }
  // 最短弧差做低通，避免越过 0/360 边界时跳变
  float diff = target - autoRotSmooth;
  if (diff > 180) diff -= 360; else if (diff < -180) diff += 360;
  autoRotSmooth += 0.15f * diff;
  if (autoRotSmooth < 0) autoRotSmooth += 360;
  else if (autoRotSmooth >= 360) autoRotSmooth -= 360;
  int deg = (int)lroundf(autoRotSmooth);
  if (deg != screenDeg) {
    setScreenDeg(deg, false);
    needFlush = true;       // 触发整帧重推（fastPush 按新 rotFree 取样）
  }
}
