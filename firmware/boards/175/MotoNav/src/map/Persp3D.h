#pragma once
// musicA.163：高德原生 3D 底图（P3D）接收 + 轻量重投影。
//
// 背景：现在固件用 rasterRows3D() 把「正俯视瓦片」逐像素挤成 3D 鸟瞰（楼块两档常量、每帧 38~64ms）。
// 新路子是让 iOS 的高德 SDK 直接渲染一张带透视的 3D 鸟瞰图（楼块、侧面、立交桥都由高德画），
// 固件只负责把它重投影到屏幕。固件仍然做重投影——因为透视底图与相机位置绑定，
// 车一动，同一栋楼对应的屏幕行就变了，纯平移贴图会立刻错位；但这里只需要「每行查一次表 + 横向等步长」，
// 不再做楼块检测/挤出/楼高运算，开销远小于 rasterRows3D。
//
// 协议见 fw147/firmware/docs/persp-3d-protocol.md：
//   头 32B（magic/P3D/1、flags、frame、w、h、锚点世界坐标、朝向、俯仰、距离范围、LUT 条数）
//   [+32B 可选 16 色调色板]
//   [LUT：lutN × {u16 画布行 v, u16 该距离处每画布列多少毫米}]
//   [像素：4bit 索引，行优先，高半字节 = 偶数列；(run-1, color) 双字节游程]
//
// 只有拿不到可用 P3D 帧（没收到 / 朝向差太多）时，才回退到原来的 rasterRows3D 路径。

#define P3D_MAX_W 1024
#define P3D_MAX_H 1024
#define P3D_BUF_BYTES (((1024 * 1024) / 2) + 4096)

bool p3dValid = false;
uint8_t *p3dBuf = nullptr;          // 4bit 索引像素，行优先
int p3dStride = 0;                  // 每行字节 = (w+1)/2
int p3dW = 0, p3dH = 0;
int32_t p3dAnchorX = 0, p3dAnchorY = 0;   // 渲染时相机正下方的世界坐标（米，与瓦片同一坐标系）
float p3dBearing = 0;                     // 渲染朝向（度，0 = 正北，顺时针）
float p3dPitch = 0;                       // 渲染俯仰（度，仅诊断）
float p3dDMin = 4.0f, p3dDMax = 2000.0f;  // LUT 覆盖的地面距离范围（米）
int p3dLutN = 0;
float p3dV[65];                           // 距离 → 画布行
float p3dMM[65];                          // 该距离处：每画布列多少米
uint16_t p3dPal[16];
bool p3dHasPal = false;
uint16_t p3dFrame = 0;
uint32_t p3dAt = 0, p3dCnt = 0;

static inline bool p3dReady() {
  return p3dValid && p3dBuf && p3dW > 0 && p3dH > 0 && p3dLutN >= 2;
}

// 朝向差（弧度）。超过 ~8° 时画布里的楼块朝向已经明显不对，宁可回退旧底图。
static inline float p3dBearingErr(float hdgRad) {
  float d = hdgRad - p3dBearing * (PI / 180.0f);
  while (d > PI) d -= 2 * PI;
  while (d < -PI) d += 2 * PI;
  return fabsf(d);
}

static inline bool p3dUsable(float hdgRad) {
  return p3dReady() && t3dActive && p3dBearingErr(hdgRad) <= 0.14f;
}

// 解码一帧 P3D。失败返回 false（保持旧帧，不影响现有地图）。
bool parseP3D(const uint8_t *p, uint32_t len) {
  if (len < 32 || p[0] != 'P' || p[1] != '3' || p[2] != 'D' || p[3] != '1') return false;
  const uint8_t flags = p[5];
  const uint16_t frame = rdu16(p + 6);
  const int w = rdu16(p + 8), h = rdu16(p + 10);
  if (w < 8 || h < 8 || w > P3D_MAX_W || h > P3D_MAX_H) return false;
  const int32_t ax = (int32_t)rd32(p + 12), ay = (int32_t)rd32(p + 16);
  const float bearing = (float)(int16_t)rdu16(p + 20) / 10.0f;
  const float pitch = (float)(int16_t)rdu16(p + 22) / 10.0f;
  float dmin = (float)rdu16(p + 24), dmax = (float)rdu16(p + 26);
  const int lutN = rdu16(p + 28);
  if (lutN < 2 || lutN > 64) return false;
  if (dmin < 1.0f) dmin = 1.0f;
  if (dmax <= dmin * 2.0f) return false;
  uint32_t pos = 32;
  uint16_t pal[16];
  if (flags & 1) {
    if (len < pos + 32) return false;
    for (int i = 0; i < 16; i++) pal[i] = rdu16(p + pos + 2 * i);
    pos += 32;
  }
  if (len < pos + (uint32_t)lutN * 4) return false;
  const int stride = (w + 1) / 2;
  const size_t need = (size_t)stride * (size_t)h;
  if (need > P3D_BUF_BYTES) return false;
  if (!p3dBuf) {
    p3dBuf = (uint8_t *)ps_malloc(P3D_BUF_BYTES);
    if (!p3dBuf) return false;
  }
  memset(p3dBuf, 0, need);
  uint32_t rp = pos + (uint32_t)lutN * 4;
  const uint32_t total = (uint32_t)w * (uint32_t)h;
  uint32_t i = 0;
  while (rp + 1 <= len && i < total) {
    const uint32_t run0 = (uint32_t)p[rp] + 1;
    const uint8_t c = p[rp + 1] & 15;
    rp += 2;
    uint32_t run = run0;
    if (run > total - i) run = total - i;
    for (uint32_t k = 0; k < run; k++, i++) {
      const uint32_t b = i >> 1;
      if (i & 1) p3dBuf[b] |= (uint8_t)c;
      else p3dBuf[b] = (uint8_t)(c << 4);
    }
  }
  if (i != total) return false;                 // 像素不完整：整帧丢弃，保留上一帧

  p3dLutN = lutN;
  for (int k = 0; k < lutN; k++) {
    p3dV[k] = (float)rdu16(p + pos + 4 * k);
    p3dMM[k] = (float)rdu16(p + pos + 4 * k + 2) / 1000.0f;
  }
  p3dW = w; p3dH = h; p3dStride = stride;
  p3dAnchorX = ax; p3dAnchorY = ay;
  p3dBearing = bearing; p3dPitch = pitch;
  p3dDMin = dmin; p3dDMax = dmax;
  p3dHasPal = (flags & 1) != 0;
  if (p3dHasPal) memcpy(p3dPal, pal, sizeof(p3dPal));
  p3dFrame = frame; p3dAt = millis(); p3dCnt++;
  p3dValid = true;
  return true;
}

