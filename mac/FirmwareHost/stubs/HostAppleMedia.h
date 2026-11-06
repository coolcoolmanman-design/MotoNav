#pragma once
// HOST_SIM stand-in for AppleMediaClient — no NimBLE GATT client.
#include "WString.h"
#include <cstdint>
#include <cstddef>
#include <cstring>

// Reuse the real AMS value/playback helpers (board dir is on the include path).
#include "AppleMediaData.h"

class AppleMediaClient {
public:
  struct Value {
    String text;
    uint16_t revision = 1;
    bool partial = false;
  };
  Value values[7];
  bool available = false, changed = false;
  volatile uint32_t generation = 0;
  AppleMediaData::Playback playback;
  uint32_t playbackAt = 0;
  int controlAck = 0;

  void begin() {}
  void tick(bool, uint16_t, uint32_t, uint32_t) {}
  int controlMask() const { return 0; }
  void resend() { changed = true; }
  bool controlReady(int /*button*/) const { return false; }
  bool control(int /*button*/) { return false; }
  bool nextFrame(uint8_t *out, size_t &size, uint32_t /*now*/) {
    if (out) out[0] = 0;
    size = 0;
    return false;
  }
};
