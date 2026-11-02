#pragma once
// 五分钟闲置关机和深睡回退。由 MotoNav.ino 按依赖顺序包含，仅编译一次。
// v7.12.9-ui.15 休眠：屏幕灭 + 蓝牙停 + ESP32-S3 深睡，电源芯片保持供电所以能自己醒。
// 唤醒源优先触摸屏中断脚（GPIO11，点一下屏幕就醒），该脚此刻仍为低就改用 BOOT（GPIO0）；
// 两个都拉不起来就直接关机，避免睡下去醒不过来。
// 和关机的区别：关机把 PMU 也关掉（必须按 PWR 才能开机），休眠只是 CPU 深睡（低功耗、几秒内唤醒）。
void hibernateNow() {
  powerMenuOn = false;
  powerOffPending = true;
  Serial.println("Hibernate: screen off + BLE off + deep sleep");
  const bool displayReady = !flushDone ||
      xSemaphoreTake(flushDone, pdMS_TO_TICKS(3000)) == pdTRUE;
  if (displayReady) {
    panel->setBrightness(0);
    panel->displayOff();
  }
  // Power down before destroying BLE objects: the host can still dispatch events
  // referencing those objects. Shutdown does not need to reclaim their memory.
#if defined(CONFIG_NIMBLE_ENABLED)
  nimble_port_stop();
#elif defined(CONFIG_BLUEDROID_ENABLED)
  esp_bluedroid_disable();
#endif
#if CONFIG_BT_CONTROLLER_ENABLED
  esp_bt_controller_disable();
#endif
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  const gpio_num_t hibPins[2] = {GPIO_NUM_11, GPIO_NUM_0};     // TP_INT / BOOT
  for (int i = 0; i < 2; i++) {
    rtc_gpio_init(hibPins[i]);
    rtc_gpio_set_direction(hibPins[i], RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pullup_en(hibPins[i]);
    rtc_gpio_pulldown_dis(hibPins[i]);
  }
  uint32_t waited = millis();                                  // 等触摸/按键松开（最多 2 秒）
  while (millis() - waited < 2000 &&
         !(rtc_gpio_get_level(hibPins[0]) && rtc_gpio_get_level(hibPins[1]))) delay(10);
  gpio_num_t wake = hibPins[0];
  if (!rtc_gpio_get_level(wake)) wake = hibPins[1];
  if (!rtc_gpio_get_level(wake)) {
    Serial.println("Hibernate: no free wake source, powering off instead");
    idlePowerOffShutdown();
    return;
  }
  esp_sleep_enable_ext0_wakeup(wake, 0);                       // 低电平唤醒
  Serial.printf("Hibernate: sleeping, wake on GPIO%d low\n", (int)wake);
  delay(50);
  esp_deep_sleep_start();
}

// Power down before destroying BLE objects: the host can still dispatch events
// referencing those objects. Shutdown does not need to reclaim their memory.
void idlePowerOffShutdown() {
  powerOffPending = true;
  Serial.println("No navigation for 5 minutes: powering off");
  const bool displayReady = !flushDone ||
      xSemaphoreTake(flushDone, pdMS_TO_TICKS(3000)) == pdTRUE;
  if (displayReady) {
    panel->setBrightness(0);
    panel->displayOff();
  }
  // Check the chip ID, rather than writing to an unknown device at address 0x34.
  const int chip = pmuRead(0x03);
  Serial.printf("Shutdown: PMIC id=%02X, on=%02X, off=%02X\n",
                chip, pmuRead(0x20), pmuRead(0x21));
  if (chip == 0x4A) {
    const int config = pmuRead(0x10);
    if (config >= 0) {
      Wire.beginTransmission(PMU_ADDR);
      Wire.write(0x10);
      // Bit 1 is a separate, self-clearing RESET command; never replay it.
      Wire.write((uint8_t)((config & ~0x02) | 0x01));
      const int result = Wire.endTransmission();
      Serial.printf("Shutdown: PMIC command result=%d\n", result);
      delay(1000);
    }
  }
  // If the PMIC is unavailable or external power keeps the CPU supplied, stop
  // screen/BLE/CPU activity instead of repeatedly rebooting or staying awake.
  // Normal PMIC-off wakes via PWR; this fallback can also wake via BOOT (GPIO0).
  Serial.println("PMIC did not remove CPU power; entering deep sleep");
  // Stop the host/controller without deleting live GATT objects. Deep sleep
  // discards RAM anyway. Keep objects valid until pending callbacks have ended.
#if defined(CONFIG_NIMBLE_ENABLED)
  nimble_port_stop();
#elif defined(CONFIG_BLUEDROID_ENABLED)
  esp_bluedroid_disable();
#endif
#if CONFIG_BT_CONTROLLER_ENABLED
  esp_bt_controller_disable();
#endif
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  rtc_gpio_init(GPIO_NUM_0);
  rtc_gpio_set_direction(GPIO_NUM_0, RTC_GPIO_MODE_INPUT_ONLY);
  rtc_gpio_pullup_en(GPIO_NUM_0);
  rtc_gpio_pulldown_dis(GPIO_NUM_0);
  // EXT0 is level-triggered: arming it while BOOT is held low causes an
  // immediate wake/reboot. Require 100 ms released, with a bounded wait.
  uint32_t releasedAt = millis(), waitAt = releasedAt;
  while (millis() - waitAt < 1500 && millis() - releasedAt < 100) {
    if (!rtc_gpio_get_level(GPIO_NUM_0)) releasedAt = millis();
    delay(10);
  }
  if (millis() - releasedAt >= 100 && rtc_gpio_get_level(GPIO_NUM_0)) {
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0);
  } else {
    Serial.println("BOOT is low; leave wake disabled to avoid instant restart (use PWR to power cycle)");
  }
  esp_deep_sleep_start();
}
