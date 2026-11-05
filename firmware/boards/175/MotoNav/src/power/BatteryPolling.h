#pragma once
// 电量轮询及状态消息。由 MotoNav.ino 按依赖顺序包含，仅编译一次。
String pendingStat2 = "";
// v6.8 每 5 秒读一次；有变化就重画、通知手机
void batPoll() {
  if (!pmuOK) return;
  unsigned long now = millis();
  if (batAt && now - batAt < 5000) return;
  batAt = now;
  int s1 = pmuRead(0x00), s2 = pmuRead(0x01);
  if (s1 < 0 || s2 < 0) return;
  bool present = (s1 >> 3) & 1;
  int pct = -1;
  if (present) {
    int v = pmuRead(0xA4);
    if (v >= 0 && v <= 100) pct = v;
    int h = pmuRead(0x34), l = pmuRead(0x35);
    if (h >= 0 && l >= 0) batMv = ((h & 0x1F) << 8) | l;
  }
  // v7.2 偶尔读不到电池：先沿用上次的电量，连续 3 次读不到才当成没接电池
  static int missCnt = 0;
  if (pct < 0) { if (batPct >= 0 && ++missCnt < 3) { pct = batPct; present = true; } }
  else missCnt = 0;
  // musicA.31：油量计越界（0xFF 等）≠ 没接电池。电池还在位就用电压估一个，
  // 否则一条 V,-1 发过去，App 圆屏 tab 就变成「设备电量 —」。
  if (pct < 0 && present && batMv > 2500) {
    pct = batPctFromMv(batMv);
    missCnt = 0;
    if (pct <= 0) pct = 1;                          // 还在跑就至少 1%，0% 会被当成已关机
  }
  if (!present) pct = -1;
  bool chg = present && ((s2 >> 5) & 3) == 1;
  bool usb = ((s1 >> 5) & 1) && !((s2 >> 3) & 1);
  bool changed = pct != batPct || chg != batChg || usb != usbIn;
  batPct = pct; batChg = chg; usbIn = usb;
  if (mediaHid && changed && pct >= 0) mediaHid->setBatteryLevel((uint8_t)constrain(pct, 0, 100));
  if (changed) {
    // 屏保期间画布是眼睛动画，没有状态栏可重画（以前这里会把状态栏糊到屏保上）
    if (!saverActive && onWaitScreen && page == 0 && !sliding) drawStatus();
    else if (inNav) navDirty = true;
  }
  if (changed || batPushNow || now - batSentAt > 60000) {
    batPushNow = false;
    batSentAt = now;
    pendingBat = "V," + String(batPct) + "," + String(batChg ? 1 : 0) + "," + String(usbIn ? 1 : 0) + "," + String(batMv);
    // musicA.31：低电量时 App 显示「—」就是这里发了 -1。留一行日志方便现场核对。
    if (changed) Serial.printf("[BAT] pct=%d mv=%d chg=%d usb=%d\n", batPct, batMv, (int)batChg, (int)usbIn);
  }
}
