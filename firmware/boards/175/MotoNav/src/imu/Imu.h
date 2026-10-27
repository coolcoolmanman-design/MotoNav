#pragma once
// v7.12.9-ui.11 IMU 驱动：圆屏本体传感器，运行时探测芯片型号。
// ui.10 实测 QMI8658/LSM6DSL/SC7A20 三个候选都没探测到——开机先做一次全总线 I2C 扫描
// 并把结果打到串口（"I2C scan:" 行），探测候选扩到 6 种；扫描日志用于确认板上真身。
// G 值口径与手机 #G 一致：减去重力（矢量低通估计），静止≈0、晃动/颠簸为动态值。
// 依赖 Wire（MotoNav.ino 已 Wire.begin）；由 Battery.h 之后包含，仅编译一次。

bool    imuOK = false;
uint8_t imuType = 0;                       // 1=QMI8658 2=LSM6DSL/DS3 3=SC7A20 4=BMA423 5=MPU6050 6=ICM-42670
uint8_t imuAddr = 0;
float   imuRawG = 0;                       // 含重力的合加速度模长（自检用，静止≈1）
float   gCurOut = 0;                       // imuPoll 的输出（动态 G）
float   imuGX = 0, imuGY = 0, imuGZ = 0;   // 重力矢量低通估计
bool    imuGInit = false;

int imuR8(uint8_t reg) {
  Wire.beginTransmission(imuAddr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return -1;
  if (Wire.requestFrom((int)imuAddr, 1) != 1) return -1;
  return Wire.read();
}
bool imuW8(uint8_t reg, uint8_t v) {
  Wire.beginTransmission(imuAddr);
  Wire.write(reg); Wire.write(v);
  return Wire.endTransmission() == 0;
}
bool imuBurst(uint8_t reg, uint8_t *buf, int n) {
  Wire.beginTransmission(imuAddr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)imuAddr, n) != n) return false;
  for (int i = 0; i < n; i++) buf[i] = Wire.read();
  return true;
}

// 由原始三轴（线性 g）计算输出值：raw = 含重力模长；g = 减重力后的动态值
void imuFinish(float ax, float ay, float az) {
  imuRawG = sqrtf(ax * ax + ay * ay + az * az);
  if (!imuGInit) { imuGX = ax; imuGY = ay; imuGZ = az; imuGInit = true; }
  imuGX += 0.1f * (ax - imuGX);            // ~1s 时间常数：静止时收敛到重力方向
  imuGY += 0.1f * (ay - imuGY);
  imuGZ += 0.1f * (az - imuGZ);
  float dx = ax - imuGX, dy = ay - imuGY, dz = az - imuGZ;
  float g = sqrtf(dx * dx + dy * dy + dz * dz);
  gCurOut = (g < 0.06f) ? 0 : g;           // 死区滤噪声，静止稳定显示 0.00
}

