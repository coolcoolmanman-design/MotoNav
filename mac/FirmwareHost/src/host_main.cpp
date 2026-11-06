#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "host_present.h"
#include "host_tcp.h"
#include "host_touch.h"

void setup();
void loop();

extern "C" uint16_t *hostSimLiveFramebuffer(int *w, int *h);
extern "C" void hostSimSetFramebuffer(uint16_t *fb, int w, int h) {
  (void)fb;
  (void)w;
  (void)h;
  // Present path is driven by DisplayPipeline::flushTask → hostSimPresentFb.
}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  std::printf("FirmwareHost HOST_SIM starting (real boards/175/MotoNav sources)\n");

  if (!hostPresentInit(HOST_LCD_W, HOST_LCD_H)) {
    std::fprintf(stderr, "hostPresentInit failed\n");
    return 1;
  }
  if (!hostTcpStart("127.0.0.1", HOST_TCP_PORT)) {
    std::fprintf(stderr, "hostTcpStart failed (is port %d free?)\n", HOST_TCP_PORT);
    hostPresentShutdown();
    return 1;
  }

  setup();

  const char *framesEnv = std::getenv("FH_HOST_FRAMES");
  int framesLeft = framesEnv ? std::atoi(framesEnv) : -1;

  while (!hostPresentPollQuit()) {
    hostTcpPoll();
    loop();

    int w = HOST_LCD_W, h = HOST_LCD_H;
    if (uint16_t *fb = hostSimLiveFramebuffer(&w, &h)) {
      hostPresentFrame(fb, w, h);
    }

    // Pump mouse → CST inject when SDL is present (headless: no-op).
    hostPresentPumpTouch();

    hostPresentDelay(16);
    if (framesLeft > 0) {
      if (--framesLeft == 0) break;
    }
  }

  hostTcpStop();
  hostPresentShutdown();
  std::printf("FirmwareHost exit\n");
  return 0;
}
