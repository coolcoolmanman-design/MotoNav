#pragma once
// 首页骑手动画和等待画面。由 MotoNav.ino 按依赖顺序包含，仅编译一次。
// ================= 整屏画面 =================

#define WAIT_READY  0
#define WAIT_NOLINK 1
#define WAIT_LOST   2

// ================= 塞尔达40周年特别版 待机动画（v7.12.10-ui.2-zelda40） =================
// 在线：林克单手抱塞尔达骑兽形摩托，大师之剑 举/挥 两帧循环（1.3s/帧），
//       车身 ±3px 悬浮、车尾尾气、风线刮过（青光呼吸烘在精灵颜色里）
// 离线：故障摩托在左冒白烟（青光喘熄烘在精灵里），林克躺在塞尔达怀里睡（右），ZZZ 漂浮
// 素材由 motonav-effect/zelda_assets.py 生成；布局常量与效果图 anim_v4.py 定稿一致
// musicA.19：ZeldaHomeBg.h（待机纹理 434KB）不再使用，include 已移除（表盘选择下线、背景透明黑）
#include "../../resources/ZeldaHomeSprites.h"

// ================= 待机页可换表盘（faceId, v7.12.10-ui.x） =================
// 背景源在「塞尔达纹理」(faceId<=1) 与「程序化表盘」(faceId>=2) 之间切换；
// zbgRect 统一从 homeBg 拷贝，表盘切换只改 homeBg 指向即可。
static uint16_t* homeBgBuf = nullptr;          // PSRAM：程序化表盘渲染到的全屏缓冲
// musicA.19：表盘选择功能下线——待机背景不再用塞尔达纹理/程序化主题，
// 默认透明（AMOLED 纯黑），仅自定义开机背景（bgCustom）优先。homeBg 保留变量壳避免大面积改动。
const uint16_t* homeBg = nullptr;              // musicA.19 起恒空（透明黑背景）
void applyFace(int id);                         // 切换背景源（渲染 / 回退到塞尔达纹理）
void renderFaceBg(int id);                      // 程序化渲染某一款到 homeBgBuf
void drawFaceMotif(unsigned long now, int st);  // 非塞尔达表盘中央主视觉（每拍重绘）

// ================= 二期C 组件槽（v7.12.10-musicA.8 / musicA.17 六槽重构） =================
// 六槽：时间（固定）+ 左上/右上/下方（功能可切 0速度 1G值 2胎压 3天气）+ 中央动画（固定）+ 连接态（固定）。
// 电量恒显示在顶部不占槽；固定三槽可显隐调色（App 经 #FC2 下发），载具经 #FV 切换。
#include "../../resources/RiderSprites.h"      // 旧版像素小人×摩托（vehId=1）
#include "../../resources/Gta6CenterSleep.h"   // musicA.46：中央槽 GTA VI 未连接
#include "../../resources/Gta6CenterChase.h"   // musicA.46：中央槽 GTA VI 已连接
#include <esp_heap_caps.h>                     // musicA.49：GTA 中央帧 PSRAM scratch
extern int tpFrontKpa, tpRearKpa;               // TpmsPage.h 定义（本文件先包含，前置声明）
extern bool tpValid;                            // TpmsPage.h：是否收到过真实 #TP 读数

#define Z_GOLD     0xFE27                                       // 金黄 (255,200,60)
#define Z_GOLD_SUB 0xD5F1                                       // 暗金 (210,190,140)
// musicA.17 六槽几何（466 圆屏，圆心 233,233）
#define BAND_Y0   136                           // 中央动画带顶（大时钟下沿 130 之下）
#define BAND_H    224                           // 中央动画带高（136..360）
#define SLOT_Y    148                           // 左右槽内容顶
#define SLOT_LX   84                            // 左上槽中心 x
#define SLOT_RX   382                           // 右上槽中心 x
#define SLOTB_Y   360                           // 下方槽内容顶
#define CONN_Y    420                           // 连接态文字 y

bool onWaitScreen = false;
bool sliding = false;                       // v5.5 正在拖动/滑动换页：别的画面先别画
int waitSt = 0;
int riderPrevF = -1, riderPrevZ = -2;       // 变量名保留（PageNavigation.h 沿用）
bool catForce = false;                      // 首页刚重画：马上画一次

// 背景优先级：上传的自定义壁纸 > 预设动态背景当前帧 > 原有透明黑。
// 上传壁纸时 dynBgActive() 为假 → 动态帧暂停但已选风格保留；恢复默认后自动续上。
const uint16_t *bgSrc() {
  if (bgCustomOn && bgCustom) return bgCustom;
  if (dynBgActive() && dynBgBuf) return dynBgBuf;
  return homeBg;
}

// musicA.19 背景透明化：无自定义背景时直接填黑（AMOLED 透明观感）
void zbgRect(int x, int y, int w, int h) {
  uint16_t *fb = gfx->getFramebuffer();
  const uint16_t *bg = bgSrc();
  if (!fb || !bg) { gfx->fillRect(x, y, w, h, C_BLACK); return; }
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > 466) w = 466 - x;
  if (y + h > 466) h = 466 - y;
  if (w <= 0 || h <= 0) return;
  for (int yy = 0; yy < h; yy++)
    memcpy(fb + (size_t)(y + yy) * 466 + x, bg + (size_t)(y + yy) * 466 + x, w * 2);
}

// StatusWidgets.h 的擦除钩子：待机页恢复纹理背景，其他页黑底
void statusErase(int x, int y, int w, int h) {
  if (onWaitScreen && !sliding) { zbgRect(x, y, w, h); return; }
  gfx->fillRect(x, y, w, h, C_BLACK);
}

// 网格精灵按 cell 像素放大绘制（1bpp 掩码跳过背景；缺省 3px，离线场景用 2px 缩小）
void zBlit(int x0, int y0, int w, int h, const uint16_t *rgb, const uint8_t *mask, int cell = ZSPRITE_CELL) {
  uint16_t *fb = gfx->getFramebuffer();
  if (!fb) return;
  const int bpr = (w + 7) / 8;
  for (int yy = 0; yy < h; yy++) {
    for (int xx = 0; xx < w; xx++) {
      if (!((mask[yy * bpr + (xx >> 3)] >> (xx & 7)) & 1)) continue;
      uint16_t c = rgb[yy * w + xx];
      int px = x0 + xx * cell, py = y0 + yy * cell;
      for (int cy = 0; cy < cell; cy++)
        for (int cx = 0; cx < cell; cx++) {
          int X = px + cx, Y = py + cy;
          if ((unsigned)X < 466 && (unsigned)Y < 466) fb[Y * 466 + X] = c;
        }
    }
  }
}

