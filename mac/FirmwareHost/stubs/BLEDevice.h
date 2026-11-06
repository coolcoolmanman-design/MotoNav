#pragma once
#include <cstdint>
#include <string>
#include <functional>
#include "BLEServer.h"
#include "BLEUUID.h"
#include "BLESecurity.h"
#include "BLEUtils.h"

class BLEDevice {
  static inline BLEServer *server_;
  static inline BLEAdvertising advertising_;
  static inline BLESecurityCallbacks *secCb_;
  static inline std::string name_;
  static inline uint16_t mtu_;
  static inline void (*gapHandler_)(struct ble_gap_event *, void *);

public:
  static void init(const std::string &deviceName) {
    name_ = deviceName;
    if (!server_) server_ = new BLEServer();
  }
  static void init(const char *deviceName) { init(std::string(deviceName ? deviceName : "")); }
  static void deinit(bool /*releaseMemory*/ = false) {}
  static BLEServer *createServer() {
    if (!server_) server_ = new BLEServer();
    return server_;
  }
  static BLEServer *getServer() { return server_; }
  static BLEAdvertising *getAdvertising() { return &advertising_; }
  static void startAdvertising() { advertising_.start(); }
  static void stopAdvertising() { advertising_.stop(); }
  static void setMTU(uint16_t mtu) { mtu_ = mtu; }
  static uint16_t getMTU() { return mtu_ ? mtu_ : 23; }
  static void setPower(int /*power*/) {}
  static void setCustomGapHandler(void (*handler)(struct ble_gap_event *, void *)) {
    gapHandler_ = handler;
  }
  static void setSecurityCallbacks(BLESecurityCallbacks *cb) { secCb_ = cb; }
  static BLESecurityCallbacks *getSecurityCallbacks() { return secCb_; }
  static const char *getName() { return name_.c_str(); }
};
