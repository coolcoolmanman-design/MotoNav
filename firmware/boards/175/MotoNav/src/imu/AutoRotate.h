#pragma once
// musicA.110 / FO-260：自适应旋转（重力朝正）——相对相位锚定 + 四分向吸附。
//
// 复用 musicA.15 相对相位思路（贴装偏角在 Δφ 中抵消；开启瞬间零跳变），
// 但不再连续写任意 screenDeg（rotFree 双线性贵）：只吸附 0/90/180/270，
// 走现有 setScreenDeg / #R 四分路径。无磁力计；近水平（平面重力 < 0.30g）冻结。
//
// 协议：#AR,0 / #AR,1（NVS key "arot"）。手动 #R 会关掉自适应。

bool autoRotateOn = false;
float autoRotRef = 0;            // 锚定：target = AROT_DIR * (φ − autoRotRef)
bool autoRotInit = false;
int autoRotSnapQ = 0;            // 当前吸附档 0..3
int autoRotPendingQ = -1;
uint32_t autoRotPendingAt = 0;

// 方向：本机 IMU 轴向使 +1 时画面随设备同向转（发散），故取 -1。
static const float AROT_DIR = -1.0f;
static const float AROT_FLAT_G = 0.30f;
static const uint32_t AROT_DWELL_MS = 280;   // 跨档需稳定 dwell 再吸附，防抖

static float arotPhase() {
  return atan2f(imuGY, imuGX) * 180.0f / (float)M_PI;
}

static float arotNorm360(float a) {
  while (a < 0) a += 360.0f;
  while (a >= 360.0f) a -= 360.0f;
  return a;
}

void autoRotateAnchor() {
  float phi = arotPhase();
  autoRotRef = phi - AROT_DIR * (float)screenDeg;
  autoRotInit = true;
  autoRotSnapQ = ((screenDeg + 45) / 90) & 3;   // 与 poll / 关闭时一致：四舍五入
  autoRotPendingQ = -1;
  Serial.printf("[AROT] anchor deg=%d q=%d phi=%.1f gh=%.2f\n", screenDeg, autoRotSnapQ, phi,
                sqrtf(imuGX * imuGX + imuGY * imuGY));
}

void autoRotateLoad() {
  Preferences pr;
  pr.begin("motonav", false);
  autoRotateOn = pr.getInt("arot", 0) != 0;
  pr.end();
  autoRotInit = false;
  autoRotPendingQ = -1;
}

void autoRotateSet(bool on) {
  autoRotateOn = on;
  Preferences pr;
  pr.begin("motonav", false);
  pr.putInt("arot", on ? 1 : 0);
  pr.end();
  if (on) {
    if (imuOK && imuGInit && sqrtf(imuGX * imuGX + imuGY * imuGY) >= AROT_FLAT_G)
      autoRotateAnchor();
    else
      autoRotInit = false;
  } else {
    // 关：固化当前四分向到 NVS（经 setScreenDeg 延迟写）
    int q = ((screenDeg + 45) / 90) & 3;
    setScreenDeg(q * 90, true);
    autoRotPendingQ = -1;
  }
}

// 主循环：非 G 页也刷新重力低通（与 imuPoll / imuFinish 共用）。
void imuPollGravity() {
  if (!imuOK) return;
  float g, dps;
  imuPoll(g, dps);
}

void autoRotatePoll() {
  if (!autoRotateOn || !imuOK || !imuGInit) return;
  float gx = imuGX, gy = imuGY;
  float gh = sqrtf(gx * gx + gy * gy);
  if (gh < AROT_FLAT_G) return;                         // 近平放：无可靠平面偏航
  if (!autoRotInit) autoRotateAnchor();

  float target = arotNorm360(AROT_DIR * (arotPhase() - autoRotRef));
  int q = ((int)floorf((target + 45.0f) / 90.0f)) & 3;  // 最近四分向
  uint32_t now = millis();
  if (q == autoRotSnapQ) {
    autoRotPendingQ = -1;
    return;
  }
  if (autoRotPendingQ != q) {
    autoRotPendingQ = q;
    autoRotPendingAt = now;
    return;
  }
  if ((uint32_t)(now - autoRotPendingAt) < AROT_DWELL_MS) return;

  autoRotSnapQ = q;
  autoRotPendingQ = -1;
  int deg = q * 90;
  if (deg != screenDeg) {
    setScreenDeg(deg, false);                           // 内存级；关自适应时再固化 NVS
    needFlush = true;
    Serial.printf("[AROT] snap q=%d deg=%d target=%.1f\n", q, deg, target);
  }
}
