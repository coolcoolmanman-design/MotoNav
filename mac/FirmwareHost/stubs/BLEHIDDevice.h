#pragma once
#include <cstdint>
#include "BLEServer.h"
#include "BLEUUID.h"
#include "BLECharacteristic.h"

class BLEHIDDevice {
  BLEServer *server_ = nullptr;
  BLEService *hidService_ = nullptr;
  BLECharacteristic *input_ = nullptr;
  BLECharacteristic *control_ = nullptr;
  BLECharacteristic *protocol_ = nullptr;
  BLECharacteristic *manufacturer_ = nullptr;
  BLEService *deviceInfo_ = nullptr;
  BLEService *battery_ = nullptr;
  uint8_t batteryLevel_ = 100;
public:
  explicit BLEHIDDevice(BLEServer *server) : server_(server) {
    if (server_) {
      hidService_ = server_->createService(BLEUUID((uint16_t)0x1812));
      deviceInfo_ = server_->createService(BLEUUID((uint16_t)0x180A));
      battery_ = server_->createService(BLEUUID((uint16_t)0x180F));
      manufacturer_ = deviceInfo_->createCharacteristic(
          BLEUUID((uint16_t)0x2A29), BLECharacteristic::PROPERTY_READ);
      deviceInfo_->createCharacteristic(
          BLEUUID((uint16_t)0x2A50), BLECharacteristic::PROPERTY_READ);
      control_ = hidService_->createCharacteristic(
          BLEUUID((uint16_t)0x2A4C), BLECharacteristic::PROPERTY_WRITE);
      protocol_ = hidService_->createCharacteristic(
          BLEUUID((uint16_t)0x2A4E),
          BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
    }
  }

  BLECharacteristic *manufacturer() { return manufacturer_; }
  BLEService *deviceInfo() { return deviceInfo_; }
  BLECharacteristic *hidControl() { return control_; }
  BLECharacteristic *protocolMode() { return protocol_; }

  void hidInfo(uint8_t /*country*/, uint8_t /*flags*/) {}
  void reportMap(uint8_t *map, uint16_t size) { (void)map; (void)size; }
  BLECharacteristic *inputReport(uint8_t /*reportId*/) {
    if (!input_ && hidService_) {
      input_ = hidService_->createCharacteristic(
          BLEUUID((uint16_t)0x2A4D),
          BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY |
              BLECharacteristic::PROPERTY_WRITE);
    }
    return input_;
  }
  void setBatteryLevel(uint8_t level) { batteryLevel_ = level; }
  void startServices() {
    if (hidService_) hidService_->start();
    if (deviceInfo_) deviceInfo_->start();
    if (battery_) battery_->start();
  }
};
