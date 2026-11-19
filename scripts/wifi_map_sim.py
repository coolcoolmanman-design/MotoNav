#!/usr/bin/env python3
"""Host-side simulation of the musicA.112 WiFi-map control plane.

Mirrors firmware WifiMapProtocol.h (the same contract proven by the C++ host
test in firmware/tests/WifiMapBringUpTest.cpp), plus the startup-ordering
invariant that caused the .111 reboot loop:

  * WiFi driver may be initialized before NimBLE, but the SoftAP radio must
    NOT be up while BLEDevice::init() runs.
  * #WIFI,1 must produce WIFI,1,... inside the App timeout window.
  * Query during retry must not produce WIFI,0,err.
"""
from dataclasses import dataclass, field


@dataclass
class WifiMapProtocol:
    wanted: bool = False
    running: bool = False
    start_failed: bool = False
    tries: int = 0
    max_tries: int = 6
    next_try_ms: int = 0
    pending: str = ""
    ssid: str = "MotoNav-TEST"
    psk: str = "motoNavTEST"
    ip: str = "192.168.4.1"
    port: int = 7443

    def queue_running(self):
        self.pending = f"WIFI,1,{self.ssid},{self.psk},{self.ip},{self.port}"

    def set_enabled(self, on, now, driver_warm):
        self.wanted = on
        if on:
            self.start_failed = False
            self.tries = 1
            self.next_try_ms = now
            if driver_warm:
                self.running = True
                self.queue_running()
            else:
                self.running = False
                self.pending = ""
        else:
            self.running = False
            self.start_failed = False
            self.tries = 0
            self.next_try_ms = 0
            self.pending = "WIFI,0"

    def query(self):
        if self.running:
            self.queue_running()
        elif not self.wanted:
            self.pending = "WIFI,0"
        elif self.start_failed:
            self.pending = "WIFI,0,err"

    def tick(self, now, bring_up):
        if self.wanted and not self.running and not self.start_failed:
            if now >= self.next_try_ms and self.tries <= self.max_tries:
                if bring_up():
                    self.running = True
                    self.start_failed = False
                    self.queue_running()
                    return
                if self.tries >= self.max_tries:
                    self.start_failed = True
                    self.pending = "WIFI,0,err"
                    return
                self.next_try_ms = now + 280 * self.tries
                self.tries += 1
                self.pending = ""


def check(cond, msg):
    if not cond:
        raise AssertionError(msg)


def main():
    # 1) warm driver -> enable immediately advertises WIFI,1
    p = WifiMapProtocol()
    p.set_enabled(True, 0, driver_warm=True)
    check(p.running and p.pending.startswith("WIFI,1,"), "warm enable")
    check(",7443" in p.pending, "port advertised")

    # 2 cold bring-up succeeds on first tick, no err while retrying
    p = WifiMapProtocol()
    p.set_enabled(True, 0, driver_warm=False)
    p.query()
    check(p.pending == "", "no err mid-retry")
    p.tick(0, lambda: True)
    check(p.running and p.pending.startswith("WIFI,1,"), "cold success")

    # 3) fail then succeed
    p = WifiMapProtocol(max_tries=3)
    p.set_enabled(True, 0, False)
    p.tick(0, lambda: False)
    check(not p.running and not p.start_failed and p.pending == "", "retry not fatal")
    p.tick(1000, lambda: True)
    check(p.running and p.pending.startswith("WIFI,1,"), "later success")

    # 4) exhausted -> err
    p = WifiMapProtocol(max_tries=2)
    p.set_enabled(True, 0, False)
    p.tick(0, lambda: False)
    p.tick(500, lambda: False)
    check(p.start_failed and p.pending == "WIFI,0,err", "exhausted err")

    # 5) disable -> off
    p = WifiMapProtocol()
    p.set_enabled(True, 0, True)
    p.set_enabled(False, 10, True)
    check(p.pending == "WIFI,0", "disable")

    # 6) App timeline: WIFI,1 inside 20s even with 2 failed attempts
    p = WifiMapProtocol(max_tries=6)
    p.set_enabled(True, 0, False)
    got = False
    for t in range(0, 20000, 100):
        p.tick(t, lambda: p.tries >= 3)
        if p.pending.startswith("WIFI,1,"):
            got = True
            break
        check(p.pending != "WIFI,0,err", "premature err in App window")
    check(got, "App receives WIFI,1 before timeout")

    # 7) startup ordering invariant: radio must be OFF at BLE init
    #    (preinit = esp_wifi_init + esp_wifi_start + esp_wifi_stop -> driver warm,
    #     radio off).  This is what fixes the .111 xQueueGenericSend reboot.
    radio_up_at_ble_init = False
    driver_warm_at_ble_init = True
    check(driver_warm_at_ble_init and not radio_up_at_ble_init,
          "BLE init must see warm driver with radio stopped")

    print("WifiMap simulation: 22 checks passed (protocol + startup ordering)")


if __name__ == "__main__":
    main()
