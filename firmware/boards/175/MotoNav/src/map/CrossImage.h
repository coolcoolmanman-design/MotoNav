#pragma once
// 路口放大图（bulk type 9 = 全图 / type 10 = 预览）。由 MotoNav.ino 按依赖顺序包含，仅编译一次。
// App 侧把高德 driveManager:showCrossImage: 给的位图按昼夜配色、缩到 272×150，
// 量化成索引图，用和栅格地图同一套 nibble RLE 压过来。固件只负责解码 + 贴图。
//
// musicA.28 B 方案：整页接管（A 方案）在圆屏上会切掉导航上下文、上下大片黑边，
// 改成「地图照常 + 上方浮窗」——小地图、转向牌、行程行、车头全都还在，路口图只占上半。
//
// musicA.30 两段式：手机锁屏时 iOS 把 BLE 连接间隔拉到 ~90ms，一片一个连接事件，
// 全图十五六片就要 1.4~2.1 秒。所以 App 先发 136×75 的预览（片数只有 1/4），
// 圆屏 2 倍放大先顶上，全图到了再替换。同时调色板 16 色 → 5 色（4 级灰 + 1 彩色槽）。
//
// musicA.32 昼夜成套：固件本来有 THEMES / THEMES_NIGHT + nightOn（App 按日出日落发 #n,1/0），
// 但路口图一直是「永久夜间」——App 固定灰阶反相 + 深底，固件固定深色投影/描边，
// 白天浅色地图上就贴着一块夜间浮窗。现在 App 按 paletteNight 出两套调色板，
// 固件按 nightOn 切描边色；调色板随包下发，所以协议不用为昼夜改。
//
// 包格式（小端）：
//   'X'(0x58) | ver=1(旧,16 色) 或 2(5 色) | u16 w | u16 h | 16 × u16 RGB565 | RLE
//   调色板永远占 32 字节，ver=2 时只有前 CROSS_PAL_N 个有效，其余填 0。
//   RLE：每字节 低 4 位 = 颜色号，高 4 位 n。n < 15 → 连续 n+1 个；n = 15 → 连续 16+下一字节 个。
//   尺寸决定目标缓冲：272×150 → 全图（type 9）；136×75 → 预览（type 10）。

// 有图、开关打开、正在导航、且没过期 → 这一帧叠路口图浮窗
bool crossReady() {
  // ui.6：大字转向样式不叠路口浮窗。这也是硬约束——crossReady() 会在
  // mapModeActive() 里强制返回 true，不屏蔽就会把大字页挤回地图页。
  if (themeIdx == ST_BIGTURN || themeIdx == ST_VECTOR) return false;
  if (!crossEnabled || !inNav) return false;
  if (!crossOn && !crossPrevOn) return false;
  unsigned long now = millis();
  if (crossOn && crossAt && now - crossAt > CROSS_HOLD_MS) crossOn = false;
  if (crossPrevOn && crossPrevAt && now - crossPrevAt > CROSS_HOLD_MS) crossPrevOn = false;
  return crossOn || crossPrevOn;
}

