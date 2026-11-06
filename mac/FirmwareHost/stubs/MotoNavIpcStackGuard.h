#pragma once
#include <stdint.h>

// HOST_SIM override of firmware/libraries/MotoNavIpcStackGuard — no-op status.
struct MotoNavIpcStackStatus {
  uint32_t bytes[2];
  uint32_t freeMinimum[2];
};

inline MotoNavIpcStackStatus motoNavIpcStackStatus() {
  MotoNavIpcStackStatus s{};
  s.bytes[0] = 4096;
  s.bytes[1] = 4096;
  s.freeMinimum[0] = 4096;
  s.freeMinimum[1] = 4096;
  return s;
}
