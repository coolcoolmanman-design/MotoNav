#pragma once
// musicA.134（源自 musicA.130 zip）：3D 鸟瞰小地图（参考视频「矢量简图3」）——整张地图按透视倾斜，楼块向上拉伸。
// musicA.136：画质固定编译期「均衡」密楼（T3D_QUALITY=1）；取消运行时 #TQ 三档（曾误关挤出）。
// 由 MotoNav.ino 在 RasterRenderer.h 之前包含，仅编译一次。
//
// 开启方式：地图样式 15「矢量3D鸟瞰」自动开；也可发 "#3D,1/0" 手动开关。
// 楼块来源：iOS 对 style 15 / 罪恶都市 / GTA6 开建筑轮廓后，位图索引 4..7 是楼块（4/6 屋顶，5/7 轮廓）。
//           没有楼块数据时只做透视地面，不会出错。
//
// 编译期开关（可在 FirmwareConfig.h 里先 #define 覆盖）：
#ifndef T3D_QUALITY
#define T3D_QUALITY 1        // 0 = 仅透视地面；1 = 单一楼高（默认均衡密楼）；2 = 两档楼高
#endif
#ifndef T3D_QUALITY_DEFAULT
#define T3D_QUALITY_DEFAULT T3D_QUALITY  // 兼容旧 NVS/协议：始终回写为均衡
#endif
#ifndef T3D_OWN_PALETTE
#define T3D_OWN_PALETTE 1    // 1 = 用视频里的浅灰白 / 夜间深蓝灰配色；0 = 沿用当前主题配色
#endif
#define T3D_PY        330       // 透视模式下车头的屏幕 y（平面模式仍是 MAP_PY）
#define T3D_HORIZON   96        // 无穷远处的屏幕 y（地平线）
#define T3D_D         240.0f    // 透视距离常数（像素）：越小越陡
#define T3D_VK        1.25f     // 楼高的垂直拉伸系数
#define T3D_H_TALL_M  42.0f     // 高楼（米）
#define T3D_H_LOW_M   20.0f     // 矮楼（米）

bool t3dOn = false;             // "#3D,1/0"
bool t3dActive = false;         // 每帧 drawMapFrame 里决定：t3dOn 且当前画法支持透视
int  t3dQuality = T3D_QUALITY_DEFAULT;  // musicA.136：固定均衡；#TQ 仅回写/ACK 1
#define MAP_CAR_Y (t3dActive ? T3D_PY : MAP_PY)   // 车头箭头所在的屏幕 y
#define T3D_FILL  (T3D_OWN_PALETTE ? (nightOn ? 0x18E5 : 0xEF5D) : C_BLOCK)   // 整屏底色（与自带配色的地面色一致）

// 平面屏幕坐标（车在 (CX, MAP_PY)、车头朝上）→ 透视屏幕坐标。返回该点的线宽缩放 k（近大远小）。
// 这是一个单应变换，直线仍是直线：x' = CX + (x-CX)·D/(u+D)，y' = PY − (PY−H)·u/(u+D)，u = MAP_PY − y。
static inline float t3dPt(float &x, float &y) {
  float u = (float)MAP_PY - y;
  const float uMin = -0.6f * T3D_D;                 // 车后最多看 0.6·D，避免除零
  if (u < uMin) u = uMin;
  const float k = T3D_D / (T3D_D + u);
  x = CX + (x - CX) * k;
  y = T3D_PY - (float)(T3D_PY - T3D_HORIZON) * (u / (u + T3D_D));
  return k;
}
static inline void t3dXY(float &x, float &y) { if (t3dActive) t3dPt(x, y); }
static inline void t3dArray(float *xs, float *ys, int n) {
  if (!t3dActive) return;
  for (int i = 0; i < n; i++) t3dPt(xs[i], ys[i]);
}
// 已透视后的屏幕 y → 线宽缩放（近 1、远趋近 0）；用于路线接地近粗远细。
static inline float t3dWidthK(float sy) {
  const float span = (float)(T3D_PY - T3D_HORIZON);
  if (span < 1.0f) return 1.0f;
  float s = ((float)T3D_PY - sy) / span;
  if (s < 0) s = 0;
  if (s > 0.92f) s = 0.92f;
  float k = 1.0f - s;
  if (k < 0.12f) k = 0.12f;
  return k;
}