// 解包 + RLE 展开。失败不动旧图（宁可继续显示上一张，也不要闪黑）。
// 全图和预览共用这一个解析器，按包里的 w/h 决定写进哪块缓冲、用哪套调色板。
bool parseCross(const uint8_t *b, uint32_t n, int gen) {
  if (n < CROSS_HDR) { Serial.printf("[CROSS] parse: 包太短 %u\n", (unsigned)n); return false; }
  // musicA.35：静默窗只丢「被隐藏那一代」的迟到残包。
  // #CXH 时记下当前显示那张图的 gen；新 gen 一到就说明是下一个路口，立即解禁，
  // 连它随后的全图也照常收。旧版按时间无差别丢 2 秒，会把相邻第二个路口的新图一起杀掉。
  if (crossHideAt && millis() - crossHideAt < CROSS_MUTE_MS) {
    if (crossHiddenGen < 0 || gen == crossHiddenGen) {
      Serial.printf("[CROSS] 已过路口，丢弃同代迟到整包 gen=%d %uB\n", gen, (unsigned)n);
      return false;
    }
    Serial.printf("[CROSS] 新一代路口图 gen=%d（隐藏的是 %d），解除静默\n", gen, crossHiddenGen);
    crossHideAt = 0;
  }
  if (b[0] != 0x58) { Serial.printf("[CROSS] parse: magic 不对 %02X\n", b[0]); return false; }
  const int ver = b[1];
  const int w = (int)rdu16(b + 2), h = (int)rdu16(b + 4);
  const bool full = (w == CROSS_W && h == CROSS_H);
  const bool prev = (w == CROSS_PW && h == CROSS_PH);
  if (!full && !prev) {
    Serial.printf("[CROSS] parse: 尺寸 %dx%d 不认识（全图 %dx%d / 预览 %dx%d）\n",
                  w, h, CROSS_W, CROSS_H, CROSS_PW, CROSS_PH);
    return false;
  }
  if (ver != 1 && ver != CROSS_VER) {
    Serial.printf("[CROSS] parse: 协议版本 %d 不支持（要 1 或 %d）\n", ver, CROSS_VER);
    return false;
  }
  uint8_t *dst = full ? crossIdx : crossPrev;
  if (!dst) { Serial.println("[CROSS] parse: 缓冲未分配"); return false; }
  uint16_t *pal = full ? crossPal : crossPrevPal;
  const int pw = full ? CROSS_W : CROSS_PW;
  const int ph = full ? CROSS_H : CROSS_PH;
  const int totalPx = pw * ph;

  for (int i = 0; i < 16; i++) pal[i] = rdu16(b + 6 + i * 2);

  uint32_t p = CROSS_HDR;
  int k = 0;
  while (p < n && k < totalPx) {
    uint8_t v = b[p++];
    int run = (v >> 4) + 1;
    if ((v >> 4) == 15) { if (p >= n) return false; run = 16 + (int)b[p++]; }
    if (k + run > totalPx) { Serial.printf("[CROSS] parse: RLE 溢出 k=%d run=%d\n", k, run); return false; }
    uint8_t c = v & 15;
    if (c >= (ver == 1 ? 16 : CROSS_PAL_N)) c = 0;      // 越界颜色兜到底色，别去读调色板外
    memset(dst + k, c, run);
    k += run;
  }
  if (k != totalPx) { Serial.printf("[CROSS] parse: 像素不足 %d/%d\n", k, totalPx); return false; }

  lastCrossGen = gen;                 // #CXH 时据此判断「隐藏的是哪一代」
  if (full) {
    crossOn = true; crossAt = millis();
  } else {
    crossPrevOn = true; crossPrevAt = millis();
  }
  Serial.printf("[CROSS] parse ok %s %dx%d %uB ver=%d pal0=%04X pal3=%04X\n",
                full ? "full" : "preview", w, h, (unsigned)n, ver, pal[0], pal[3]);
  return true;
}

// 圆角行内缩：j 是相对浮窗顶部的行号，返回该行左右各要内缩多少像素
static inline int crossRowInset(int j) {
  const int R = CROSS_CORNER;
  if (j < R) {
    int dy = R - j;
    return R - (int)sqrtf((float)(R * R - dy * dy));
  }
  if (j >= CROSS_H - R) {
    int dy = j - (CROSS_H - R - 1);
    return R - (int)sqrtf((float)(R * R - dy * dy));
  }
  return 0;
}

// 全图：逐行算两侧圆角内缩，直接写 framebuffer（比逐点判断快）
void crossBlitFull() {
  uint16_t *fb = gfx->getFramebuffer();
  if (!fb || !crossIdx) return;
  for (int j = 0; j < CROSS_H; j++) {
    int y = CROSS_Y + j;
    if (y < 0 || y >= LCD_HEIGHT) continue;
    int inset = crossRowInset(j);
    int x0 = CROSS_X + inset, x1 = CROSS_X + CROSS_W - inset;
    if (x0 < 0) x0 = 0;
    if (x1 > LCD_WIDTH) x1 = LCD_WIDTH;
    if (x1 <= x0) continue;
    uint16_t *row = fb + (size_t)y * LCD_WIDTH;
    const uint8_t *src = crossIdx + j * CROSS_W + inset;
    for (int x = x0; x < x1; x++) row[x] = crossPal[*src++];
  }
}

