#pragma once
// musicA.26 路口放大图（bulk type 9）。由 MotoNav.ino 按依赖顺序包含，仅编译一次。
// App 侧把高德 driveManager:showCrossImage: 给的位图深色化、缩到 380×243（25:16 原比例），
// 量化成 16 色索引图，用和栅格地图同一套 nibble RLE 压过来。固件只负责解码 + 贴图。
//
// 包格式（小端）：
//   'X'(0x58) | ver=1 | u16 w | u16 h | 16 × u16 RGB565 | RLE
//   RLE：每字节 低 4 位 = 颜色号，高 4 位 n。n < 15 → 连续 n+1 个；n = 15 → 连续 16+下一字节 个。
//
// 为什么用索引色而不是 RGB565：380×243 直接传 RGB565 是 180KB，索引色量化后实测 12~20KB，
// 圆屏在路口前 300 米才开始要这张图，1 秒内能到位。

// 有图、开关打开、正在导航、且没过期 → 这一帧用路口图代替小地图
bool crossReady() {
  if (!crossOn || !crossIdx || !crossEnabled || !inNav) return false;
  if (crossAt && millis() - crossAt > CROSS_HOLD_MS) { crossOn = false; return false; }
  return true;
}

// 解包 + RLE 展开。失败不动旧图（宁可继续显示上一张，也不要闪黑）
bool parseCross(const uint8_t *b, uint32_t n) {
  if (!crossIdx) { Serial.println("[CROSS] parse: crossIdx 未分配"); return false; }
  if (n < CROSS_HDR) { Serial.printf("[CROSS] parse: 包太短 %u\n", (unsigned)n); return false; }
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
  gfx->drawRoundRect(CROSS_X - 1, CROSS_Y - 1, CROSS_W + 2, CROSS_H + 2, R + 1, 0x2945);
}

// 路口图页：上「车速 + 电量」，中「路口图卡片」，下「转向箭头 + 距离」，最底「行程行」
void drawCrossPage() {
  unsigned long t0 = millis();
  static unsigned long loggedAt = 0;
  if (loggedAt != crossAt) { loggedAt = crossAt; Serial.printf("[CROSS] page on (crossAt=%lu)\n", crossAt); }
  gfx->fillScreen(C_BLACK);
  crossBlit();

  // 车速（位置和地图页一致，切换时不跳）
  if (speedKmh >= 0) {
    bool over = hasCam && camLimit > 0 && speedKmh > camLimit;
    printOutlined(String(speedKmh), CX, 44, 4, over ? C_RED : TH.hud);
    printOutlined("km/h", CX, 80, 1, over ? 0xFC10 : TH.hud);
  }

  // 转向箭头 + 距离（比地图页的 near 布局稍微上移，给行程行让位）
  if (!cruiseOn && navDir != "S") {
    bool nearL = navDist <= STAGE_NEAR_M;
    uint16_t col = nearL ? C_ORANGE : C_WHITE;
    String dt = distText(navDist);
    iconHalo = true;
    drawBigIconAt(navDir, col, 382, 1.0f, CX - 58, 352, 412, 84);
    iconHalo = false;
    printOutlined(dt, CX + 54, 356, dt.length() <= 4 ? 5 : 4, col);
  } else if (cruiseOn) {
    printOutlined("CRUISE", CX, 380, 2, C_CYAN);
  }

  if (tripOn && !cruiseOn) drawTripLine(420);     // 到达时间 · 剩余里程 · 剩余灯数
  drawBattery(CX - 16, 18, true);

  needFlush = true;
  mapFrames++;
  mapDrawMsAcc += millis() - t0;
  ovlMsAcc += millis() - t0;
}
