#pragma once
// v7.12.9-ui.3：PWR 键短按 = 息屏 / 再按亮屏（接电时生效；长按关机仍由 AXP2101 硬件完成，原行为不变）。
// 短按事件从 AXP2101 的 PEKEY 中断状态位读取（STATUS2 bit3，写 1 清除），主循环轮询。
// 电池供电时不息屏（防骑行误触；已息屏时短按仍可亮屏，避免没电时把屏锁死）。
// 由 MotoNav.ino 在 Battery.h / DeviceContext.h / DisplayPipeline.h 之后包含，仅编译一次。
// 依赖：pmuRead / pmuSetBit / PMU_ADDR / pmuOK / usbIn（Battery.h）、panel（DeviceContext.h）、
//       flushDone（DisplayPipeline.h）、idlePowerOffTimer + idlePowerOffReset（IdlePowerOff.h）。

bool screenOff = false;        // 当前是否处于息屏
int  lastBrightness = 200;     // 息屏前的亮度（开机默认 200；#b 亮度命令实时更新）

// 亮屏恢复用：记录最新亮度；息屏期间收到新亮度命令时保持黑屏、只记账
void noteBrightnessForWake(int b) {
  lastBrightness = b;
  if (screenOff) panel->setBrightness(0);
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

// 开机调一次：使能 AXP2101 PEKEY 短按中断（ENABLE2 0x41 bit3）
void pwrKeyInit() {
  if (!pmuOK) return;
  pmuSetBit(0x41, 3);
}

// 主循环每圈调用：读并清 PEKEY 短按事件；接电时短按切换息屏，电池供电时只允许亮屏
void pwrKeyPoll() {
  if (!pmuOK) return;
  const int st = pmuRead(0x49);                    // STATUS2
  if (st < 0 || !(st & 0x08)) return;
  Wire.beginTransmission(PMU_ADDR);                // 写 1 清除短按事件
  Wire.write(0x49);
  Wire.write((uint8_t)0x08);
  Wire.endTransmission();
  idlePowerOffReset(&idlePowerOffTimer, millis()); // 手动息屏/亮屏算一次交互，别和闲置关机打架
  if (screenOff) {                                 // 已息屏：任何时候短按都亮屏
    screenOffApply(false);
    return;
  }
  if (!usbIn) {                                    // 电池供电：不息屏（防骑行误触）
    Serial.println("PWR short press ignored on battery");
    return;
  }
  screenOffApply(true);
}
