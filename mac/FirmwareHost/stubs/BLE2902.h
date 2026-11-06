#pragma once
#include "BLECharacteristic.h"

class BLE2902 : public BLEDescriptor {
public:
  BLE2902() : BLEDescriptor(BLEUUID((uint16_t)0x2902)) {
    uint8_t v[2] = {0, 0};
    setValue(v, 2);
  }
  void setNotifications(bool enable) {
    uint8_t v[2] = {(uint8_t)(enable ? 1 : 0), 0};
    setValue(v, 2);
  }
  bool getNotifications() const { return false; }
  void setIndications(bool) {}
};
