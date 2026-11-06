// HOST_SIM firmware amalgamation — compiles the real boards/175/MotoNav entry
// (same include order as MotoNav.ino) against mac/FirmwareHost stubs.
#ifndef HOST_SIM
#define HOST_SIM 1
#endif

#include <Arduino.h>
#include <cstdint>
#include <cstring>
#include <cstdio>

#include "host_present.h"
#include "host_tcp.h"

// Present hook used by DisplayPipeline::flushTask under HOST_SIM.
static const uint16_t *gPresentedFb = nullptr;
static int gPresentedW = HOST_LCD_W;
static int gPresentedH = HOST_LCD_H;

extern "C" void hostSimPresentFb(const uint16_t *fb) {
  gPresentedFb = fb;
  if (fb) hostPresentFrame(fb, gPresentedW, gPresentedH);
}

extern "C" void hostSimSetFramebuffer(uint16_t *fb, int w, int h);

// Bring in the real firmware translation unit (header-only modules + setup/loop).
#include "MotoNav.ino"

// TCP → firmware rx queue (same path as BLE RxCB::onWrite).
extern "C" void hostRxEnqueue(const uint8_t *data, size_t len) {
  if (!data || len == 0 || powerOffPending) return;
  if (len > RXQ_MAX) len = RXQ_MAX;
  // Any inbound traffic clears App-bye (matches device: new write implies App present).
  appBye = false;
  portENTER_CRITICAL(&mux);
  int next = (rxHead + 1) % RXQ_SLOTS;
  if (next == rxTail) rxTail = (rxTail + 1) % RXQ_SLOTS;
  rxq[rxHead].len = (uint16_t)len;
  std::memcpy(rxq[rxHead].data, data, len);
  rxHead = next;
  portEXIT_CRITICAL(&mux);
}

// RoundSim/DEBUG iOS TCP session ≈ authenticated App link.
extern "C" void hostBleOnClientConnect() {
  bleConnected = true;
  bleAuthed = true;
  txSubscribed = true;
  appBye = false;
  bleConnAt = millis();
  bleAuthAt = millis();
  ++mediaConnectionEpoch;
  lastRxMs = millis();
  std::printf("HOST_SIM: TCP client → bleConnected/authed/txSubscribed\n");
}

extern "C" void hostBleOnClientDisconnect() {
  bleConnected = false;
  bleAuthed = false;
  txSubscribed = false;
  ++mediaConnectionEpoch;
  std::printf("HOST_SIM: TCP client gone\n");
}

// Expose live canvas to host_main's present path as a fallback when flush hasn't run.
extern "C" uint16_t *hostSimLiveFramebuffer(int *w, int *h) {
  if (w) *w = LCD_WIDTH;
  if (h) *h = LCD_HEIGHT;
  if (gPresentedFb) return const_cast<uint16_t *>(gPresentedFb);
  return gfx ? gfx->getFramebuffer() : nullptr;
}
