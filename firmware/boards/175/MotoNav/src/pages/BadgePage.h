#pragma once
// 电子吧唧页（page==3）：主屏上划进入；多图轮播；下划 / 导航·巡航强制退出。
// 由 MotoNav.ino 按依赖顺序包含，仅编译一次。
// v7.12.11-opt.12：多槽静图 + 左右滑切图 + 自动轮播点位。

static void badgeDrawDots(uint16_t *fb) {
  if (badgeCount <= 1) return;
  const int n = badgeCount;
  const int pitch = 18;
  const int r = 4;
  const int y = LCD_HEIGHT - 28;
  const int total = (n - 1) * pitch;
  const int x0 = CX - total / 2;
  for (int i = 0; i < n; i++) {
    int x = x0 + i * pitch;
    uint16_t c = (i == badgeIndex) ? 0x3A7F : 0x4208;   // 选中偏蓝 / 未选灰
    for (int dy = -r; dy <= r; dy++) {
      for (int dx = -r; dx <= r; dx++) {
        if (dx * dx + dy * dy > r * r) continue;
        int px = x + dx, py = y + dy;
        if ((unsigned)px >= (unsigned)LCD_WIDTH || (unsigned)py >= (unsigned)LCD_HEIGHT) continue;
        fb[(size_t)py * LCD_WIDTH + px] = c;
      }
    }
  }
}

void drawBadgePage() {
  gfx->fillScreen(C_BLACK);
  onWaitScreen = false;
  frameIsMap = false;
  uint16_t *fb = gfx->getFramebuffer();
  uint16_t *img = badgeCurrentImg();
  int bw = badgeCurrentW(), bh = badgeCurrentH();
  if (badgeHasCurrent() && img && bw > 0 && bh > 0) {
    if (bw == LCD_WIDTH && bh == LCD_HEIGHT) {
      memcpy(fb, img, (size_t)LCD_WIDTH * LCD_HEIGHT * 2);
    } else {
      for (int y = 0; y < LCD_HEIGHT; y++) {
        int sy = y * bh / LCD_HEIGHT;
        if (sy >= bh) sy = bh - 1;
        const uint16_t *src = img + (size_t)sy * bw;
        uint16_t *dst = fb + (size_t)y * LCD_WIDTH;
        for (int x = 0; x < LCD_WIDTH; x++) {
          int sx = x * bw / LCD_WIDTH;
          if (sx >= bw) sx = bw - 1;
          dst[x] = src[sx];
        }
      }
    }
    badgeDrawDots(fb);
  } else {
    printCentered("BADGE", CX, CX - 24, 5, C_GREY);
    printCentered("swipe down", CX, CX + 36, 2, 0x4208);
  }
  badgeDirty = false;
  badgeCarouselAt = millis();
  needFlush = true;
}

void forceExitBadge(bool redraw) {
  if (page != 3) return;
  if (sliding) dragAbort();
  page = 0;
  pageAt = millis();
  badgeDirty = false;
  if (!redraw || callOn || pairOn || fxOn) return;
  if (inNav) navDirty = true;
  else {
    riderPrevF = -1; riderPrevZ = -2; catForce = true;
    drawWaiting(bleConnected ? WAIT_READY : WAIT_NOLINK);
  }
}

void enterBadgeMode() {
  if (inNav || cruiseOn || callOn || pairOn || fxOn) return;
  if (page == 3) { badgeDirty = true; return; }
  if (sliding) dragAbort();
  page = 3;
  pageAt = millis();
  onWaitScreen = false;
  drawBadgePage();
}

/// 轮播切图：dir=+1 下一张 / -1 上一张；循环。
bool badgeStep(int dir) {
  if (badgeCount <= 1) return false;
  int next = badgeIndex + dir;
  if (next < 0) next = badgeCount - 1;
  if (next >= badgeCount) next = 0;
  // 跳过空槽（理论上 count 连续，但仍防御）
  for (int k = 0; k < BADGE_SLOTS; k++) {
    if (next < 0) next = BADGE_SLOTS - 1;
    if (next >= BADGE_SLOTS) next = 0;
    if (badgeSlotOk[next]) { badgeIndex = next; badgeDirty = true; badgeCarouselAt = millis(); return true; }
    next += dir;
  }
  return false;
}

void badgeTick() {
  if (page != 3) return;
  if (inNav || cruiseOn) { forceExitBadge(true); return; }
  if (!sliding && !callOn && !pairOn && !fxOn) {
    if (badgeCount > 1 && (millis() - badgeCarouselAt) >= BADGE_CAROUSEL_MS) {
      badgeStep(+1);
    }
    if (badgeDirty) drawBadgePage();
  }
}