// 在线：抱塞尔达骑行，举/挥两帧 + 悬浮 + 尾气 + 风线
// musicA.17：cell 3→4 放大（200×224），场景带 136..360（上不碰大时钟、下不碰下方槽）
void drawZeldaOnline(unsigned long now) {
  const int w = Z_ON_RAISE_W;
  const int cell = 4;                                                    // musicA.17 放大（原 ZSPRITE_CELL=3）
  const int gx0 = CX - w * cell / 2;
  zbgRect(0, BAND_Y0, 466, BAND_H);                                       // 场景带整条恢复（含左右槽区，槽随后重画）
  float phase = (now % 2600) / 2600.0f;
  static const float wspd[4] = {1.15f, 0.85f, 1.35f, 0.65f};
  static const int wy[4] = {190, 228, 266, 304};
  static const int wln[4] = {46, 34, 52, 30};
  for (int k = 0; k < 4; k++) {                                          // 风线（精灵后面）
    float off = fmodf(phase * wspd[k] + k * 0.37f, 1.0f);
    int xh = (int)(-60 + off * (466 + 120));
    const uint16_t lc[3] = {0x07FF, 0x03EF, 0x0142};
    for (int s = 0; s < 3; s++) {
      int lx = xh - s * wln[k];
      int a0 = lx < 0 ? 0 : lx, a1 = lx + wln[k] > 466 ? 466 : lx + wln[k];
      if (a1 > a0) gfx->fillRect(a0, wy[k], a1 - a0, 2, lc[s]);
    }
  }
  bool slash = (now / Z_ONLINE_FRAME_MS) % 2;
  const uint16_t *rgb = slash ? Z_ON_SLASH_RGB : Z_ON_RAISE_RGB;
  const uint8_t *mk = slash ? Z_ON_SLASH_MASK : Z_ON_RAISE_MASK;
  int dy = (int)lroundf(2.0f * sinf(phase * 6.2831853f));
  zBlit(gx0, BAND_Y0 - 2 + dy, w, Z_ON_RAISE_H, rgb, mk, cell);          // musicA.17 cell 4 放大
  int tx = gx0 + (int)(w * cell * 0.90f);                                // 车尾尾气
  int ty = BAND_Y0 - 2 + (int)(Z_ON_RAISE_H * cell * 0.55f) + dy;
  for (int k = 0; k < 4; k++) {
    float life = fmodf(phase * 1.6f + k / 4.0f, 1.0f);
    int px = (int)(tx + life * 26), py = (int)(ty - life * 30);
    int rad = 2 + (int)(life * 5);
    uint16_t c = BootAnimation::zeldaScale(0xCE79, (uint32_t)((1.0f - life) * 160));
    aaDisc(px, py, rad * 3, c);
  }
}

// 离线：故障摩托（左）冒白烟 + 林克躺塞尔达怀里睡（右）+ ZZZ
// zelda40.1：2px 网格缩小（原 3px 太大，场景带擦到了时钟下部），场景带 150..325
void drawZeldaOffline(unsigned long now) {
  float phase = (now % 2000) / 2000.0f;
  const int cell = 2;
  const int bw = Z_OFF_BIKE_W * cell, dw = Z_OFF_DOZE_W * cell;
  const int total = bw + Z_OFF_GAP_PX + dw;
  const int x0 = CX - total / 2;
  const int bx0 = x0, by0 = Z_OFF_BIKE_BOTTOM - Z_OFF_BIKE_H * cell;     // 底边 318 不变
  const int gx0 = x0 + bw + Z_OFF_GAP_PX;
  const int gy0 = Z_OFF_BIKE_BOTTOM - Z_OFF_DOZE_H * cell + ((phase < 0.5f) ? 1 : 0);
  zbgRect(bx0 - 30, 150, total + 60, 175);                               // 场景区恢复（150..325，白烟/ZZZ 均在带内）
  zBlit(bx0, by0, Z_OFF_BIKE_W, Z_OFF_BIKE_H, Z_OFF_BIKE_RGB, Z_OFF_BIKE_MASK, cell);
  zBlit(gx0, gy0, Z_OFF_DOZE_W, Z_OFF_DOZE_H, Z_OFF_DOZE_RGB, Z_OFF_DOZE_MASK, cell);
  int sx = (int)(bx0 + bw * 0.45f), sy = (int)(by0 + Z_OFF_BIKE_H * cell * 0.30f);
  for (int k = 0; k < 6; k++) {                                          // 白烟（只画明亮段）
    float life = fmodf(phase * 1.4f + k / 6.0f, 1.0f);
    float a = (1.0f - life) * 0.9f;
    if (a < 0.30f) continue;
    int px = (int)(sx + sinf(life * 3.1f + k * 1.9f) * 9 - life * 7);
    int py = (int)(sy - life * 60);
    int rad = 2 + (int)(life * 5);
    aaDisc(px, py, rad * 3, BootAnimation::zeldaScale(0xFFFF, (uint32_t)(a * 255)));
  }
  int hx = gx0 + (int)(Z_ZZZ_GX * cell) - 2;                             // ZZZ 向左上漂
  int hy = gy0 + (int)(Z_ZZZ_GY * cell) - 20;
  for (int k = 0; k < 3; k++) {
    float life = fmodf(phase * 3.0f + k / 3.0f, 1.0f);
    int zx = (int)(hx + k * 8 - life * 16);
    int zy = (int)(hy - life * 52);
    printCentered("Z", zx, zy, 2, life < 0.5f ? C_CYAN : C_GREY);
  }
}

// ================= musicA.17 中央动画载具：像素小人×摩托（vehId=1，旧版素材复用） =================
extern bool wxHave;                // WeatherPage.h（后包含，前置声明）
extern int wxIcon, wxTemp;
void drawWxIcon(int id, float cx, float cy, float s);
extern float gPeak;                // GForcePage.h（后包含，前置声明）
extern float gCur;                 // 当前 G（#G 处理 / G 页更新）
bool gLive();                      // 手机 #G 是否仍在更新（GForcePage.h）

// musicA.21：功能槽 G 值改「实时」——手机 #G（2Hz，3 秒内有效）优先，否则板载 IMU 现采；
// 都没有就显示 0.00。峰值 gPeak 仍在后台跟踪（G 页 / #G 处理），这里不改其语义。
float liveG() {
  if (gLive()) return gCur;
  if (imuOK) {
    float g = 0, dps = 0;
    if (imuPoll(g, dps)) {
      gCur = constrain(g, 0.0f, 8.0f);
      if (gCur > gPeak) gPeak = gCur;
      return gCur;
    }
  }
  return 0;
}
uint16_t riderColor(char ch) {                     // 旧版调色板（像素骑手，来自 v4.6）
  switch (ch) {
    case 'K': return 0x18C6; case 'H': return 0x2E57; case 'h': return 0xAFBD;
    case 'V': return 0x29ED; case 'v': return 0x7D1C; case 'S': return 0xFEB5;
    case 'E': return 0x18C6; case 'B': return 0xFCB2; case 'R': return 0x9225;
    case 'J': return 0xFC65; case 'j': return 0xD2E2; case 'F': return 0xFF07;
    case 'G': return 0x39E8; case 'P': return 0x3A91; case 'p': return 0x29AC;
    case 'b': return 0x6A25; case 'W': return 0xFFFF; case 'M': return 0xE187;
    case 'm': return 0xA0E5; case 'C': return 0xCE7A; case 'c': return 0x8432;
    case 'T': return 0x2966; case 't': return 0x94B4; case 'L': return 0xFF8F;
    case 'l': return 0x6B6C; case 's': return 0x4185;
    default: return 0;
  }
}

void drawRiderPixel(unsigned long now, bool online) {
  const int cell = 5;                              // 40×26 → 200×130
  const int x0 = CX - RIDER_W * cell / 2;
  const int y0 = 196;
  int f;
  if (online) f = ((now / 900) % 2) ? 0 : 1;       // stand0 / stand1 交替
  else f = 6;                                      // 离线：睡帧
  int dy = online ? (int)lroundf(2.0f * sinf((now % 2600) / 2600.0f * 6.2831853f)) : 0;
  for (int yy = 0; yy < RIDER_H; yy++) {
    const char *row = RIDER[f][yy];
    for (int xx = 0; row[xx] && xx < RIDER_W; xx++) {
      uint16_t c = riderColor(row[xx]);
      if (!c && row[xx] != 'K') continue;
      if (!c) c = 0x18C6;
      gfx->fillRect(x0 + xx * cell, y0 + yy * cell + dy, cell, cell, c);
    }
  }
}

