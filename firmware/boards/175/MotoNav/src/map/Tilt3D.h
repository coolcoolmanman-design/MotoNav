#pragma once
// musicA.134（源自 musicA.130 zip）：3D 鸟瞰小地图（参考视频「矢量简图3」）——整张地图按透视倾斜，楼块向上拉伸。
// musicA.136：画质固定编译期「均衡」密楼（T3D_QUALITY=1）；取消运行时 #TQ 三档（曾误关挤出）。
// musicA.137：远端 HUD 裁切——导航蓝线截到地网/楼块平面尽头；地平线以上 / frustum 外不画红绿灯。
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
#ifndef T3D_MAIN_SHARE
#define T3D_MAIN_SHARE 0.60f    // musicA.143：第一帧按开销估算时主核心（核心 1）承担的比例；之后按实测用时自动微调
#endif
// musicA.137：与 Tilt3DRaster s≥0.97「只填雾色」对齐；略留余量，蓝线停在地网尽头而非贴地平线横条
// musicA.139：0.92 与栅格真实地面尽头 0.97 差了 5% 屏幕距离（约 12px），蓝线/红绿灯会画进
//             「只有雾色、没有地网」的空档，看着像飘出地面尽头。收紧到 0.965（留约 1px 余量）。
// musicA.140：实机证明 .139 方向错了——0.965 对应 y≈98，离地平线 y=90 仅 8px，蓝线几乎画到
//             地平线，加上远处雾色渐隐全白，看就是「穿出地网飘进白雾区」。回退到 0.75（y≈150，
//             屏幕约 32%），与实机可辨认的楼块地网上沿对齐；0.92/0.965 都太贴地平线。
#ifndef T3D_GROUND_EDGE_S
#define T3D_GROUND_EDGE_S 0.75f
#endif

bool t3dOn = false;             // "#3D,1/0"
bool t3dActive = false;         // 每帧 drawMapFrame 里决定：t3dOn 且当前画法支持透视
int  t3dQuality = T3D_QUALITY_DEFAULT;  // musicA.136：固定均衡；#TQ 仅回写/ACK 1
// musicA.142：高架/桥梁悬浮。App 按当前 link 路名（高架/立交/大桥/匝道/桥）发 "#ELV,<0|1|2>"：
// 0 = 地面；1 = 高架·立交·大桥；2 = 匝道·中小桥。
// musicA.144：14px/8px 实机看不出高度（466 圆的 3%），加到 28px/16px，并补桥腹侧面（见 RouteRenderer）。
int   elevLevel = 0;
float elevCur = 0.0f;           // 当前抬高像素（平滑；绘制处再乘近大远小 k）
static inline float elevTgtPx() { return elevLevel >= 2 ? 16.0f : (elevLevel == 1 ? 28.0f : 0.0f); }
// musicA.142：高架时车头箭头跟着浮到桥面，否则会贴在地面显得「车在桥下」
#define MAP_CAR_Y (t3dActive ? (t3dPy() - elevCur) : (float)MAP_PY)   // 车头箭头所在的屏幕 y（随倾斜系数回到平面）
// musicA.147c：整屏/圆外底色改为变量——样式16 要跟青蓝地面同色，不能再用固定的浅灰白。
static uint16_t t3dBgCol = 0xEF5D;
static inline void t3dSyncBg() {
  if (styleIdx() == ST_DAYNIGHT_GLASS) t3dBgCol = C_BLOCK;      // musicA.150：地面色统一取主题表
  else t3dBgCol = T3D_OWN_PALETTE ? (nightOn ? 0x18E5 : 0xEF5D) : C_BLOCK;
}
#define T3D_FILL (t3dBgCol)

// musicA.139：等红灯平视化——连续倾斜系数，1 = 全 3D 鸟瞰，0.02 ≈ 平面正交。
// 不切换渲染路径（那样会突变），而是把透视距离参数化：D_eff = T3D_D / blend，
// blend→0 时 D→∞，单应变换数学上自然退化为平面（u = MAP_PY - y），视觉平滑。
// 只在样式15/Tilt3D 下由停车状态驱动（见 MapFrame.h），其它样式恒为 1。
#define T3D_BLEND_MIN 0.02f
static float t3dBlend = 1.0f;
// 透视常数 / 车头 y / 地平线随 blend 插值；span = D_eff，保证 D→∞ 时精确退化为 u=w。
static inline float t3dD()    { return (float)T3D_D / (t3dBlend > T3D_BLEND_MIN ? t3dBlend : T3D_BLEND_MIN); }
static inline float t3dPy()   { return (float)MAP_PY + ((float)T3D_PY - (float)MAP_PY) * t3dBlend; }
static inline float t3dSpan() { return t3dD(); }                 // span = PY - H = D
static inline float t3dDrawnBlend() { return t3dBlend; }        // 楼块高度按此压平

// 平面屏幕坐标（车在 (CX, MAP_PY)、车头朝上）→ 透视屏幕坐标。返回该点的线宽缩放 k（近大远小）。
// 这是一个单应变换，直线仍是直线：x' = CX + (x-CX)·D/(u+D)，y' = PY − (PY−H)·u/(u+D)，u = MAP_PY − y。
static inline float t3dPt(float &x, float &y) {
  float u = (float)MAP_PY - y;
  const float d = t3dD();                            // musicA.139：随倾斜系数变化（停车时 →∞ 退化平面）
  const float uMin = -0.6f * d;                      // 车后最多看 0.6·d，避免除零
  if (u < uMin) u = uMin;
  const float k = d / (d + u);
  x = CX + (x - CX) * k;
  y = t3dPy() - t3dSpan() * (u / (u + d));
  return k;
}
static inline void t3dXY(float &x, float &y) { if (t3dActive) t3dPt(x, y); }
static inline void t3dArray(float *xs, float *ys, int n) {
  if (!t3dActive) return;
  for (int i = 0; i < n; i++) t3dPt(xs[i], ys[i]);
}
// musicA.137：地网/楼块平面尽头的屏幕 y（更小 y = 更远/天空侧）。蓝线与灯以此裁切。
// musicA.139：随倾斜系数走；blend→0 时 span→∞，edge→负无穷，裁切自然失效（平面模式无天空）。
static inline float t3dGroundEdgeY() {
  return t3dPy() - T3D_GROUND_EDGE_S * t3dSpan();
}
// 点是否落在可画地平面上（含圆屏视锥近似）：地平线以上 / 雷达外 = 否
static inline bool t3dOnGroundFrustum(float px, float py) {
  if (py < t3dGroundEdgeY()) return false;
  float ex = px - CX, ey = py - CX;
  return ex * ex + ey * ey <= 196.0f * 196.0f;
}
// 已透视后的屏幕 y → 线宽缩放（近 1、远趋近 0）；用于路线接地近粗远细。
static inline float t3dWidthK(float sy) {
  const float span = t3dSpan();
  if (span < 1.0f) return 1.0f;
  float s = ((float)T3D_PY - sy) / span;
  if (s < 0) s = 0;
  if (s > 0.92f) s = 0.92f;
  float k = 1.0f - s;
  if (k < 0.12f) k = 0.12f;
  return k;
}
