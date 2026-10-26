#pragma once
// v7.12.9-ui.9 IMU 驱动：圆屏本体传感器，运行时探测芯片型号。
// 这类 AXP2101 + 1.75 圆屏的板子常见 QMI8658 六轴（0x6B），少数用 LSM6DSL（0x6A）
// 或 SC7A20 三轴加速度（0x19）。探测到哪种用哪种；都探测不到则 imuOK=false，
// G 页退回手机 #G 通道 / 模拟数据。
// 依赖 Wire（MotoNav.ino 已 Wire.begin(IIC_SDA, IIC_SCL)）；由 Battery.h 之后包含，仅编译一次。

bool    imuOK = false;
uint8_t imuType = 0;                       // 1=QMI8658 2=LSM6DSL 3=SC7A20
uint8_t imuAddr = 0;

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

// 一次采样：g = 加速度合矢量模长（含重力，静止约 1.0）；dps = 角速度模长（无陀螺则 -1）
bool imuPoll(float &g, float &dps) {
  g = 0; dps = -1;
  if (!imuOK) return false;
  uint8_t b[12];
  if (imuType == 1) {                      // QMI8658：加速度 0x35.. 陀螺 0x3B..，一次连读 12 字节
    if (!imuBurst(0x35, b, 12)) return false;
    float ax = (int16_t)(b[1] << 8 | b[0]) / 2048.0f;   // ±8g
    float ay = (int16_t)(b[3] << 8 | b[2]) / 2048.0f;
    float az = (int16_t)(b[5] << 8 | b[4]) / 2048.0f;
    float rx = (int16_t)(b[7] << 8 | b[6]) / 64.0f;     // ±512 dps
    float ry = (int16_t)(b[9] << 8 | b[8]) / 64.0f;
    float rz = (int16_t)(b[11] << 8 | b[10]) / 64.0f;
    g = sqrtf(ax * ax + ay * ay + az * az);
    dps = sqrtf(rx * rx + ry * ry + rz * rz);
    return true;
  }
  if (imuType == 2) {                      // LSM6DSL：陀螺 0x22.. 加速度 0x28..，各读 6 字节
    if (!imuBurst(0x28, b, 6)) return false;
    float ax = (int16_t)(b[1] << 8 | b[0]) * 0.000122f; // ±8g
    float ay = (int16_t)(b[3] << 8 | b[2]) * 0.000122f;
    float az = (int16_t)(b[5] << 8 | b[4]) * 0.000122f;
    g = sqrtf(ax * ax + ay * ay + az * az);
    if (imuBurst(0x22, b, 6)) {
      float rx = (int16_t)(b[1] << 8 | b[0]) * 0.070f;  // ±2000 dps
      float ry = (int16_t)(b[3] << 8 | b[2]) * 0.070f;
      float rz = (int16_t)(b[5] << 8 | b[4]) * 0.070f;
      dps = sqrtf(rx * rx + ry * ry + rz * rz);
    }
    return true;
  }
  // SC7A20：三轴加速度，12 位左对齐，±2g 量程 1g=1024
  if (!imuBurst(0x28, b, 6)) return false;
  float ax = ((int16_t)(b[1] << 8 | b[0]) >> 4) / 1024.0f;
  float ay = ((int16_t)(b[3] << 8 | b[2]) >> 4) / 1024.0f;
  float az = ((int16_t)(b[5] << 8 | b[4]) >> 4) / 1024.0f;
  g = sqrtf(ax * ax + ay * ay + az * az);
  return true;
}

// 开机调一次（Wire.begin 之后）。探测 + 初始化 + 自检（静止合加速度须约 1g）。
void imuInit() {
  const uint8_t addrs[] = {0x6B, 0x6A, 0x19};
  for (uint8_t addr : addrs) {
    imuAddr = addr;
    // WHO_AM_I：QMI8658 reg0x00=0x05；LSM6DSL/SC7A20 reg0x0F=0x6A/0x11
    int id0 = imuR8(0x00), id0f = imuR8(0x0F);
    if (id0 == 0x05) {                     // QMI8658
      imuW8(0x02, 0x60);                   // CTRL1：地址自增
      imuW8(0x03, 0x25);                   // CTRL2 加速度 ±8g
      imuW8(0x04, 0x65);                   // CTRL3 陀螺 ±512dps
      imuW8(0x06, 0x03);                   // CTRL7 使能加速度+陀螺
      imuType = 1;
    } else if (id0f == 0x6A) {             // LSM6DSL
      imuW8(0x10, 0x7C);                   // CTRL1_XL 833Hz ±8g
      imuW8(0x11, 0x7C);                   // CTRL2_G 833Hz ±2000dps
      imuType = 2;
    } else if (id0f == 0x11) {             // SC7A20
      imuW8(0x20, 0x57);                   // CTRL_REG1 100Hz 三轴使能
      imuW8(0x23, 0x00);                   // CTRL_REG4 ±2g
      imuType = 3;
    } else { imuAddr = 0; continue; }
    delay(60);                             // 等首批数据
    float g, dps;
    bool ok = false;
    for (int i = 0; i < 5; i++) {          // 自检：合加速度在合理区间（静止约 1g）
      if (imuPoll(g, dps) && g > 0.25f && g < 1.9f) { ok = true; break; }
      delay(30);
    }
    if (ok) { imuOK = true; break; }
    imuType = 0; imuAddr = 0;              // 读到了 ID 但数据不对，按没探测到处理
  }
  Serial.printf("IMU: %s (type %d addr 0x%02X)\n",
                imuOK ? "detected" : "not found", imuType, imuAddr);
}
