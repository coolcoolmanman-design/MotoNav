#pragma once
#include <cstdint>

class BLESecurityCallbacks {
public:
  virtual ~BLESecurityCallbacks() = default;
  virtual uint32_t onPassKeyRequest() { return 0; }
  virtual void onPassKeyNotify(uint32_t pass_key) { (void)pass_key; }
  virtual bool onSecurityRequest() { return true; }
  virtual void onAuthenticationComplete(/* ble_gap_conn_desc* */ void *) {}
  virtual bool onConfirmPIN(uint32_t pin) { (void)pin; return true; }
  virtual bool onAuthorizationRequest(uint16_t connHandle, uint16_t attrHandle, bool isRead) {
    (void)connHandle; (void)attrHandle; (void)isRead;
    return true;
  }
};

class BLESecurity {
public:
  BLESecurity() = default;
  void setAuthenticationMode(bool bonding, bool mitm, bool sc) {
    (void)bonding; (void)mitm; (void)sc;
  }
  void setCapability(uint8_t capability) { (void)capability; }
  void setInitEncryptionKey(uint8_t) {}
  void setRespEncryptionKey(uint8_t) {}
  void setKeySize(uint8_t) {}
  void setStaticPIN(uint32_t) {}
  void setPassKey(bool staticPass) { (void)staticPass; }
  void regenPassKeyOnConnect(bool v) { (void)v; }
};
