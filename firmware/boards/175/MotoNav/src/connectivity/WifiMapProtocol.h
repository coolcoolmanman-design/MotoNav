#pragma once
// musicA.111：SoftAP 控制面协议（可 host 单测，无 Arduino/WiFi 依赖）。
// 契约：
//  - enable 后成功 → 状态 "WIFI,1,<ssid>,<psk>,<ip>,<port>"
//  - 重试中 query → 不产出 WIFI,0,err（空串 = 尚无终态）
//  - 用尽失败 → "WIFI,0,err"
//  - disable → "WIFI,0"
// App 侧「无法打开圆屏热点」对应终态 err 或超时未收到 WIFI,1。

#include <cstdint>
#include <cstring>
#include <string>

struct WifiMapProtocol {
  bool wanted = false;
  bool running = false;
  bool startFailed = false;
  uint8_t tries = 0;
  uint8_t maxTries = 6;
  uint32_t nextTryMs = 0;
  std::string pendingStatus;   // 待 BLE 回执；空 = 无新状态
  std::string ssid = "MotoNav-TEST";
  std::string psk = "motoNavTEST";
  std::string ip = "192.168.4.1";
  uint16_t tcpPort = 7443;

  void clearStatus() { pendingStatus.clear(); }

  void queueRunning() {
    pendingStatus = "WIFI,1," + ssid + "," + psk + "," + ip + "," + std::to_string(tcpPort);
  }
  void queueOff() { pendingStatus = "WIFI,0"; }
  void queueErr() { pendingStatus = "WIFI,0,err"; }

  // alreadyUp：射频 SoftAP 已暖启动成功，只需标 running 并回执。
  void setEnabled(bool on, uint32_t nowMs, bool alreadyUp) {
    wanted = on;
    if (on) {
      startFailed = false;
      tries = 1;
      nextTryMs = nowMs;           // 允许立刻 try
      if (alreadyUp) {
        running = true;
        queueRunning();
      } else {
        running = false;
        clearStatus();             // 重试中不报 err
      }
    } else {
      running = false;
      startFailed = false;
      tries = 0;
      nextTryMs = 0;
      queueOff();
    }
  }

  void query() {
    if (running) queueRunning();
    else if (!wanted) queueOff();
    else if (startFailed) queueErr();
    // else：重试中 → 保持 pending 为空，避免 App 误判
  }

  // bringUp() 返回是否 SoftAP 已可用（含 IP）。成功后调用方再起 HTTP/TCP。
  template <typename BringUpFn>
  void tick(uint32_t nowMs, BringUpFn bringUp) {
    if (wanted && !running && !startFailed) {
      if (nowMs >= nextTryMs && tries <= maxTries) {
        bool ok = bringUp();
        if (ok) {
          running = true;
          startFailed = false;
          queueRunning();
          return;
        }
        if (tries >= maxTries) {
          startFailed = true;
          queueErr();
          return;
        }
        nextTryMs = nowMs + 280u * tries;
        tries++;
        clearStatus();
      }
      return;
    }
  }
};