// 把 P3D 画布重投影到整屏。返回 true = 本帧底图已画完（调用方跳过 rasterRows3D）。
bool p3dDrawRows(uint16_t *fb, const uint16_t *pal, float cx, float cy, float sc, float hdgRad) {
  if (!p3dUsable(hdgRad)) return false;
  const float D = t3dD(), PY = t3dPy(), span = t3dSpan(), inv = 1.0f / sc;
  const float br = p3dBearing * (PI / 180.0f);
  const float sb = sinf(br), cb = cosf(br);
  // 相机相对画布锚点的偏移（画布坐标：u 沿渲染朝向，L 向右）
  const float dx = cx - (float)p3dAnchorX, dy = cy - (float)p3dAnchorY;
  const float uPlus = dx * sb + dy * cb;
  const float lPlus = dx * cb - dy * sb;
  const float l2 = logf(p3dDMax / p3dDMin) / (float)(p3dLutN - 1);
  const int halfW = p3dW >> 1;
  const uint16_t fogC = T3D_FILL;
  for (int y = 0; y < LCD_HEIGHT; y++) {
    uint16_t *out = fb + y * LCD_WIDTH;
    const int xa = mapCircleRows.left[y], xb = mapCircleRows.right[y];
    for (int x = 0; x < xa; x++) out[x] = fogC;
    for (int x = xb + 1; x < LCD_WIDTH; x++) out[x] = fogC;
    if (xa > xb) continue;
    const float s = (PY - (y + 0.5f)) / span;
    if (s >= 0.965f) { for (int x = xa; x <= xb; x++) out[x] = fogC; continue; }
    const float ss = s < -2.0f ? -2.0f : s;
    const float u = D * ss / (1.0f - ss);              // 前方平面像素距离
    const float uc = u * inv + uPlus;                  // 距画布锚点的前向米数
    if (uc <= p3dDMin * 0.4f) { for (int x = xa; x <= xb; x++) out[x] = fogC; continue; }
    float t = uc > p3dDMin ? logf(uc / p3dDMin) / l2 : 0.0f;
    if (t > (float)(p3dLutN - 1)) t = (float)(p3dLutN - 1);
    const int i0 = (int)t;
    const int i1 = (i0 + 1 < p3dLutN) ? i0 + 1 : i0;
    const float ft = t - (float)i0;
    const float v = p3dV[i0] + (p3dV[i1] - p3dV[i0]) * ft;
    const float mm = p3dMM[i0] + (p3dMM[i1] - p3dMM[i0]) * ft;   // 米/画布列
    const int vr = (int)(v + 0.5f);
    if (mm <= 0.0005f || vr < 0 || vr >= p3dH) {
      for (int x = xa; x <= xb; x++) out[x] = fogC;
      continue;
    }
    const float latM = inv / (1.0f - ss);              // 屏幕横向 米/像素
    const float stepCol = latM / mm;                   // 画布列 / 屏幕像素
    float col = (float)halfW + lPlus / mm + ((float)xa + 0.5f - CX) * stepCol;
    const uint8_t *row = p3dBuf + (size_t)vr * (size_t)p3dStride;
    const int wm1 = p3dW - 1;
    for (int x = xa; x <= xb; x++, col += stepCol) {
      const int ci = (int)(col + 0.5f);
      if (ci < 0 || ci > wm1) { out[x] = fogC; continue; }
      const uint8_t by = row[ci >> 1];
      const uint8_t idx = (ci & 1) ? (uint8_t)(by & 15) : (uint8_t)(by >> 4);
      out[x] = pal[idx];
    }
  }
  return true;
}
