#pragma once
// ============================ 电源键（PWR / PEKEY）============================
// v7.12.9-ui.3 ：PWR 短按 = 息屏 / 再按亮屏（接电时生效；电池供电只允许亮屏）。
// v7.12.9-ui.15：PWR 长按 ≈1 秒 = 弹出电源菜单（关机 / 重启），点选后执行。
// musicA.49：原「休眠」改为「重启」（ESP.restart），不再进深睡。
//
// AXP2101 按键时序寄存器 0x27：
//   IRQLEVEL[5:4] 长按中断时长（00=1s 01=1.5s 10=2s 11=2.5s，缺省 1.5s）
//   OFFLEVEL[3:2] 长按硬件关机时长（00=4s 01=6s 10=8s 11=10s，缺省 6s）
//   ONLEVEL[1:0]  开机按键时长（EFUSE，保留不动）
//   开机把长按中断提前到 1s、把 PMU 硬件关机推后到 10s：菜单有约 9 秒操作窗口；
//   固件卡死时长按 10s 仍由 PMU 硬件关机，保底路径不变。
// 中断状态 INTSTS2(0x49)，写 1 清除（0x41 使能位一一对应）：
//   bit0 按下边沿  bit1 松开边沿  bit2 长按  bit3 短按
//
// 由 MotoNav.ino 在 Battery.h / DeviceContext.h / DisplayPipeline.h / VectorDrawing.h
// 之后包含，仅编译一次。
// 依赖：pmuRead / pmuSetBit / PMU_ADDR / pmuOK / usbIn / batPct（Battery.h）、
//       panel（DeviceContext.h）、flushDone（DisplayPipeline.h）、printCentered、
//       touchVectorToCanvas、idlePowerOffShutdown（FunctionDeclarations.h）。

bool screenOff = false;        // 当前是否处于息屏
int  lastBrightness = 200;     // 息屏前的亮度（开机默认 200；#b 亮度命令实时更新）

// ---------------------------- ui.15 电源菜单 ----------------------------
bool         powerMenuOn  = false;    // 菜单是否展开
unsigned long powerMenuAt = 0;        // 展开时刻（超时自动收起）
#define PWR_MENU_TIMEOUT_MS 20000     // 20 秒没操作自动收起

#define PM_BG     0x0023   // 面板底 #06061E
#define PM_EDGE   0x4B1D   // 面板描边 #4A63E8
#define PM_DIM    0x7C39   // 次要文字 #7A86C8
#define PM_CY     0x37FF   // 重启主色 #35E0FF
#define PM_CY_D   0x0A36   // 重启按钮底
#define PM_RD     0xF800   // 关机主色
#define PM_RD_D   0x2004   // 关机按钮底

// 亮屏恢复用：记录最新亮度；息屏期间收到新亮度命令时保持黑屏、只记账
void noteBrightnessForWake(int b) {
  lastBrightness = b;
  if (screenOff) panel->setBrightness(0);
}

static void pmuWrite8(uint8_t reg, uint8_t v) {
  Wire.beginTransmission(PMU_ADDR);
  Wire.write(reg);
  Wire.write(v);
  Wire.endTransmission();
}

// 息屏切换（CO5300 AMOLED：displayOff 彻底灭屏，比亮度 0 更省电）。
// 刷屏任务占用显示总线时等最多 3 秒；等不到就放弃本次切换，保持原状态，下次短按再试。
void screenOffApply(bool off) {
  if (off == screenOff) return;
  if (flushDone && xSemaphoreTake(flushDone, pdMS_TO_TICKS(3000)) != pdTRUE) {
    Serial.println("PWR: display busy, state unchanged");
    return;
  }
  screenOff = off;
  if (off) {
    panel->setBrightness(0);
    panel->displayOff();
  } else {
    panel->displayOn();
    panel->setBrightness(lastBrightness);
  }
  if (flushDone) xSemaphoreGive(flushDone);
  Serial.println(off ? "PWR short press: screen off" : "PWR short press: screen on");
}

// ------------------------------ 菜单绘制 ------------------------------
// 电源符号：开口圆环 + 中间竖条
void pmIconPower(int cx, int cy, uint16_t c, uint16_t bg) {
  for (int r = 16; r >= 14; r--) gfx->drawCircle(cx, cy, r, c);
  gfx->fillRect(cx - 6, cy - 22, 12, 13, bg);          // 顶部开口
  gfx->fillRect(cx - 2, cy - 20, 4, 20, c);            // 竖条（穿过开口）
}
// 重启符号：开口圆环 + 箭头头（musicA.49 替换原月牙休眠图标）
void pmIconRestart(int cx, int cy, uint16_t c, uint16_t bg) {
  for (int r = 16; r >= 14; r--) gfx->drawCircle(cx, cy, r, c);
  gfx->fillRect(cx + 4, cy - 18, 14, 12, bg);              // 右上开口
  gfx->fillTriangle(cx + 2, cy - 18, cx + 14, cy - 18, cx + 8, cy - 8, c);  // 箭头
}

