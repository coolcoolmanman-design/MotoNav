// Host-side SoftAP control-plane proof (no ESP radio).
// Mirrors musicA.111 WifiMapProtocol contract used by WifiMapReceiver:
//   #WIFI,1 path must yield WIFI,1,... before App times out;
//   #WIFI? during retry must NOT return WIFI,0,err;
//   only exhausted failure returns WIFI,0,err.
#include "../boards/175/MotoNav/src/connectivity/WifiMapProtocol.h"
#include <cassert>
#include <iostream>
#include <string>

static int checks = 0;
static void expect(bool cond, const char *msg) {
  if (!cond) {
    std::cerr << "FAIL: " << msg << "\n";
    std::abort();
  }
  ++checks;
}

int main() {
  // 1) Warm SoftAP already up → enable immediately returns WIFI,1
  {
    WifiMapProtocol p;
    p.setEnabled(true, /*now*/0, /*alreadyUp*/true);
    expect(p.running, "warm enable running");
    expect(p.pendingStatus.rfind("WIFI,1,", 0) == 0, "warm enable WIFI,1");
    expect(p.pendingStatus.find("MotoNav-TEST") != std::string::npos, "ssid in status");
    expect(p.pendingStatus.find(",7443") != std::string::npos, "port in status");
  }

  // 2) Cold bring-up succeeds on first tick
  {
    WifiMapProtocol p;
    p.setEnabled(true, 0, false);
    expect(p.pendingStatus.empty(), "retrying: no status yet");
    p.query();
    expect(p.pendingStatus.empty(), "query while retrying yields no err");
    int calls = 0;
    p.tick(0, [&]() {
      ++calls;
      return true;
    });
    expect(calls == 1, "bringUp called once");
    expect(p.running, "running after success");
    expect(p.pendingStatus.rfind("WIFI,1,", 0) == 0, "success WIFI,1");
    p.clearStatus();
    p.query();
    expect(p.pendingStatus.rfind("WIFI,1,", 0) == 0, "query after up WIFI,1");
  }

  // 3) Fail then succeed — never err mid-retry
  {
    WifiMapProtocol p;
    p.maxTries = 3;
    p.setEnabled(true, 0, false);
    int n = 0;
    p.tick(0, [&]() {
      ++n;
      return false;
    });
    expect(!p.running && !p.startFailed, "first fail still retrying");
    expect(p.pendingStatus.empty(), "no err after first fail");
    p.query();
    expect(p.pendingStatus.empty(), "query mid-retry empty");
    // advance clock past backoff
    p.tick(1000, [&]() {
      ++n;
      return true;
    });
    expect(p.running, "succeeds on later try");
    expect(p.pendingStatus.rfind("WIFI,1,", 0) == 0, "later success WIFI,1");
  }

  // 4) Exhaust retries → WIFI,0,err
  {
    WifiMapProtocol p;
    p.maxTries = 2;
    p.setEnabled(true, 0, false);
    p.tick(0, []() { return false; });
    expect(!p.startFailed, "not failed after try1");
    p.tick(500, []() { return false; });
    expect(p.startFailed, "failed after maxTries");
    expect(p.pendingStatus == "WIFI,0,err", "exhausted WIFI,0,err");
    p.clearStatus();
    p.query();
    expect(p.pendingStatus == "WIFI,0,err", "query after fail still err");
  }

  // 5) Disable → WIFI,0；BLE-only path intact
  {
    WifiMapProtocol p;
    p.setEnabled(true, 0, true);
    p.setEnabled(false, 10, false);
    expect(!p.wanted && !p.running, "disabled");
    expect(p.pendingStatus == "WIFI,0", "disable WIFI,0");
    p.query();
    expect(p.pendingStatus == "WIFI,0", "query off WIFI,0");
  }

  // 6) Simulate App timeline: enable → ticks → must see WIFI,1 within window
  {
    WifiMapProtocol p;
    p.maxTries = 6;
    p.setEnabled(true, 0, false);
    bool got = false;
    for (uint32_t t = 0; t < 20000; t += 100) {
      p.tick(t, [&]() {
        // succeed on 3rd attempt (warm-path stand-in)
        return p.tries >= 3;
      });
      if (p.pendingStatus.rfind("WIFI,1,", 0) == 0) {
        got = true;
        break;
      }
      // App must not treat empty mid-retry as fatal
      if (!p.pendingStatus.empty() && p.pendingStatus == "WIFI,0,err") break;
    }
    expect(got, "App window receives WIFI,1 before timeout");
  }

  std::cout << "WifiMap bring-up protocol: " << checks << " checks passed\n";
  return 0;
}
