#pragma once
#include <cstdint>
#include <cstring>
#include "esp_system.h"

typedef enum {
  ESP_MAC_WIFI_STA = 0,
  ESP_MAC_WIFI_SOFTAP,
  ESP_MAC_BT,
  ESP_MAC_ETH,
  ESP_MAC_IEEE802154,
  ESP_MAC_BASE,
  ESP_MAC_EFUSE_FACTORY,
  ESP_MAC_EFUSE_CUSTOM
} esp_mac_type_t;

inline esp_err_t esp_read_mac(uint8_t *mac, esp_mac_type_t type) {
  if (!mac) return ESP_FAIL;
  (void)type;
  // Stable fake BT MAC for DeviceName / bleName.
  static const uint8_t kMac[6] = {0x02, 0x00, 0x00, 0x11, 0x75, 0xA1};
  std::memcpy(mac, kMac, 6);
  return ESP_OK;
}