// 画在所有页面之上（MotoNav.ino 在推屏前调用）。面板不透明，反复重画不会越压越暗。
void drawPowerMenu() {
  gfx->fillRoundRect(62, 132, 342, 200, 20, PM_BG);
  gfx->drawRoundRect(62, 132, 342, 200, 20, PM_EDGE);
  gfx->drawRoundRect(64, 134, 338, 196, 18, PM_EDGE);

  printCentered("POWER", CX, 150, 3, C_WHITE);
  printCentered("SELECT AN OPTION", CX, 186, 1, PM_DIM);
  if (batPct >= 0) printCentered(String("BATT ") + batPct + "%", 348, 142, 1, PM_DIM);

  // 关机
  gfx->fillRoundRect(84, 204, 141, 84, 12, PM_RD_D);
  gfx->drawRoundRect(84, 204, 141, 84, 12, PM_RD);
  pmIconPower(154, 232, PM_RD, PM_RD_D);
  printCentered("SHUTDOWN", 154, 258, 2, PM_RD);
  printCentered("POWER OFF", 154, 276, 1, PM_DIM);

  // 重启（musicA.49：原 SLEEP / DEEP SLEEP）
  gfx->fillRoundRect(241, 204, 141, 84, 12, PM_CY_D);
  gfx->drawRoundRect(241, 204, 141, 84, 12, PM_CY);
  pmIconRestart(311, 232, PM_CY, PM_CY_D);
  printCentered("RESTART", 311, 258, 2, PM_CY);
  printCentered("REBOOT DEVICE", 311, 276, 1, PM_DIM);

  printCentered("TAP ELSEWHERE OR PWR TO CANCEL", CX, 306, 1, PM_DIM);
}

void powerMenuOpen() {
  if (screenOff) screenOffApply(false);     // 息屏时长按：先点亮再弹菜单
  powerMenuOn = true;
  powerMenuAt = millis();
  Serial.println("PWR long press: power menu");
}

void powerMenuClose() {
  if (!powerMenuOn) return;
  powerMenuOn = false;
  Serial.println("PWR menu: cancelled");
  if (page == 1) wxDirty = true;
  else if (page == 2) musicDirty = true;
  else if (inNav) navDirty = true;
  else drawWaiting(shownConn ? WAIT_READY : WAIT_NOLINK);   // 0/3/4 页由各自节拍重画
  needFlush = true;
}

// 点选：命中按钮就执行，点其它地方取消
void powerMenuTap(int x, int y) {
  float dx = x - CX, dy = y - CX;            // 圆屏 y 圆心 = CX
  touchVectorToCanvas(dx, dy);
  x = (int)lroundf(CX + dx); y = (int)lroundf(CX + dy);
  if (y >= 204 && y <= 288) {
    if (x >= 84 && x <= 225) {
      Serial.println("PWR menu: shutdown");
      powerMenuOn = false;
      idlePowerOffShutdown();                // 关电源芯片（要按 PWR 才能再开机）
      return;
    }
    if (x >= 241 && x <= 382) {
      Serial.println("PWR menu: restart");
      powerMenuOn = false;
      delay(50);
      ESP.restart();                         // musicA.49：重启设备（非深睡）
      return;
    }
  }
  powerMenuClose();
}

// ------------------------------ 休眠（深睡）------------------------------
// 实现在 Shutdown.h（要用到 nimble/蓝牙停机，那边包含顺序最稳）。
// 屏幕灭 + 蓝牙停 + ESP32-S3 深睡；电源芯片保持供电，所以能自己醒。
// 唤醒源优先用触摸屏中断脚（GPIO11，点一下屏幕就醒），该脚此刻若仍为低
// （手指没离开 / 芯片拉着）就改用 BOOT（GPIO0）；两个都拉不起来就直接关机，
// 避免睡下去醒不过来。

// ------------------------------ 开机初始化 ------------------------------
// 使能 PEKEY 短按 + 长按中断，并把长按时序改成“1 秒弹菜单 / 10 秒才硬件关机”。
void pwrKeyInit() {
  if (!pmuOK) return;
  pmuSetBit(0x41, 3);                      // 短按中断
  pmuSetBit(0x41, 2);                      // 长按中断（ui.15 菜单靠它）
  const int t = pmuRead(0x27);
  if (t >= 0) {
    const uint8_t v = (uint8_t)((t & 0xC3) | 0x0C);   // IRQLEVEL=00(1s) OFFLEVEL=11(10s)
    pmuWrite8(0x27, v);
    Serial.printf("PWR timing: 0x27 %02X -> %02X (irq 1s, hw off 10s)\n", t, pmuRead(0x27) & 0xFF);
  }
  pmuWrite8(0x49, 0x0F);                   // 清掉开机前的残留按键事件
}

// ------------------------------ 主循环轮询 ------------------------------
void pwrKeyPoll() {
  if (!pmuOK) return;
  const int st = pmuRead(0x49);                    // INTSTS2
  if (st >= 0) {
    const uint8_t ev = (uint8_t)(st & 0x0F);       // bit0 按下 bit1 松开 bit2 长按 bit3 短按
    if (ev) {
      pmuWrite8(0x49, ev);                         // 写 1 清除
      idlePowerOffReset(&idlePowerOffTimer, millis());   // 按键算一次交互，别和闲置关机打架
      Serial.printf("PWR key event=%02X\n", ev);
      if (ev & 0x04) {                             // 长按 → 电源菜单
        powerMenuOpen();
        return;
      }
      if (ev & 0x08) {                             // 短按
        if (powerMenuOn) { powerMenuClose(); return; }
        if (screenOff) { screenOffApply(false); return; }     // 已息屏：任何时候短按都亮屏
        if (!usbIn) {                                          // 电池供电：不息屏（防骑行误触）
          Serial.println("PWR short press ignored on battery");
          return;
        }
        screenOffApply(true);
        return;
      }
    }
  }
  if (powerMenuOn && millis() - powerMenuAt > PWR_MENU_TIMEOUT_MS) powerMenuClose();
}
