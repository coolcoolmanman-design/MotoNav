#pragma once
// 电池、连接状态、车速和限速牌。由 MotoNav.ino 按依赖顺序包含，仅编译一次。
// 塞尔达40周年特别版：首页待机背景是纹理图，擦除走 statusErase（HomePage.h 定义：
// 待机页恢复纹理，其他页黑底），避免状态栏在金纹背景上擦出黑条。
void statusErase(int x, int y, int w, int h);
void drawBattery(int x0, int y0, bool backing = false) {
  if (batPct < 0) return;                                  // 没接电池不画
  // musicA.40：电量图标加大（约 40×18，原 30×14）
  const int W = 40, H = 18;
  if (backing) {                                           // v7.2 小地图上垫一块黑底
    gfx->fillRoundRect(x0 - 4, y0 - 3, W + 2 + 8, H + 6, 6, C_BLACK);
  }
  uint16_t c = slotBattColor ? slotBattColor                      // musicA.8 电量槽颜色可由 App 覆盖（0 = 自动）
              : batChg ? C_CYAN : batPct > 50 ? C_GREEN : batPct >= 20 ? 0xFE60 : C_RED;
  if (!batChg && batPct < 10 && (millis() / 600) % 2) c = 0x4208;   // 快没电：闪
  gfx->drawRoundRect(x0, y0, W, H, 3, c);
  gfx->drawRoundRect(x0 + 1, y0 + 1, W - 2, H - 2, 2, c);
  gfx->fillRect(x0 + W, y0 + 5, 3, H - 10, c);
  // v7.12.10-musicA.8 电池+百分比合二为一：数字嵌在图标内部（充电时画闪电不显示数字）
  if (batChg) {
    int bx = x0 + W / 2, by = y0 + H / 2;
    gfx->fillTriangle(bx + 2, by - 6, bx - 4, by + 1, bx + 1, by + 1, C_BLACK);
    gfx->fillTriangle(bx - 2, by + 6, bx + 4, by - 1, bx - 1, by - 1, C_BLACK);
  } else {
    String t = String(batPct);
    int tw = t.length() * 6;
    gfx->setTextSize(1);
    gfx->setTextColor(c);
    gfx->setCursor(x0 + (W - tw) / 2, y0 + (H - 8) / 2);
    gfx->print(t);
  }
}

void drawStatus() {
  // musicA.8 组件槽：蓝牙点归连接态槽（bit2）、电池归电量槽（bit1），可分别关
  // musicA.40：加大电量后整组略加宽，擦除带抬高以免与上方槽抢位
  const int sx = CX - 32;
  statusErase(sx - 3, 18, 78, 24);
  if (slotMask & 0x04) aaDisc(sx + 8, 29, 8, bleConnected ? slotColor(C_GREEN, slotConnColor) : C_GREY);
  if (slotMask & 0x02) drawBattery(sx + 22, 20);           // 电量（蓝牙小圆点右边，百分比内嵌）
  needFlush = true;
}

// ================= 车速和限速牌 =================

void drawSpeedBlock(bool over) {
  if (speedKmh < 0) return;
  int sx = hasCam ? 200 : CX;
  printCentered(String(speedKmh), sx, SPEED_Y, 5, over ? C_RED : C_WHITE);   // 每个字符 30x40
  printCentered("km/h", sx, SPEED_Y + 46, 2, over ? 0xFC10 : C_GREY);
  if (hasCam) {
    const int bx = 298, by = 76;
    aaDisc(bx, by, 28, C_SIGNRED);
    aaDisc(bx, by, 21, C_WHITE);
    String lim = String(camLimit);
    int sz = lim.length() >= 3 ? 2 : 3;
    printCentered(lim, bx, by - 4 * sz, sz, C_BLACK);
    printCentered(distText(camDist), bx, by + 34, 2, over ? 0xFC10 : C_GREY);
  }
}
