#pragma once
#include <cstdint>

#ifndef BLE_STORE_OBJ_TYPE_OUR_SEC
#define BLE_STORE_OBJ_TYPE_OUR_SEC 1
#endif
#ifndef BLE_STORE_OBJ_TYPE_PEER_SEC
#define BLE_STORE_OBJ_TYPE_PEER_SEC 2
#endif

inline int ble_store_util_count(int /*type*/, int *out) {
  if (out) *out = 0;
  return 0;
}
inline int ble_store_clear() { return 0; }