// musicA.49-ui.1：中央槽 GTA VI — 黑键透明叠到 dynBg/自定义壁纸；
// 方案 A：以 (233,248) 为锚整体放大 1.5×（内容约 245px），并改到 animateCat 末尾最后绘制（最上层），
// 允许人物/车轻微压住上下左右槽；清残影改由 animateCat 提前扩大到 y100..400 完成。
#define GTA_ANCHOR_X 233
#define GTA_ANCHOR_Y 248
#define GTA_SCALE    1.5f
void drawGtaCenter(unsigned long now, bool online) {
  uint16_t *fb = gfx->getFramebuffer();
  if (!fb) return;
  int fi;
  const uint8_t *zbase;
  const uint32_t *zoff;
  const uint32_t *zlen;
  if (online) {
    fi = (int)((now % GTA6_CHASE_TOTAL_MS) / GTA6_CHASE_FRAME_MS);
    if (fi >= GTA6_CHASE_COUNT) fi = GTA6_CHASE_COUNT - 1;
    zbase = GTA6_CHASE_ZDATA; zoff = GTA6_CHASE_ZOFF; zlen = GTA6_CHASE_ZLEN;
  } else {
    fi = (int)((now % GTA6_SLEEP_TOTAL_MS) / GTA6_SLEEP_FRAME_MS);
    if (fi >= GTA6_SLEEP_COUNT) fi = GTA6_SLEEP_COUNT - 1;
    zbase = GTA6_SLEEP_ZDATA; zoff = GTA6_SLEEP_ZOFF; zlen = GTA6_SLEEP_ZLEN;
  }
  static uint16_t *gtaScratch = nullptr;
  if (!gtaScratch) {
    gtaScratch = (uint16_t *)heap_caps_malloc((size_t)466 * 466 * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
  }
  if (!gtaScratch) return;                                   // 置顶绘制不能用整屏不透明 blit 兜底
  const size_t need = (size_t)466 * 466 * 2;
  if (!BootAnimation::inflateZlibRgb565(zbase + zoff[fi], zlen[fi], gtaScratch, need)) return;
  // 最近邻放样：目标像素 (dx,dy) 反查源像素，0 = 透明键，保留壁纸/槽内容
  static int16_t sxMap[466];
  static bool sxMapReady = false;
  if (!sxMapReady) {
    for (int x = 0; x < 466; x++)
      sxMap[x] = (int16_t)(GTA_ANCHOR_X + (int)lroundf((x - GTA_ANCHOR_X) / GTA_SCALE));
    sxMapReady = true;
  }
  for (int dy = 0; dy < 466; dy++) {
    int sy = GTA_ANCHOR_Y + (int)lroundf((dy - GTA_ANCHOR_Y) / GTA_SCALE);
    if (sy < 0 || sy >= 466) continue;
    const uint16_t *src = gtaScratch + (size_t)sy * 466;
    uint16_t *dst = fb + (size_t)dy * 466;
    for (int dx = 0; dx < 466; dx++) {
      int sx = sxMap[dx];
      if (sx < 0 || sx >= 466) continue;
      uint16_t c = src[sx];
      if (c) dst[dx] = c;
    }
  }
}

// ================= musicA.18 中央动画载具：像素女孩×自行车（vehId=2，重绘） =================
// 参考实车像素图重画：橙色折叠车架 + 厚黑轮胎 + 灰色前叉；女孩坐姿朝右（丸子头+红发卡+粉上衣）。
// 60x44 低分辨率网格绘制，cell 4 放大成 240x176——真像素画风格，替代 musicA.17 的简笔画。
#define BIKE_GW 60
#define BIKE_GH 44
#define BIKE_CELL 4
static uint8_t bikeGrid[BIKE_GW * BIKE_GH];
enum { BK_T = 0, BK_K, BK_o, BK_O, BK_G, BK_W, BK_S, BK_H, BK_P, BK_D, BK_R };
static uint16_t bikePal(int i) {
  switch (i) {
    case BK_K: return 0x0000;                       // 轮胎 / 鞍座 黑
    case BK_o: return 0x2124;                       // 暗灰（轮圈内缘）
    case BK_O: return 0xFCA3;                       // 车架 橙
    case BK_G: return 0x8C71;                       // 前叉 / 毂 / 灰
    case BK_W: return 0xC618;                       // 踏板 浅灰
    case BK_S: return 0xFED8;                       // 皮肤
    case BK_H: return 0x6AC7;                       // 头发 棕
    case BK_P: return 0xFDB7;                       // 上衣 / 裙 粉
    case BK_D: return 0x3A2F;                       // 腿 牛仔蓝
    case BK_R: return 0xF800;                       // 发卡 红
    default:   return 0x0000;
  }
}
static void bSet(int x, int y, int v) {
  if ((unsigned)x < BIKE_GW && (unsigned)y < BIKE_GH) bikeGrid[y * BIKE_GW + x] = (uint8_t)v;
}
static void bLine(int x0, int y0, int x1, int y1, int v, int w = 1) {   // Bresenham + 粗细
  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  for (;;) {
    for (int oy = 0; oy < w; oy++)
      for (int ox = 0; ox < w; ox++) bSet(x0 + ox, y0 + oy, v);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}
static void bDisc(int cx, int cy, int r, int v) {
  for (int y = -r; y <= r; y++)
    for (int x = -r; x <= r; x++)
      if (x * x + y * y <= r * r) bSet(cx + x, cy + y, v);
}
static void bRing(int cx, int cy, int r, int v) {                      // 2px 环
  for (int y = -r; y <= r; y++)
    for (int x = -r; x <= r; x++) {
      int d = x * x + y * y;
      if (d <= r * r && d >= (r - 2) * (r - 2)) bSet(cx + x, cy + y, v);
    }
}
void drawBikeRider(unsigned long now, bool online) {
  memset(bikeGrid, 0, sizeof(bikeGrid));
  const int RCX = 13, FCX = 47, WY = 32, CKX = 30, CKY = 31;   // 后/前轮心、牙盘
  float pa = online ? (now % 1800) / 1800.0f * 6.2831853f : 0.8f;
  // 车轮：厚黑胎 + 暗灰内缘 + 灰毂 + 旋转辐条
  bRing(RCX, WY, 10, BK_K); bRing(FCX, WY, 10, BK_K);
  bRing(RCX, WY, 7, BK_o);  bRing(FCX, WY, 7, BK_o);
  bDisc(RCX, WY, 2, BK_G);  bDisc(FCX, WY, 2, BK_G);
  for (int k = 0; k < 3; k++) {
    float a = pa * 2.0f + k * 2.0943951f;
    bLine(RCX, WY, RCX + (int)(5 * cosf(a)), WY + (int)(5 * sinf(a)), BK_G);
    bLine(FCX, WY, FCX + (int)(5 * cosf(a)), WY + (int)(5 * sinf(a)), BK_G);
  }
  // 车架（橙，参考图折叠车几何）+ 灰前叉 + 折叠扣
  bLine(RCX, WY, CKX, CKY, BK_O, 2);               // 后下叉
  bLine(RCX, WY, 26, 18, BK_O, 2);                 // 后上叉
  bLine(CKX, CKY, 25, 16, BK_O, 2);                // 座管
  bLine(25, 16, 42, 15, BK_O, 2);                  // 上管
  bLine(43, 15, CKX, CKY, BK_O, 2);                // 下管
  bLine(44, 13, FCX, WY, BK_G, 2);                 // 前叉
  bLine(44, 13, 42, 9, BK_G, 1);                   // 立管
  bLine(39, 9, 44, 9, BK_K, 2);                    // 把手
  bDisc(35, 22, 1, BK_G);                          // 折叠铰扣
  // 牙盘 + 曲柄踏板（旋转）
  bDisc(CKX, CKY, 3, BK_G);
  int p1x = CKX + (int)(6 * cosf(pa)), p1y = CKY + (int)(6 * sinf(pa));
  int p2x = CKX - (int)(6 * cosf(pa)), p2y = CKY - (int)(6 * sinf(pa));
  bLine(CKX, CKY, p1x, p1y, BK_K); bLine(CKX, CKY, p2x, p2y, BK_K);
  bLine(p1x - 2, p1y, p1x + 2, p1y, BK_W); bLine(p2x - 2, p2y, p2x + 2, p2y, BK_W);
  // 鞍座
  bLine(20, 14, 27, 14, BK_K, 2);
  // ---- 像素女孩（坐姿朝右）：腿 → 裙 → 上身 → 手臂 → 头 ----
  const int HX = 23, HY = 13;                      // 髋部（鞍座上）
  int k1x = (HX + p1x) / 2 + 2, k1y = (HY + p1y) / 2 - 1;
  int k2x = (HX + p2x) / 2 + 2, k2y = (HY + p2y) / 2 - 1;
  bLine(HX, HY, k1x, k1y, BK_D, 2); bLine(k1x, k1y, p1x, p1y, BK_D, 2);
  bLine(HX, HY, k2x, k2y, BK_D, 2); bLine(k2x, k2y, p2x, p2y, BK_D, 2);
  bLine(HX - 3, HY - 1, HX + 3, HY - 1, BK_P, 2);  // 裙摆
  bLine(HX, HY - 1, 27, 7, BK_P, 3);               // 上身（前倾）
  bLine(27, 8, 34, 10, BK_S);                      // 手臂两段 → 车把
  bLine(34, 10, 42, 10, BK_S);
  bDisc(30, 4, 3, BK_S);                           // 头
  for (int x = 27; x <= 33; x++) bSet(x, 1, BK_H); // 发际
  bDisc(28, 3, 2, BK_H);                           // 后脑发量
  bDisc(26, 1, 1, BK_H);                           // 丸子头
  bSet(32, 1, BK_R); bSet(33, 2, BK_R);            // 红色发卡
  bSet(32, 4, BK_K);                               // 眼睛
  // 网格 → 屏幕（cell 4，按色块行程 fillRect；透明格露出背景）
  int gx0 = CX - BIKE_GW * BIKE_CELL / 2, gy0 = 176;
  for (int y = 0; y < BIKE_GH; y++) {
    int yy = gy0 + y * BIKE_CELL;
    if (yy < 0 || yy + BIKE_CELL > 466) continue;
    int x = 0;
    while (x < BIKE_GW) {
      uint8_t v = bikeGrid[y * BIKE_GW + x];
      if (!v) { x++; continue; }
      int x2 = x;
      while (x2 < BIKE_GW && bikeGrid[y * BIKE_GW + x2] == v) x2++;
      int sx = gx0 + x * BIKE_CELL, ex = gx0 + x2 * BIKE_CELL;
      if (sx < 0) sx = 0;
      if (ex > 466) ex = 466;
      if (ex > sx) gfx->fillRect(sx, yy, ex - sx, BIKE_CELL, bikePal(v));
      x = x2;
    }
  }
}

// ================= musicA.17 六槽绘制：左上 / 右上 / 下方（功能可切） =================
void drawFuncSlotAt(int idx, int cx, int y, bool bottom) {
  int fn = faceSlotFunc[idx];
  uint16_t ucol = (idx == 0) ? slotTLColor : (idx == 1) ? slotTRColor : slotBTColor;
  uint16_t def = (fn == 0) ? C_WHITE : (fn == 1) ? C_CYAN : (fn == 2) ? C_WHITE : Z_GOLD;
  uint16_t c = slotColor(def, ucol);
  // musicA.20：左右槽放大（104x68 → 116x78，字号整体升一档），仍贴边不越圆屏
  if (bottom) zbgRect(cx - 140, y - 4, 280, 62); else zbgRect(cx - 58, y - 6, 116, 78);
  if (fn == 4) return;                             // musicA.18 隐藏（区域已擦除，让出空间）
  if (fn == 0) {                                   // 速度
    if (bottom) {
      printCentered(String(speedKmh >= 0 ? speedKmh : 0), cx, y, 5, c);
      printCentered("km/h", cx, y + 44, 2, slotColor(0x8C73, ucol));
    } else {
      printCentered(String(speedKmh >= 0 ? speedKmh : 0), cx, y + 8, 4, c);
      printCentered("km/h", cx, y + 48, 2, slotColor(0x8C73, ucol));
    }
  } else if (fn == 1) {                            // 实时 G 值（musicA.21：不再是会话峰值）
    float gv = liveG();
    if (bottom) {
      printCentered("LIVE G", cx, y, 2, slotColor(0x8C73, ucol));
      printCentered(String(gv, 2), cx, y + 20, 5, c);
    } else {
      printCentered("LIVE G", cx, y + 2, 2, slotColor(0x8C73, ucol));
      printCentered(String(gv, 2), cx, y + 22, 4, c);
    }
  } else if (fn == 2) {                            // 胎压（前/后双值）
    char l1[24], l2[24];
    if (tpValid) {
      snprintf(l1, sizeof(l1), "F %d", tpFrontKpa);
      snprintf(l2, sizeof(l2), "R %d", tpRearKpa);
    } else {
      snprintf(l1, sizeof(l1), "F --");
      snprintf(l2, sizeof(l2), "R --");
    }
    if (bottom) {
      if (tpValid) snprintf(l1, sizeof(l1), "F %d / R %d", tpFrontKpa, tpRearKpa);
      else snprintf(l1, sizeof(l1), "F -- / R --");
      printCentered(String(l1), cx, y + 4, 3, c);
      printCentered("kPa", cx, y + 32, 2, slotColor(0x8C73, ucol));
    } else {
      printCentered(String(l1), cx, y + 6, 3, c);
      printCentered(String(l2), cx, y + 32, 3, c);
      printCentered("kPa", cx, y + 58, 2, slotColor(0x8C73, ucol));
    }
  } else if (bottom) {                             // 天气（下方槽：原待机天气区内容）
    if (wxHave || imgs[3].showW) {
      drawTextImgC(3, y, 0xFF, c);                 // "地点 · 天气现象" 中文位图
      String t = String(wxTemp);
      int tw = t.length() * 24, icw = 28, gap = 10;
      int x0 = cx - (icw + gap + tw + 8) / 2;
      drawWxIcon(wxIcon, x0 + icw / 2, y + 42, 14);
      printCentered(t, x0 + icw + gap + tw / 2, y + 30, 4, c);
      aaDisc(x0 + icw + gap + tw + 3, y + 35, 4, c);
    } else {
      printCentered("--", cx, y + 10, 4, slotColor(0x4208, ucol));
    }
  } else {                                         // 天气（左右槽：温度大字在上 + 图标在下，竖排防溢出）
    if (wxHave) {
      String t = String(wxTemp);
      printCentered(t, cx, y + 8, 4, c);           // 温度 size 4（32px 字高）
      aaDisc(cx + t.length() * 16 + 5, y + 20, 4, c);   // 度数圈
      drawWxIcon(wxIcon, cx, y + 54, 12);          // 天气图标（半径 12）
    } else {
      printCentered("--", cx, y + 16, 4, slotColor(0x4208, ucol));
    }
  }
}

void drawFuncSlots() {                             // musicA.17 六槽之三：左上/右上/下方
  drawFuncSlotAt(0, SLOT_LX, SLOT_Y, false);
  drawFuncSlotAt(1, SLOT_RX, SLOT_Y, false);
  drawFuncSlotAt(2, CX, SLOTB_Y, true);
}


// 手机在 BLE 连上时和每小时发一次 "#TIME,yyyy-MM-dd HH:mm:ss"（本地时间）；
// 圆屏用 millis() 走秒，跨天自动翻日期。没对过时就不显示时钟。
extern bool wxHourly, wxHave;
extern int wxIcon, wxTemp, wxN;
extern int wxFh[6], wxFl[6];
void drawWxIcon(int id, float cx, float cy, float s);       // 定义在 WeatherPage.h（后包含，前置声明）

int clkY = 2026, clkMo = 1, clkD = 1;      // 对时那一刻的日期
int clkDaySec = 0;                         // 对时那一刻的当天秒数
unsigned long clkSyncMs = 0;               // 对时时的 millis()；0 = 还没对过时

bool clkLeap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

// 当前时刻：y/m/d/h/m/s（v7.12.9-ui.4 秒针环需要秒）
void clkNow(int &Y, int &Mo, int &D, int &H, int &Mi, int &S) {
  static const int mdays[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
  unsigned long el = (clkSyncMs ? (millis() - clkSyncMs) / 1000 : 0) + clkDaySec;
  long day = el / 86400L;
  int sec = el % 86400L;
  Y = clkY; Mo = clkMo; D = clkD;
  while (day-- > 0) {
    D++;
    int dm = mdays[Mo - 1] + (Mo == 2 && clkLeap(Y) ? 1 : 0);
    if (D > dm) { D = 1; Mo++; if (Mo > 12) { Mo = 1; Y++; } }
  }
  H = sec / 3600; Mi = (sec % 3600) / 60; S = sec % 60;
}

// v7.12.9-ui.4 没对过时：灰色 --:-- 占位（大时钟同款 size 10 字距，每格 60px）
// musicA.17：时钟上移（60..130），给放大的中央动画带让位
void drawClockPlaceholder() {
  const int x0 = CX - 150, top = 60, cell = 60, u = 10;   // 5 格，每格 6u 宽、字高 7u
  for (int k = 0; k < 5; k++) {
    if (k == 2) {                                          // 冒号：上下两个点
      gfx->fillRect(x0 + k * cell + 2 * u, top + 2 * u, u, u, C_GREY);
      gfx->fillRect(x0 + k * cell + 2 * u, top + 5 * u, u, u, C_GREY);
    } else {                                               // 横杠：中间一行
      gfx->fillRect(x0 + k * cell + u, top + 3 * u, 5 * u, u, C_GREY);
    }
  }
}

void drawIdleClock() {
  if (!(slotMask & 0x01)) return;                          // 时间槽关闭则不画日期/时钟，露出背景
  // musicA.40：日期行略下移，避开加大电量底边
  zbgRect(CX - 82, 42, 164, 22);                           // 日期行
  zbgRect(CX - 156, 62, 312, 72);                          // 大时钟
  if (!clkSyncMs) {                                        // v7.12.9-ui.4 未对时占位，不再空白
    drawClockPlaceholder();
    needFlush = true;
    return;
  }
  int Y, Mo, D, H, Mi, S;
  clkNow(Y, Mo, D, H, Mi, S);
  char db[14], tb[6];
  snprintf(db, sizeof(db), "%04d/%02d/%02d", Y, Mo, D);
  snprintf(tb, sizeof(tb), "%02d:%02d", H, Mi);
  printCentered(String(db), CX, 44, 2, slotColor(Z_GOLD_SUB, slotTimeColor));
  printCentered(String(tb), CX, 64, 10, slotColor(Z_GOLD, slotTimeColor));
  needFlush = true;                                        // v7.13.1 别依赖骑手动画兜底刷屏
}

// ================= v7.12.9-ui.4 外圈像素秒针刻度环 =================
// 60 格 × 6°，r=222（对齐固件进度环半径，距屏边 11px）：主刻度×4（12/3/6/9 点位）5px 亮灰、
// 小时刻度×12 4px 中灰、秒刻度×44 3px 暗灰；红色像素圆点每秒离散跳一格。
// 未对时（clkSyncMs==0）不画环——没有秒数据。与待机时钟同用 millis() 时间基准。
#define SEC_RING_R 222
#define C_RING_S   0x144B                                       // 秒刻度 暗灰 #4a4a58
#define C_RING_H   0x8C73                                       // 小时刻度 中灰 #8e8e9c
#define C_RING_Q   0xD69B                                       // 主刻度 亮灰 #d0d0da
#define C_RING_RED 0xFA27                                       // 秒针红点 #FF4438
uint8_t ringShownSec = 255;                                     // 当前红点位置；255 = 还没画

void ringXY(int deg, int &x, int &y) {
  float a = (deg - 90) * 0.01745329f;                           // 0 秒在 12 点位
  x = CX + (int)lroundf(SEC_RING_R * cosf(a));
  y = CX + (int)lroundf(SEC_RING_R * sinf(a));  // 圆屏 y 圆心 = CX（同 ProgressRings 约定）
}

void ringTickSize(int idx, int &sz, uint16_t &c) {
  if (idx % 15 == 0)      { sz = 5; c = C_RING_Q; }
  else if (idx % 5 == 0)  { sz = 4; c = C_RING_H; }
  else                    { sz = 3; c = C_RING_S; }
}

// 画/擦一个刻度位（erase=true 时把红点位置还原成刻度）
void ringDotAt(int idx, bool erase) {
  int x, y, sz; uint16_t c;
  ringXY(idx * 6, x, y); ringTickSize(idx, sz, c);
  if (erase) {
    zbgRect(x - 4, y - 4, 9, 9);                              // 塞尔达40周年特别版：恢复纹理而不是擦黑
    gfx->fillRect(x - sz / 2, y - sz / 2, sz, sz, c);         // 再还原该位刻度
    return;
  }
  gfx->fillRect(x - 4, y - 4, 9, 9, C_RING_RED);                // 9x9 四角削 1px 的像素圆
  gfx->fillRect(x - 4, y - 4, 1, 1, C_BLACK);
  gfx->fillRect(x + 4, y - 4, 1, 1, C_BLACK);
  gfx->fillRect(x - 4, y + 4, 1, 1, C_BLACK);
  gfx->fillRect(x + 4, y + 4, 1, 1, C_BLACK);
}

// 整环重画（页面重画/每拍补画）
// musicA.37：刻度与红点分离。drawRingTicks() 在 ClockStyles.h，由 hasTimeFace() 门控
// （上方槽=时间 / 中央槽=时间 / 全屏表盘 任一成立即画 60 格静态刻度）；
// 红点仍严格由 dotOn() 管（= slotMask bit0，App 由 top==0 推导），一秒跳一格。
void drawSecondRing(int sec) {
  drawRingTicks();
  if (dotOn() && clkSyncMs) { ringDotAt(sec, false); ringShownSec = (uint8_t)sec; }
  else ringShownSec = 255;
  needFlush = true;
}

// 每秒推进：只擦旧点、画新点（省总线）；没有红点权限时直接不维护状态
void tickSecondRing(int sec) {
  if (!dotOn() || !clkSyncMs) { ringShownSec = 255; return; }
  if (ringShownSec > 59 || sec == ringShownSec) return;
  ringDotAt(ringShownSec, true);
  ringDotAt(sec, false);
  ringShownSec = (uint8_t)sec;
  needFlush = true;
}

// 每 0.1 秒检查一次，样子变了才重画（函数名沿用 v3.6）
// musicA.17：中央动画按 vehId 调度（0 塞尔达 1 像素摩托 2 自行车），
// 之后依次重画连接态槽（y420）、时钟、秒环、三个功能槽——场景带擦除会盖住它们，每拍补画。
void animateCat() {
  if (!onWaitScreen || sliding) return;
  static unsigned long last = 0;
  unsigned long now = millis();
  if (!catForce && now - last < 100) return;
  bool force = catForce;
  catForce = false;
  last = now;
  bool online = (waitSt == WAIT_READY);

  // musicA.37 预设动态背景：每拍渲染一帧到 dynBgBuf 并整屏恢复，
  // 后面所有槽位 / 刻度 / 连接态都叠在这帧上（上传自定义壁纸时 dynBgActive() 为假）。
  if (dynBgActive()) {
    dynBgRenderFrame(now);
    zbgRect(0, 0, 466, 466);
    needFlush = true;
  }

  // musicA.37 全屏指针表盘：整屏重画后直接短路返回。
  // 不短路的话下面每拍 zbgRect(0,BAND_Y0,466,BAND_H) 会把表盘下半部擦掉。
  if (centerFunc == 2 && !inNav) {   // onWaitScreen 已隐含 page==0
    // 静态背景下只需秒针跳格时重画（1fps）；开了动态背景才跟着每拍刷新（10fps）。
    static int fullFaceLastSec = -1;
    int sec = -1;
    if (clkSyncMs) { int Y, Mo, D, H, Mi; clkNow(Y, Mo, D, H, Mi, sec); }
    if (dynBgActive() || force || sec != fullFaceLastSec) {
      tickFullFace();
      fullFaceLastSec = sec;
    }
    (void)force;
    return;
  }

  // 中央槽：0 中央动画（原逻辑） / 1 九种时间样式。bit3 仍是「中央槽总显隐」。
  // musicA.43：中央=时间改到下方槽/秒环之后再画，避免事后 wipe 与环刻度压裁切。
  const bool centerTime = (centerFunc == 1);
  // musicA.49-ui.1：放大后的 GTA 会越过原场景带，先扩大范围清残影；动画放到本函数末尾置顶绘制。
  const bool gtaTop = (vehId == 3) && (slotMask & 0x08) && !centerTime && (faceId <= 1)
                      && (online || waitSt == WAIT_NOLINK);
  if (gtaTop) zbgRect(0, 100, 466, 300);                    // 槽/时钟/状态随后各自重画
  if (!(slotMask & 0x08)) {
    zbgRect(0, BAND_Y0, 466, BAND_H);
    if (faceId > 1) zbgRect(16, 110, 434, 200);
  } else if (!centerTime && faceId <= 1) {                   // 塞尔达纹理表盘（默认 / 塞尔达绿）
    if (online || waitSt == WAIT_NOLINK) {
      if (vehId == 0) {
        if (online) { drawZeldaOnline(now); needFlush = true; }
        else { drawZeldaOffline(now); needFlush = true; }
      } else if (vehId == 3) {                               // musicA.49-ui.1：改到末尾置顶绘制
        needFlush = true;
      } else {
        zbgRect(0, BAND_Y0, 466, BAND_H);                    // 载具场景带整条恢复
        if (vehId == 1) drawRiderPixel(now, online);
        else drawBikeRider(now, online);
        needFlush = true;
      }
    }                                                        // WAIT_LOST：场景静态，载具不重画
  } else if (!centerTime) {                                  // 程序化表盘中央主视觉
    drawFaceMotif(now, waitSt);
  }
  // 连接态槽（musicA.17：移到底部边缘 y420，颜色/显隐 App 可调；WAIT_LOST 时让位给 LINK LOST）
  if ((slotMask & 0x04) && waitSt != WAIT_LOST) {
    zbgRect(CX - 60, CONN_Y - 4, 120, 24);
    if (online) printCentered("ready", CX, CONN_Y, 2, slotColor(C_GREEN, slotConnColor));
    else printCentered("no link", CX, CONN_Y, 2, slotColor(C_GREY, slotConnColor));
  }
  // musicA.41：中央=时间 → 圆屏不画上方/左上/右上，下方保留；其他模式照旧。
  // musicA.43：省略事后 top/TL/TR wipe（会裁切放大中央时间）；drawCenterClock 自清 y48…348。
  if (clkSyncMs) {
    int Y, Mo, D, H, Mi, S;
    clkNow(Y, Mo, D, H, Mi, S);
    (void)Y; (void)Mo; (void)D; (void)H; (void)Mi;
    if (!centerTime) drawTopSlot();                          // 时间 / 速度 / 实时G / 胎压 / 天气 / 隐藏
    drawSecondRing(S);                                       // 刻度(hasTimeFace) + 红点(dotOn)
  } else if (!centerTime) {
    drawTopSlot();                                           // --:-- 占位；对时后立刻有字
  }
  if (centerTime) {
    drawFuncSlotAt(2, CX, SLOTB_Y, true);                    // 只画下方；不擦 TL/TR
    if (slotMask & 0x08) drawCenterClock();                  // bit3 开才画；最后叠，无边裁
  } else {
    drawFuncSlots();                                         // musicA.17 三个功能槽
  }
  drawStatus();                                              // musicA.40 P0：dynBg 整屏擦除后重画电量/蓝牙
  // musicA.49：下方/左右槽擦除带会吃掉 SEC_RING 斜角刻度；中央时钟 wipe 亦然。
  // 槽与状态画完后统一补画外圈秒环（覆盖 center=时间 与 中央=动画+上方=时间）。
  if (clkSyncMs && hasTimeFace()) {
    int Y3, Mo3, D3, H3, Mi3, S3;
    clkNow(Y3, Mo3, D3, H3, Mi3, S3);
    (void)Y3; (void)Mo3; (void)D3; (void)H3; (void)Mi3;
    drawSecondRing(S3);
  }
  if (gtaTop) {                                              // musicA.49-ui.1：最后叠，覆盖槽一点点也没关系
    drawGtaCenter(now, online);
    needFlush = true;
  }
  (void)force;
}

// 页面：导航左滑天气，右滑音乐；上划 G 值速度页，下划胎压页（v7.12.9-ui.6，原来上下滑都是天气）
int page = 0;                               // 0 = 导航 / 首页，1 = 天气，2 = 音乐，3 = G 值速度，4 = 胎压
unsigned long pageAt = 0;

// 页面顺序对应滑动位置：音乐在左，主页居中，天气在右；G/胎压在上下（页点行用左右位表示，导航主页不显示）。
void drawPageDots() {
  if (page == 0 && inNav) return;
  const int pageOrder[] = { (page == 3 || page == 4) ? 4 : 2, 0, (page == 3 || page == 4) ? 3 : 1 };
  for (int i = 0; i < 3; i++) {
    aaDisc(CX + (i - 1) * 20, 444, 4, page == pageOrder[i] ? C_WHITE : 0x4208);
  }
}

void drawWaiting(int st) {
  waitSt = st;
  if (sliding) return;                                                 // v5.5 正在滑动：滑完再画
  if (page == 2) { onWaitScreen = false; musicDirty = true; return; }
  if (page == 1) { onWaitScreen = false; wxDirty = true; return; }   // v5.3 正在看天气：先不画首页，滑回来时再画
  if (page == 3 || page == 4) { onWaitScreen = false; return; }      // v7.12.9-ui.6 G/胎压页由 gTpmsTick 重画
  if (dynBgActive()) dynBgRenderFrame(millis());                           // musicA.37 整页重画前先出一帧背景
  zbgRect(0, 0, 466, 466);                                           // 塞尔达40周年特别版：纹理背景代替黑屏（musicA.17：含自定义开机背景）
  onWaitScreen = true; waitSt = st;          // v3.6 首页猫猫 → v4.6 像素骑手
  riderPrevF = -1; riderPrevZ = -2;
  catForce = true;
  // musicA.43：中央=时间时跳过上方槽首画，避免重建闪一下再被擦掉
  if (centerFunc != 1) drawTopSlot();        // musicA.37 上方槽（时间/速度/实时G/胎压/天气/隐藏）
  if (clkSyncMs) {                           // v7.12.9-ui.4 整环重画（页面被清屏过）
    int Y, Mo, D, H, Mi, S;
    clkNow(Y, Mo, D, H, Mi, S);
    drawSecondRing(S);
  }
  animateCat();                              // musicA.17 场景带 + 连接态槽 + 三个功能槽都在这里画（末尾含 drawStatus）
  if (st == WAIT_LOST) {
    drawRedRing();
    printCentered("LINK LOST", CX, CONN_Y, 3, C_RED);
  }
  // musicA.41：全屏指针已由 tickFullFace 画轴心上方电量；勿再叠顶栏电量
  if (centerFunc != 2) drawStatus();
  drawPageDots();
  needFlush = true;
}

// ================= 待机页可换表盘：背景渲染与切换 =================
// 程序化表盘用 gfx 原语画到帧缓冲后快照进 homeBgBuf（避免给 zbgRect 另写一套缓冲绘制 API）。

// v7.12.10 二期C 新增：模拟指针时钟（加州/光影/寻路共用）。中心 (cx,cy)，指针最大半径 r。
void drawAnalogHands(int cx, int cy, int r, int H, int Mi, int S, uint16_t hc, uint16_t mc, uint16_t sc) {
  float ah = ((H % 12) + Mi / 60.0f) * 0.52359878f - 1.5707963f;   // 30°/时
  float am = (Mi + S / 60.0f) * 0.10471976f - 1.5707963f;           // 6°/分
  float as = S * 0.10471976f - 1.5707963f;                          // 6°/秒
  gfx->drawLine(cx, cy, cx + (int)(r * 0.52f * cosf(ah)), cy + (int)(r * 0.52f * sinf(ah)), hc);
  gfx->drawLine(cx, cy, cx + (int)(r * 0.80f * cosf(am)), cy + (int)(r * 0.80f * sinf(am)), mc);
  gfx->drawLine(cx, cy, cx + (int)(r * 0.92f * cosf(as)), cy + (int)(r * 0.92f * sinf(as)), sc);
  aaDisc(cx, cy, 5, hc);
}

void renderFaceBg(int id) {
  if (!homeBgBuf) homeBgBuf = (uint16_t *)ps_malloc(466 * 466 * 2);
  uint16_t *fb = gfx->getFramebuffer();
  if (!homeBgBuf || !fb) { homeBg = nullptr; return; }
  uint16_t base = 0x0000;
  switch (id) {
    case 2: base = 0x0000; break;            // 极简黑
    case 3: base = 0x0006; break;            // 赛博霓虹（近黑偏蓝）
    case 4: base = 0x0010; break;            // 机械（深蓝灰）
    case 5: base = 0x1804; break;            // 运动橙（深棕黑）
    case 6: base = 0x0000; break;            // 都市：黑底
    case 7: base = 0xF6F5; break;            // 加州：米色
    case 8: base = 0x0000; break;            // 天文：黑底
    case 9: base = 0x2945; break;            // 光影：钢灰
    case 10: base = 0x0000; break;           // 寻路：黑底
    default: base = 0x0000;
  }
  gfx->fillScreen(base);
  int cx = CX, cy = CX;
  if (id == 6 || id == 7 || id == 8 || id == 9 || id == 10) cy = 210;   // 新增表盘：中心略上移，给顶部数字留位
  if (id == 2) {                              // 极简黑：仅一圈细灰环
    aaDisc(cx, cy, 150, 0x6B4D); aaDisc(cx, cy, 146, base);
    aaDisc(cx, cy, 6, 0x6B4D);
  } else if (id == 3) {                       // 赛博霓虹：放射线 + 同心霓虹环
    for (int a = 0; a < 360; a += 15) {
      float r = a * 0.01745329f;
      gfx->drawLine(cx + (int)(60 * cosf(r)), cy + (int)(60 * sinf(r)),
                    cx + (int)(200 * cosf(r)), cy + (int)(200 * sinf(r)), 0x0142);
    }
    aaDisc(cx, cy, 120, 0x07FF); aaDisc(cx, cy, 114, base);
    aaDisc(cx, cy, 90, 0xF81F);  aaDisc(cx, cy, 84, base);
    aaDisc(cx, cy, 60, 0x07FF);  aaDisc(cx, cy, 56, base);
  } else if (id == 4) {                       // 机械：表盘刻度环 + 内圈
    aaDisc(cx, cy, 140, 0x8C73); aaDisc(cx, cy, 136, base);
    for (int k = 0; k < 60; k++) {
      float a = k * 6 * 0.01745329f;
      int r0 = 120, r1 = (k % 5 == 0) ? 108 : 114;
      gfx->drawLine(cx + (int)(r0 * cosf(a)), cy + (int)(r0 * sinf(a)),
                    cx + (int)(r1 * cosf(a)), cy + (int)(r1 * sinf(a)), 0x8C73);
    }
    aaDisc(cx, cy, 100, 0x2945); aaDisc(cx, cy, 96, base);
  } else if (id == 5) {                       // 运动橙：速度表弧段
    aaDisc(cx, cy, 140, 0xFBC3); aaDisc(cx, cy, 135, base);
    for (int k = 0; k <= 50; k++) {
      float a = (-140 + k * 5.6) * 0.01745329f;
      int x = cx + (int)(120 * cosf(a)), y = cy + (int)(120 * sinf(a));
      gfx->fillRect(x - 2, y - 2, 4, 4, 0xFBC3);
    }
    aaDisc(cx, cy, 90, 0xF623); aaDisc(cx, cy, 86, base);
  } else if (id == 6) {                       // 都市（Metropolitan）：粉→蓝渐变细环，极简现代
    uint16_t grc[3] = {0xF81F, 0x89F9, 0x07FF};
    int gr0[3] = {150, 128, 106};
    for (int k = 0; k < 3; k++) { aaDisc(cx, cy, gr0[k], grc[k]); aaDisc(cx, cy, gr0[k] - 3, base); }
    aaDisc(cx, cy, 6, 0xF81F);
  } else if (id == 7) {                        // 加州（California）：米色经典表盘 + 刻度
    aaDisc(cx, cy, 188, 0x6B4D); aaDisc(cx, cy, 184, base);
    for (int k = 0; k < 12; k++) {
      float a = k * 30 * 0.01745329f;
      int r0 = (k % 3 == 0) ? 158 : 170, r1 = 180;
      gfx->drawLine(cx + (int)(r0 * cosf(a)), cy + (int)(r0 * sinf(a)),
                    cx + (int)(r1 * cosf(a)), cy + (int)(r1 * sinf(a)), 0x6B4D);
    }
    aaDisc(cx, cy, 150, base);
  } else if (id == 8) {                        // 天文（Astronomy）：星空 + 地球渐变球
    uint32_t seed = 0x9E3779B1u;
    for (int i = 0; i < 80; i++) {
      seed = seed * 1103515245u + 12345u;
      int sx = (int)(seed & 0x1FF) % 466, sy = (int)((seed >> 9) & 0x1FF) % 466;
      int dx = sx - cx, dy = sy - cy;
      long d2 = (long)dx * dx + (long)dy * dy;
      if (d2 > 128 * 128 && d2 < 232 * 232) gfx->fillRect(sx, sy, 2, 2, 0x7BEF);
    }
    aaDisc(cx, cy, 120, 0x0410); aaDisc(cx, cy, 96, 0x0451);
    aaDisc(cx, cy, 72, 0x06E3);  aaDisc(cx, cy, 48, 0x07E0); aaDisc(cx, cy, 24, 0x9FE3);
  } else if (id == 9) {                        // 光影（Reflections）：金属放射光纹 + 刻度
    for (int a = 0; a < 360; a += 6) {
      float r = a * 0.01745329f;
      uint16_t c = (a % 30 == 0) ? 0xCE79 : 0x5A6D;
      gfx->drawLine(cx, cy, cx + (int)(190 * cosf(r)), cy + (int)(190 * sinf(r)), c);
    }
    aaDisc(cx, cy, 150, 0x2945); aaDisc(cx, cy, 146, base);
    for (int k = 0; k < 12; k++) {
      float a = k * 30 * 0.01745329f;
      int r0 = (k % 3 == 0) ? 120 : 132, r1 = 142;
      gfx->drawLine(cx + (int)(r0 * cosf(a)), cy + (int)(r0 * sinf(a)),
                    cx + (int)(r1 * cosf(a)), cy + (int)(r1 * sinf(a)), 0xC618);
    }
  } else if (id == 10) {                       // 寻路（Wayfinder）：指南针外环 + 方位刻度
    aaDisc(cx, cy, 190, 0x4228); aaDisc(cx, cy, 184, base);
    for (int k = 0; k < 36; k++) {
      float a = k * 10 * 0.01745329f;
      int r0 = (k % 9 == 0) ? 160 : 174, r1 = 182;
      uint16_t c = (k % 9 == 0) ? 0xFBC3 : 0x6B4D;
      gfx->drawLine(cx + (int)(r0 * cosf(a)), cy + (int)(r0 * sinf(a)),
                    cx + (int)(r1 * cosf(a)), cy + (int)(r1 * sinf(a)), c);
    }
  }
  memcpy(homeBgBuf, fb, 466 * 466 * 2);
  homeBg = homeBgBuf;
}

// musicA.19：默认背景透明黑。applyFace 兼容旧调用，恒定透明（不再渲染程序化主题）。
void applyFace(int id) {
  (void)id;
  homeBg = nullptr;                              // 透明黑；homeBgBuf 不再分配
}

// 非塞尔达表盘的中央主视觉。每拍先恢复背景再画，和塞尔达场景同生命周期。
void drawFaceMotif(unsigned long now, int st) {
  (void)st;
  zbgRect(0, BAND_Y0, 466, BAND_H);
  int cx = CX, cy = 246;
  int id = faceId;
  if (id == 2) {                                 // 极简黑：静止细环 + 中心点
    aaDisc(cx, cy, 70, 0x6B4D); aaDisc(cx, cy, 66, 0x0000);
    aaDisc(cx, cy, 6, 0x6B4D);
  } else if (id == 3) {                          // 赛博霓虹：旋转扫描线
    float a = (now % 4000) / 4000.0f * 6.2831853f;
    gfx->drawLine(cx, cy, cx + (int)(110 * cosf(a)), cy + (int)(110 * sinf(a)), 0x07FF);
    aaDisc(cx, cy, 8, 0xF81F);
  } else if (id == 4) {                          // 机械：秒针式指针
    float a = (now % 60000) / 60000.0f * 6.2831853f;
    gfx->drawLine(cx, cy, cx + (int)(100 * cosf(a)), cy + (int)(100 * sinf(a)), 0x8C73);
    aaDisc(cx, cy, 6, 0xD69B);
  } else if (id == 5) {                          // 运动橙：脉冲扩散环
    float p = (now % 1500) / 1500.0f;
    aaDisc(cx, cy, (int)(40 + p * 70), 0xFBC3);
    aaDisc(cx, cy, (int)(40 + p * 70) - 4, 0x1804);
  } else if (id == 6) {                         // 都市：呼吸光晕（半径限制在 cleared band 内，避免拖影）
    float p = (now % 3000) / 3000.0f;
    uint16_t c = BootAnimation::zeldaScale(0x07FF, (uint32_t)(90 + 130 * (0.5f + 0.5f * sinf(p * 6.2831853f))));
    int rr = 95 + (int)(4 * sinf(p * 6.2831853f));
    aaDisc(cx, cy, rr, c); aaDisc(cx, cy, rr - 3, 0x0000);
  } else if (id == 7) {                         // 加州：经典模拟指针
    if (clkSyncMs) { int Y, Mo, D, H, Mi, S; clkNow(Y, Mo, D, H, Mi, S); drawAnalogHands(cx, cy, 95, H, Mi, S, 0x2945, 0x2945, 0xF800); }
  } else if (id == 8) {                         // 天文：月球绕地（半径限制在 cleared band 内，避免拖影）
    float a = (now % 12000) / 12000.0f * 6.2831853f;
    int mx = cx + (int)(90 * cosf(a)), my = cy + (int)(90 * sinf(a));
    aaDisc(mx, my, 9, 0xEFBF);
  } else if (id == 9) {                         // 光影：金属模拟指针
    if (clkSyncMs) { int Y, Mo, D, H, Mi, S; clkNow(Y, Mo, D, H, Mi, S); drawAnalogHands(cx, cy, 95, H, Mi, S, 0xC618, 0xC618, 0xF81F); }
  } else if (id == 10) {                        // 寻路：指针 + 指北针
    if (clkSyncMs) {
      int Y, Mo, D, H, Mi, S; clkNow(Y, Mo, D, H, Mi, S);
      drawAnalogHands(cx, cy, 95, H, Mi, S, 0xFBC3, 0xFBC3, 0xF800);
    }
    gfx->drawLine(cx, cy, cx + (int)(62 * cosf(-1.5707963f)), cy + (int)(62 * sinf(-1.5707963f)), 0xF800);   // 指北（红）
    gfx->drawLine(cx, cy, cx + (int)(62 * cosf(1.5707963f)),  cy + (int)(62 * sinf(1.5707963f)),  0xFFFF);   // 指南（白）
  }
}

// ================= musicA.17：旧「气压占位槽」已并入下方功能槽（drawFuncSlotAt fn==2） =================
