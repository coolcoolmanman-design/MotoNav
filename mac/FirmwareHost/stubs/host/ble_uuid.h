#pragma once
#include <cstdint>
struct ble_uuid_t { uint8_t type; };
struct ble_uuid16_t { ble_uuid_t u; uint16_t value; };
struct ble_uuid128_t { ble_uuid_t u; uint8_t value[16]; };
