#pragma once
// musicA.26 路口放大图（bulk type 9）。由 MotoNav.ino 按依赖顺序包含，仅编译一次。
// App 侧把高德 driveManager:showCrossImage: 给的位图深色化、缩到 272×174（25:16 原比例），
// 量化成 16 色索引图，用和栅格地图同一套 nibble RLE 压过来。固件只负责解码 + 贴图。
//
// musicA.28 B 方案：整页接管（A 方案）在圆屏上会切掉导航上下文、上下大片黑边，
// 改成「地图照常 + 上方浮窗」——小地图、转向牌、行程行、车头全都还在，路口图只占上半。
//
// 包格式（小端）：
//   'X'(0x58) | ver=1 | u16 w | u16 h | 16 × u16 RGB565 | RLE
//   RLE：每字节 低 4 位 = 颜色号，高 4 位 n。n < 15 → 连续 n+1 个；n = 15 → 连续 16+下一字节 个。
//
// 为什么用索引色而不是 RGB565：整屏直接传 RGB565 是 180KB，索引色量化后实测 6~12KB，
// 圆屏在路口前 300 米才开始要这张图，1 秒内能到位。

// 有图、开关打开、正在导航、且没过期 → 这一帧叠路口图浮窗
bool crossReady() {
  if (!crossOn || !crossIdx || !crossEnabled || !inNav) return false;
  if (crossAt && millis() - crossAt > CROSS_HOLD_MS) { crossOn = false; return false; }
  return true;
}

// 解包 + RLE 展开。失败不动旧图（宁可继续显示上一张，也不要闪黑）
bool parseCross(const uint8_t *b, uint32_t n) {
  if (!crossIdx) { Serial.println("[CROSS] parse: crossIdx 未分配"); return false; }
  if (n < CROSS_HDR) { Serial.printf("[CROSS] parse: 包太短 %u\n", (unsigned)n); return false; }
  // 刚通过路口（#CXH）后到齐的迟到分片：App 侧也做了取消，这里是兜底
  if (crossHideAt && millis() - crossHideAt < CROSS_MUTE_MS) {
    Serial.printf("[CROSS] 已过路口，丢弃迟到整包 %uB\n", (unsigned)n);
    return false;
  }
  if (b[0] != 0x58 || b[1] != 1) { Serial.printf("[CROSS] parse: 头不对 %02X %02X\n", b[0], b[1]); return false; }
  int w = (int)rdu16(b + 2), h = (int)rdu16(b + 4);
  if (w != CROSS_W || h != CROSS_H) { Serial.printf("[CROSS] parse: 尺寸 %dx%d != %dx%d\n", w, h, CROSS_W, CROSS_H); return false; }
  for (int i = 0; i < 16; i++) crossPal[i] = rdu16(b + 6 + i * 2);
  uint32_t p = CROSS_HDR;
  int k = 0;
  const int totalPx = CROSS_W * CROSS_H;
  while (p < n && k < totalPx) {
    uint8_t v = b[p++];
    int run = (v >> 4) + 1;
    if ((v >> 4) == 15) { if (p >= n) return false; run = 16 + (int)b[p++]; }
    if (k + run > totalPx) { Serial.printf("[CROSS] parse: RLE 溢出 k=%d run=%d\n", k, run); return false; }
    uint8_t c = v & 15;
    memset(crossIdx + k, c, run);
    k += run;
  }
  if (k != totalPx) { Serial.printf("[CROSS] parse: 像素不足 %d/%d\n", k, totalPx); return false; }
  crossOn = true;
  crossAt = millis();
  Serial.printf("[CROSS] parse ok %dx%d %uB pal0=%04X pal9=%04X\n",
                CROSS_W, CROSS_H, (unsigned)n, crossPal[0], crossPal[9]);
  return true;
}

// 圆角矩形贴图：逐行算两侧圆角内缩，直接写 framebuffer（比逐点判断快）
void crossBlit() {
  uint16_t *fb = gfx->getFramebuffer();
  if (!fb || !crossIdx) return;
  const int R = CROSS_CORNER;
  for (int j = 0; j < CROSS_H; j++) {
    int y = CROSS_Y + j;
    if (y < 0 || y >= LCD_HEIGHT) continue;
    int inset = 0;
    if (j < R) {
      int dy = R - j;
      inset = R - (int)sqrtf((float)(R * R - dy * dy));
    } else if (j >= CROSS_H - R) {
      int dy = j - (CROSS_H - R - 1);
      inset = R - (int)sqrtf((float)(R * R - dy * dy));
    }
    int x0 = CROSS_X + inset, x1 = CROSS_X + CROSS_W - inset;
    if (x0 < 0) x0 = 0;
    if (x1 > LCD_WIDTH) x1 = LCD_WIDTH;
    if (x1 <= x0) continue;
    uint16_t *row = fb + (size_t)y * LCD_WIDTH;
    const uint8_t *src = crossIdx + j * CROSS_W + inset;
    for (int x = x0; x < x1; x++) row[x] = crossPal[*src++];
  }
}

// 浮窗底板：投影 + 深色垫边 + 亮描边，让图从地图上「浮」起来
void crossBlitFrame() {
  gfx->fillRoundRect(CROSS_X - 4, CROSS_Y + 2, CROSS_W + 8, CROSS_H + 8, CROSS_CORNER + 4, 0x1082);
  gfx->fillRoundRect(CROSS_X - 2, CROSS_Y - 1, CROSS_W + 4, CROSS_H + 5, CROSS_CORNER + 2, 0x18C3);
  crossBlit();
  gfx->drawRoundRect(CROSS_X - 1, CROSS_Y - 1, CROSS_W + 2, CROSS_H + 2, CROSS_CORNER, 0x52AA);
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
// 车速、km/h、车道、转向牌、行程行全部沿用地图页布局（车道条在图里已有，由 MapHud 关掉）。
void drawCrossOverlay() {
  static unsigned long loggedAt = 0;
  if (loggedAt != crossAt) { loggedAt = crossAt; Serial.printf("[CROSS] overlay on (crossAt=%lu)\n", crossAt); }
  crossBlitFrame();
  drawCrossCamBadge();
}
