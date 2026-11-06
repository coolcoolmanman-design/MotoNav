#pragma once
#include <cstdint>

// NimBLE gap types — only needed when CONFIG_NIMBLE_ENABLED is defined.
// HOST_SIM leaves CONFIG_NIMBLE_ENABLED undefined; these exist for accidental includes.

#ifndef BLE_GAP_LE_PHY_1M_MASK
#define BLE_GAP_LE_PHY_1M_MASK 0x01
#endif
#ifndef BLE_GAP_LE_PHY_2M_MASK
#define BLE_GAP_LE_PHY_2M_MASK 0x02
#endif

struct ble_gap_sec_state {
  unsigned encrypted : 1;
  unsigned authenticated : 1;
  unsigned bonded : 1;
  unsigned key_size : 5;
};

struct ble_gap_conn_desc {
  uint16_t conn_handle;
  ble_gap_sec_state sec_state;
};

struct ble_gap_event {
  uint8_t type;
  union {
    struct {
      uint16_t conn_handle;
    } connect;
    struct {
      uint16_t conn_handle;
    } disconnect;
  };
};

inline int ble_gap_set_prefered_default_le_phy(uint8_t, uint8_t) { return 0; }
inline int ble_gap_write_sugg_def_data_len(uint16_t, uint16_t) { return 0; }