// 预览：136×75 原样最近邻放大 2 倍铺满同一块浮窗区域。
// 放大不做插值——索引色插值没有意义，而且 2 倍整点复制正好一个源像素对 2×2 目标像素，
// 写起来就是把同一行写两遍、每个像素写两次，比全图还快一点。
void crossBlitPrev() {
  uint16_t *fb = gfx->getFramebuffer();
  if (!fb || !crossPrev) return;
  for (int j = 0; j < CROSS_H; j++) {
    int sy = j >> 1;
    if (sy >= CROSS_PH) sy = CROSS_PH - 1;
    int y = CROSS_Y + j;
    if (y < 0 || y >= LCD_HEIGHT) continue;
    int inset = crossRowInset(j);
    int x0 = CROSS_X + inset, x1 = CROSS_X + CROSS_W - inset;
    if (x0 < 0) x0 = 0;
    if (x1 > LCD_WIDTH) x1 = LCD_WIDTH;
    if (x1 <= x0) continue;
    uint16_t *row = fb + (size_t)y * LCD_WIDTH;
    const uint8_t *srcRow = crossPrev + sy * CROSS_PW;
    for (int x = x0; x < x1; x++) {
      int sx = (x - CROSS_X) >> 1;
      if (sx < 0) sx = 0;
      if (sx >= CROSS_PW) sx = CROSS_PW - 1;
      row[x] = crossPrevPal[srcRow[sx]];
    }
  }
}

void crossBlit() {
  if (crossOn) crossBlitFull();          // 有全图就用全图
  else if (crossPrevOn) crossBlitPrev(); // 全图还没到，先用预览顶着
}

// musicA.32 轻外框：不再叠两层投影（圆屏 + 橙环 + 矢量底图已经够重，卡片本身要退让）。
// 描边按昼夜切：夜间青绿细边配深色路口图，白天深灰细边配浅色路口图。
// 图的内容和底色由 App 随包下发的 5 色调色板决定，固件这里只负责一条边。
void crossBlitFrame() {
  crossBlit();
  const uint16_t edge = nightOn ? 0x52AA : 0x4208;
  gfx->drawRoundRect(CROSS_X - 1, CROSS_Y - 1, CROSS_W + 2, CROSS_H + 2, CROSS_CORNER, edge);
}

// 限速牌改挂在浮窗右上角：浮窗模式下顶部中央要给车速和单位让位
void drawCrossCamBadge() {
  if (!hasCam || camLimit <= 0) return;
  const int bx = CROSS_X + CROSS_W - 12, by = CROSS_Y + 12;
  bool over = speedKmh >= 0 && speedKmh > camLimit;
  aaDisc(bx, by, 24, C_BLACK);
  aaDisc(bx, by, 22, C_SIGNRED);
  aaDisc(bx, by, 17, C_WHITE);
  String lim = String(camLimit);
  int sz = lim.length() >= 3 ? 2 : 3;
  printCentered(lim, bx, by - 4 * sz, sz, C_BLACK);
  printOutlined(distText(camDist), bx, by + 28, 2, over ? 0xFC10 : C_WHITE);
}

// musicA.28 B 方案：路口图浮窗。小地图/HUD 由 drawMapFrame 正常画完，这里只叠浮窗。
// 车速、km/h、转向牌、行程行沿用地图页布局；musicA.33 起车道胶囊画在卡片下沿（此处最后调用）。
void drawCrossOverlay() {
  static unsigned long loggedAt = 0;
  // 预览和全图各自会刷新时间戳，取较大者做去重键：先打 preview、再打 full
  unsigned long key = crossPrevAt > crossAt ? crossPrevAt : crossAt;
  if (loggedAt != key) {
    loggedAt = key;
    Serial.printf("[CROSS] overlay on (%s crossAt=%lu prevAt=%lu)\n",
                  crossOn ? "full" : "preview", crossAt, crossPrevAt);
  }
  crossBlitFrame();
  drawCrossCamBadge();
  drawCrossLanes();              // musicA.33：必须最后画，压住卡片下沿和车头三角尖
}
