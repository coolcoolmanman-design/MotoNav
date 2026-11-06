#pragma once
#include <cstdint>
#include <cstring>
#include <vector>
#include <string>
#include "BLEUUID.h"

class BLECharacteristic;
class BLEDescriptor;

enum Status {
  SUCCESS_INDICATE,
  SUCCESS_NOTIFY,
  ERROR_INDICATE_DISABLED,
  ERROR_NOTIFY_DISABLED,
  ERROR_GATT,
  ERROR_NO_CLIENT,
  ERROR_INDICATE_TIMEOUT,
  ERROR_INDICATE_FAILURE
};

class BLECharacteristicCallbacks {
public:
  virtual ~BLECharacteristicCallbacks() = default;
  virtual void onRead(BLECharacteristic *pCharacteristic) { (void)pCharacteristic; }
  virtual void onWrite(BLECharacteristic *pCharacteristic) { (void)pCharacteristic; }
  virtual void onNotify(BLECharacteristic *pCharacteristic) { (void)pCharacteristic; }
  virtual void onStatus(BLECharacteristic *pCharacteristic, Status s, uint32_t code) {
    (void)pCharacteristic; (void)s; (void)code;
  }
};

class BLEDescriptor {
  BLEUUID uuid_;
  std::vector<uint8_t> value_;
public:
  explicit BLEDescriptor(BLEUUID uuid) : uuid_(uuid) {}
  virtual ~BLEDescriptor() = default;
  BLEUUID getUUID() const { return uuid_; }
  void setValue(const uint8_t *data, size_t size) {
    if (!data) { value_.clear(); return; }
    value_.assign(data, data + size);
  }
  void setValue(const std::string &v) { value_.assign(v.begin(), v.end()); }
};

class BLECharacteristic {
public:
  static const uint32_t PROPERTY_READ = 1u << 0;
  static const uint32_t PROPERTY_WRITE = 1u << 1;
  static const uint32_t PROPERTY_NOTIFY = 1u << 2;
  static const uint32_t PROPERTY_BROADCAST = 1u << 3;
  static const uint32_t PROPERTY_INDICATE = 1u << 4;
  static const uint32_t PROPERTY_WRITE_NR = 1u << 5;
  static const uint32_t PROPERTY_WRITE_ENC = 1u << 6;
  static const uint32_t PROPERTY_WRITE_AUTHEN = 1u << 7;
  static const uint32_t PROPERTY_READ_ENC = 1u << 8;
  static const uint32_t PROPERTY_READ_AUTHEN = 1u << 9;

  BLECharacteristic(const char *uuid, uint32_t properties = 0)
      : uuid_(uuid), properties_(properties) {}
  BLECharacteristic(BLEUUID uuid, uint32_t properties = 0)
      : uuid_(uuid), properties_(properties) {}
  virtual ~BLECharacteristic() = default;

  BLEUUID getUUID() const { return uuid_; }
  void setCallbacks(BLECharacteristicCallbacks *cb) { callbacks_ = cb; }
  BLECharacteristicCallbacks *getCallbacks() const { return callbacks_; }

  void setValue(const uint8_t *data, size_t size) {
    if (!data) { value_.clear(); return; }
    value_.assign(data, data + size);
  }
  void setValue(const std::string &v) { value_.assign(v.begin(), v.end()); }
  void setValue(uint8_t v) { setValue(&v, 1); }
  void setValue(uint16_t v) {
    uint8_t b[2] = {(uint8_t)(v & 0xff), (uint8_t)(v >> 8)};
    setValue(b, 2);
  }
  void setValue(uint32_t v) {
    uint8_t b[4] = {(uint8_t)(v), (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24)};
    setValue(b, 4);
  }
  void setValue(const char *s) { if (s) setValue(std::string(s)); }

  uint8_t *getData() { return value_.empty() ? nullptr : value_.data(); }
  size_t getLength() const { return value_.size(); }
  std::string getValue() const { return std::string(value_.begin(), value_.end()); }

  void notify() {
    // HOST_SIM: forward GATT notify payload over RoundSim TCP framing.
    extern bool hostTcpSend(const uint8_t *data, size_t len);
    if (!value_.empty()) hostTcpSend(value_.data(), value_.size());
    if (callbacks_) callbacks_->onStatus(this, SUCCESS_NOTIFY, 0);
  }
  void indicate() {}
  void addDescriptor(BLEDescriptor *d) { if (d) descriptors_.push_back(d); }
  BLEDescriptor *getDescriptor(BLEUUID uuid) {
    for (auto *d : descriptors_) if (d->getUUID() == uuid) return d;
    return nullptr;
  }

  // Host inject: simulate a phone write into this characteristic.
  void hostInjectWrite(const uint8_t *data, size_t len) {
    setValue(data, len);
    if (callbacks_) callbacks_->onWrite(this);
  }

private:
  BLEUUID uuid_;
  uint32_t properties_ = 0;
  std::vector<uint8_t> value_;
  std::vector<BLEDescriptor *> descriptors_;
  BLECharacteristicCallbacks *callbacks_ = nullptr;
};