// 一次采样：g = 动态 G（去重力，静止≈0）；dps = 角速度模长（无陀螺则 -1）
bool imuPoll(float &g, float &dps) {
  g = 0; dps = -1;
  if (!imuOK) return false;
  uint8_t b[14];
  float ax = 0, ay = 0, az = 0;
  bool ok = false;
  if (imuType == 1) {                      // QMI8658：加速度 0x35.. 陀螺 0x3B..，一次连读 12 字节
    if (!imuBurst(0x35, b, 12)) {          // burst 失败 → 单字节逐读兜底（部分批次 burst 行为异常）
      for (int i = 0; i < 12; i++) {
        int v = imuR8(0x35 + i);
        if (v < 0) return false;
        b[i] = (uint8_t)v;
      }
    }
    ax = (int16_t)(b[1] << 8 | b[0]) / 4096.0f;         // ±8g（4096 LSB/g）
    ay = (int16_t)(b[3] << 8 | b[2]) / 4096.0f;
    az = (int16_t)(b[5] << 8 | b[4]) / 4096.0f;
    float rx = (int16_t)(b[7] << 8 | b[6]) / 64.0f;     // ±512 dps
    float ry = (int16_t)(b[9] << 8 | b[8]) / 64.0f;
    float rz = (int16_t)(b[11] << 8 | b[10]) / 64.0f;
    dps = sqrtf(rx * rx + ry * ry + rz * rz);
    ok = true;
  } else if (imuType == 2) {               // LSM6DS3/DSL/DSO/DSV 家族：加速度 0x28.. 陀螺 0x22..
    if (!imuBurst(0x28, b, 6)) return false;
    ax = (int16_t)(b[1] << 8 | b[0]) * 0.000244f;       // ±8g（4096 LSB/g）
    ay = (int16_t)(b[3] << 8 | b[2]) * 0.000244f;
    az = (int16_t)(b[5] << 8 | b[4]) * 0.000244f;
    if (imuBurst(0x22, b, 6)) {
      float rx = (int16_t)(b[1] << 8 | b[0]) * 0.070f;  // ±2000 dps
      float ry = (int16_t)(b[3] << 8 | b[2]) * 0.070f;
      float rz = (int16_t)(b[5] << 8 | b[4]) * 0.070f;
      dps = sqrtf(rx * rx + ry * ry + rz * rz);
    }
    ok = true;
  } else if (imuType == 3) {               // SC7A20：12 位左对齐 ±2g，1g=1024
    if (!imuBurst(0x28, b, 6)) return false;
    ax = ((int16_t)(b[1] << 8 | b[0]) >> 4) / 1024.0f;
    ay = ((int16_t)(b[3] << 8 | b[2]) >> 4) / 1024.0f;
    az = ((int16_t)(b[5] << 8 | b[4]) >> 4) / 1024.0f;
    ok = true;
  } else if (imuType == 4) {               // BMA423：12 位 ±2g 1g=1024，数据 0x12..
    if (!imuBurst(0x12, b, 6)) return false;
    ax = (int16_t)(b[1] << 8 | b[0]) / 1024.0f;
    ay = (int16_t)(b[3] << 8 | b[2]) / 1024.0f;
    az = (int16_t)(b[5] << 8 | b[4]) / 1024.0f;
    ok = true;
  } else if (imuType == 5) {               // MPU6050：±8g 1g=4096，数据 0x3B.. 陀螺 0x43..
    if (!imuBurst(0x3B, b, 14)) return false;
    ax = (int16_t)(b[0] << 8 | b[1]) / 4096.0f;
    ay = (int16_t)(b[2] << 8 | b[3]) / 4096.0f;
    az = (int16_t)(b[4] << 8 | b[5]) / 4096.0f;
    float rx = (int16_t)(b[8] << 8 | b[9]) / 32.768f;   // ±1000 dps
    float ry = (int16_t)(b[10] << 8 | b[11]) / 32.768f;
    float rz = (int16_t)(b[12] << 8 | b[13]) / 32.768f;
    dps = sqrtf(rx * rx + ry * ry + rz * rz);
    ok = true;
  } else if (imuType == 6) {               // ICM-42670：±8g 1g=4096，加速度 0x1B.. 陀螺 0x25..
    if (!imuBurst(0x1B, b, 12)) return false;
    ax = (int16_t)(b[0] << 8 | b[1]) / 4096.0f;
    ay = (int16_t)(b[2] << 8 | b[3]) / 4096.0f;
    az = (int16_t)(b[4] << 8 | b[5]) / 4096.0f;
    float rx = (int16_t)(b[6] << 8 | b[7]) / 32.768f;   // ±1000 dps
    float ry = (int16_t)(b[8] << 8 | b[9]) / 32.768f;
    float rz = (int16_t)(b[10] << 8 | b[11]) / 32.768f;
    dps = sqrtf(rx * rx + ry * ry + rz * rz);
    ok = true;
  } else if (imuType == 7) {               // BMA400：12 位左对齐 ±2g 1g=1024，数据 0x04..
    if (!imuBurst(0x04, b, 6)) return false;
    ax = ((int16_t)(b[1] << 8 | b[0]) >> 4) / 1024.0f;
    ay = ((int16_t)(b[3] << 8 | b[2]) >> 4) / 1024.0f;
    az = ((int16_t)(b[5] << 8 | b[4]) >> 4) / 1024.0f;
    ok = true;
  }
  if (!ok) return false;
  imuFinish(ax, ay, az);
  g = gCurOut;
  return true;
}

