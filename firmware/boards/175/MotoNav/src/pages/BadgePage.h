#pragma once
// 电子吧唧页（page==3）：主屏上划进入；全圆静图；下划 / 导航·巡航强制退出。
// 由 MotoNav.ino 按依赖顺序包含，仅编译一次。

void drawBadgePage() {
  gfx->fillScreen(C_BLACK);
  onWaitScreen = false;
  frameIsMap = false;
  if (badgeValid && badgeImg && badgeW > 0 && badgeH > 0) {
    uint16_t *fb = gfx->getFramebuffer();
    if (badgeW == LCD_WIDTH && badgeH == LCD_HEIGHT) {
      memcpy(fb, badgeImg, (size_t)LCD_WIDTH * LCD_HEIGHT * 2);
    } else {
      // 非满屏：居中最近邻铺满（MVP 管线固定 466×466，正常不走这里）
      for (int y = 0; y < LCD_HEIGHT; y++) {
        int sy = y * badgeH / LCD_HEIGHT;
        if (sy >= badgeH) sy = badgeH - 1;
        const uint16_t *src = badgeImg + (size_t)sy * badgeW;
        uint16_t *dst = fb + (size_t)y * LCD_WIDTH;
        for (int x = 0; x < LCD_WIDTH; x++) {
          int sx = x * badgeW / LCD_WIDTH;
          if (sx >= badgeW) sx = badgeW - 1;
          dst[x] = src[sx];
        }
      }
    }
  } else {
    printCentered("BADGE", CX, CX - 24, 5, C_GREY);
    printCentered("swipe down", CX, CX + 36, 2, 0x4208);
  }
  badgeDirty = false;
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

void badgeTick() {
  if (page != 3) return;
  if (inNav || cruiseOn) { forceExitBadge(true); return; }
  if (!sliding && !callOn && !pairOn && !fxOn && badgeDirty) drawBadgePage();
}
