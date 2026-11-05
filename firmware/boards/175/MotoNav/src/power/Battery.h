#pragma once
// 电源芯片读取及外接电源检测。由 MotoNav.ino 按依赖顺序包含，仅编译一次。
// ================= v6.8 电池（AXP2101 电源芯片，和触摸屏共用 I2C，地址 0x34）=================
#define PMU_ADDR 0x34
bool pmuOK = false;
int batPct = -1;                       // -1 = 没接电池
bool batChg = false, usbIn = false;
int batMv = 0;
unsigned long batAt = 0, batSentAt = 0;
bool batPushNow = false;               // musicA.31：手机刚认证时立刻补推一次电量，别等 60 秒心跳
String pendingBat = "";

int pmuRead(uint8_t reg) {
  Wire.beginTransmission(PMU_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return -1;
  if (Wire.requestFrom((uint8_t)PMU_ADDR, (uint8_t)1) != 1) return -1;
  return Wire.read();
}
void pmuSetBit(uint8_t reg, uint8_t bit) {
  int v = pmuRead(reg);
  if (v < 0 || (v & (1 << bit))) return;
  Wire.beginTransmission(PMU_ADDR);
  Wire.write(reg);
  Wire.write((uint8_t)(v | (1 << bit)));
  Wire.endTransmission();
}

// musicA.31：AXP2101 电量计（0xA4）在快没电 / 刚上电 / 温度极端时会返回 0xFF 之类的越界值，
// 旧逻辑直接判成「没电池」（-1）发给手机，App 那边就显示成「—」。电池明明在，必须给个数。
// 这里按单节锂电的放电曲线做分段线性插值（空载电压，骑行带载会略低，误差可接受）。
int batPctFromMv(int mv) {
  static const int16_t mvTab[] = {3300, 3450, 3550, 3650, 3720, 3780, 3850, 3950, 4050, 4150, 4200};
  static const int8_t  pcTab[] = {   0,    1,    5,   18,   38,   55,   70,   82,   91,   97,  100};
  const int n = (int)(sizeof(mvTab) / sizeof(mvTab[0]));
  if (mv <= mvTab[0]) return 0;
  if (mv >= mvTab[n - 1]) return 100;
  for (int i = 1; i < n; i++) {
    if (mv < mvTab[i]) {
      const int span = mvTab[i] - mvTab[i - 1];
      const int pct = pcTab[i - 1] + (int)((long)(mv - mvTab[i - 1]) * (pcTab[i] - pcTab[i - 1]) / span);
      return pct < 0 ? 0 : (pct > 100 ? 100 : pct);
    }
  }
  return 100;
}

// 开机时调一次：只打开电量计、电池检测、电池电压测量，不动任何供电设置
void batInit() {
  pmuOK = pmuRead(0x00) >= 0;
  if (!pmuOK) return;
  pmuSetBit(0x18, 3);                  // 电量计
  pmuSetBit(0x68, 0);                  // 电池检测
  pmuSetBit(0x30, 0);                  // 电池电压测量
}

// AXP2101 STATUS1 bit 5 is VBUS-good, independent of charging/full state.
// Unknown/stale power readings keep the device awake, rather than guessing battery.
void idlePowerPollSupply(bool force = false) {
  const uint32_t now = millis();
  if (!force && idlePowerSupply.sampled &&
      (uint32_t)(now - idlePowerSupply.checkedAt) < 500) return;
  const bool wasBattery = idlePowerSupplyAllowsOff(&idlePowerSupply, now);
  const int status = pmuOK ? pmuRead(0x00) : -1;
  idlePowerSupplyUpdate(&idlePowerSupply, now, status);
  // Unplugging / recovering a valid battery-only reading starts a full interval.
  if (!wasBattery && idlePowerSupplyAllowsOff(&idlePowerSupply, now))
    idlePowerOffReset(&idlePowerOffTimer, now);
}

// 小电池图标：x0,y0 = 左上角（电池身 24x12，右边小凸头，再右边是百分比数字）