// 开机调一次（Wire.begin 之后）。扫描总线 + 探测 + 自检（含重力合加速度须约 1g）。
void imuInit() {
  // 全总线扫描：串口日志确认板上 I2C 设备（IMU 探测失败时的排查依据）
  Serial.print("I2C scan:");
  for (uint8_t a = 3; a < 0x78; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) { Serial.printf(" 0x%02X", a); }
  }
  Serial.println();

  const uint8_t addrs[] = {0x6B, 0x6A, 0x19, 0x18, 0x68, 0x69};
  for (uint8_t addr : addrs) {
    imuAddr = addr;
    int id00 = imuR8(0x00), id01 = imuR8(0x01), id0f = imuR8(0x0F), id75 = imuR8(0x75);
    Serial.printf("IMU probe 0x%02X: id00=0x%02X id01=0x%02X id0F=0x%02X id75=0x%02X\n",
                  addr, id00, id01, id0f, id75);       // 诊断：识别失败时看真身 ID
    if (id00 == 0x05) {                    // QMI8658
      // ui.16 诊断：CTRL7=0x03 已写对、回读也对，但 0x35 起的数据寄存器读不到（NAK）。
      // 这里在“传感器还没使能”的原生状态下把 0x00..0x5F 逐个单字节读一遍，
      // 看哪些地址 NAK——NAK 边界能直接说明芯片寄存器窗口/接口状态。
      for (int base = 0; base <= 0x50; base += 16) {
        Serial.printf("IMU map %02X:", base);
        for (int r = base; r < base + 16 && r <= 0x5F; r++) {
          int v = imuR8((uint8_t)r);
          Serial.print(v < 0 ? "  X" : "  o");
        }
        Serial.println();
      }

      imuW8(0x02, 0x40);                   // CTRL1：地址自增（bit5=BE 大端必须为 0，小端解包）
      imuW8(0x03, 0x25);                   // CTRL2 加速度 ±8g 224Hz（aFS=010）
      imuW8(0x04, 0x55);                   // CTRL3 陀螺 ±512dps 224Hz（gFS=101，与 /64 换算一致）
      imuW8(0x09, 0x80);                   // ui.16 CTRL8=0x80（SensorLib initImpl 同款步骤）
      // ui.15 真因：CTRL7 的寄存器地址是 0x08 不是 0x06（对过 SensorLib 的 REG_CTRL7）。
      // 之前写到 0x06 等于没使能传感器，STATUS 一直全是 0，自检拿不到 1g → 探测失败。
      imuW8(0x08, 0x03);                   // CTRL7 使能加速度（bit0）+ 陀螺（bit1）
      imuType = 1;
      delay(20);                           // 使能后等首批数据有效（QMI8658 约 1~2 个采样周期）
      Serial.printf("IMU diag 0x%02X: CTRL1=%02X CTRL2=%02X CTRL3=%02X CTRL8=%02X CTRL7=%02X\n",
                    addr, imuR8(0x02) & 0xFF, imuR8(0x03) & 0xFF, imuR8(0x04) & 0xFF,
                    imuR8(0x09) & 0xFF, imuR8(0x08) & 0xFF);
      {   // ui.16：状态与数据读取的逐项体检
        uint8_t b[16];
        Serial.printf("IMU st: STATUS_INT=%02X STATUS0=%02X TS=%02X%02X%02X temp=%02X%02X\n",
                      imuR8(0x2D), imuR8(0x2E),
                      imuR8(0x32), imuR8(0x31), imuR8(0x30), imuR8(0x34), imuR8(0x33));
        Serial.printf("IMU burst: acc6=%d gyr6=%d acc12=%d\n",
                      (int)imuBurst(0x35, b, 6), (int)imuBurst(0x3B, b, 6), (int)imuBurst(0x35, b, 12));
        Serial.print("IMU acc regs:");
        for (int r = 0x33; r <= 0x40; r++) Serial.printf(" %02X=%02X", r, imuR8((uint8_t)r));
        Serial.println();
        Wire.setClock(100000);             // 降速再试一次（排除 400k 下数据段读不稳）
        Serial.printf("IMU @100k: acc6=%d acc12=%d acc35=%02X%02X\n",
                      (int)imuBurst(0x35, b, 6), (int)imuBurst(0x35, b, 12), imuR8(0x36), imuR8(0x35));
      }
    } else if (id0f == 0x6A || id0f == 0x69 || id0f == 0x6C || id0f == 0x70) {
      imuType = 2;                         // LSM6DS3/DSL/DSO/DSV 家族（基础寄存器同构）
      imuW8(0x10, 0x7C);                   // CTRL1_XL 833Hz ±8g
      imuW8(0x11, 0x7C);                   // CTRL2_G 833Hz ±2000dps
    } else if (id0f == 0x11 || id0f == 0x33) {   // SC7A20 / LIS2DH 系列
      imuW8(0x20, 0x57);                   // CTRL_REG1 100Hz 三轴使能
      imuW8(0x23, 0x00);                   // CTRL_REG4 ±2g
      imuType = 3;
    } else if (id01 == 0x13) {             // BMA423
      imuW8(0x7E, 0xB6);                   // CMD 软复位
      delay(5);
      imuW8(0x40, 0xA8);                   // ACC_CONF 100Hz 连续模式
      imuW8(0x7C, 0x04);                   // PWR_CTRL 加速度使能
      imuType = 4;
    } else if (id00 == 0x92) {             // BMA400
      imuW8(0x40, 0x22);                   // ACC_CONFIG0 100Hz + normal 模式
      imuType = 7;
    } else if (id75 == 0x68) {             // MPU6050
      imuW8(0x6B, 0x00);                   // 唤醒
      imuW8(0x1C, 0x10);                   // 加速度 ±8g
      imuW8(0x1B, 0x10);                   // 陀螺 ±1000dps
      imuType = 5;
    } else if (id75 == 0x67) {             // ICM-42670
      imuW8(0x1F, 0x0F);                   // PWR_MGMT0 加速度+陀螺 LN 模式
      imuW8(0x4A, 0x06);                   // ACCEL_CONFIG0
      imuType = 6;
    } else { imuAddr = 0; continue; }
    delay(80);                             // 等首批数据
    float g, dps;
    bool ok = false;
    for (int i = 0; i < 6; i++) {          // 自检：能读到数，且含重力合加速度在合理区间
      if (imuPoll(g, dps) && imuRawG > 0.25f && imuRawG < 1.9f) { ok = true; break; }
      if (i == 0 || i == 5)
        Serial.printf("IMU selfcheck 0x%02X try%d: rawG=%.3f dps=%.1f\n", addr, i, imuRawG, dps);
      delay(30);
    }
    if (ok) { imuOK = true; break; }
    imuType = 0; imuAddr = 0; imuGInit = false;
  }
  const char *names[] = {"", "QMI8658", "LSM6DSx", "SC7A20/LIS2DH", "BMA423", "MPU6050", "ICM-42670", "BMA400"};
  Serial.printf("IMU: %s (type %d addr 0x%02X)\n",
                imuOK ? names[imuType] : "not found", imuType, imuAddr);
}
