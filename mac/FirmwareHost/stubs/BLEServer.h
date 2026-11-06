#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include "BLEUUID.h"
#include "BLECharacteristic.h"

class BLEServer;
class BLEService;

class BLEServerCallbacks {
public:
  virtual ~BLEServerCallbacks() = default;
  virtual void onConnect(BLEServer *pServer) { (void)pServer; }
  virtual void onDisconnect(BLEServer *pServer) { (void)pServer; }
};

class BLEService {
  BLEUUID uuid_;
  std::vector<BLECharacteristic *> chars_;
  bool started_ = false;
public:
  explicit BLEService(BLEUUID uuid) : uuid_(uuid) {}
  BLECharacteristic *createCharacteristic(const char *uuid, uint32_t properties) {
    auto *c = new BLECharacteristic(uuid, properties);
    chars_.push_back(c);
    return c;
  }
  BLECharacteristic *createCharacteristic(BLEUUID uuid, uint32_t properties) {
    auto *c = new BLECharacteristic(uuid, properties);
    chars_.push_back(c);
    return c;
  }
  BLECharacteristic *getCharacteristic(BLEUUID uuid) {
    for (auto *c : chars_) if (c->getUUID() == uuid) return c;
    return nullptr;
  }
  void start() { started_ = true; }
  void stop() { started_ = false; }
  BLEUUID getUUID() const { return uuid_; }
};

class BLEAdvertising {
public:
  void addServiceUUID(const char *uuid) { (void)uuid; }
  void addServiceUUID(BLEUUID uuid) { (void)uuid; }
  void setAppearance(uint16_t) {}
  void setScanResponse(bool) {}
  void setMinPreferred(uint16_t) {}
  void start() {}
  void stop() {}
};

class BLEServer {
  BLEServerCallbacks *callbacks_ = nullptr;
  std::vector<BLEService *> services_;
  BLEAdvertising adv_;
public:
  void setCallbacks(BLEServerCallbacks *cb) { callbacks_ = cb; }
  BLEService *createService(const char *uuid) {
    auto *s = new BLEService(BLEUUID(uuid));
    services_.push_back(s);
    return s;
  }
  BLEService *createService(BLEUUID uuid) {
    auto *s = new BLEService(uuid);
    services_.push_back(s);
    return s;
  }
  BLEAdvertising *getAdvertising() { return &adv_; }
  void startAdvertising() { adv_.start(); }
  uint16_t getConnectedCount() { return connected_ ? 1 : 0; }

  // Host helpers
  void hostConnect() {
    connected_ = true;
    if (callbacks_) callbacks_->onConnect(this);
  }
  void hostDisconnect() {
    connected_ = false;
    if (callbacks_) callbacks_->onDisconnect(this);
  }
  BLEService *hostFindService(BLEUUID uuid) {
    for (auto *s : services_) if (s->getUUID() == uuid) return s;
    return nullptr;
  }

private:
  bool connected_ = false;
};
