#pragma once
// 试验：圆屏 SoftAP + TCP 整包收 type3。
// musicA.111 / FO-261 v3（0.16.2.100 实机仍「无法打开圆屏热点」后）：
//   .110 仅 preinit + esp_wifi_stop 不够：BLE 起来后再 softAP/start 仍失败。
//   本版改为 **暖 SoftAP**：BLEDevice::init 前就 softAP（hidden）并保持射频不 stop；
//   #WIFI,1 只做 unhide/重配 + 起 HTTP/TCP，并先回 WIFI,1 再起服务器。
//   #WIFI,0：停服务器并重新 hidden，不 deinit、不 esp_wifi_stop。
//   SoftAP 仍可选；默认不启用时手机看不到（hidden）。
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <esp_wifi.h>
#include <esp_heap_caps.h>

#ifndef WIFI_MAP_PSK_PREFIX
#define WIFI_MAP_PSK_PREFIX "motoNav"
#endif
#ifndef WIFI_MAP_TCP_PORT
#define WIFI_MAP_TCP_PORT 7443
#endif
#ifndef WIFI_MAP_START_TRIES
#define WIFI_MAP_START_TRIES 8
#endif

static bool wifiMapWanted = false;
static bool wifiMapRunning = false;       // 对 App 可见：服务器已起、可灌图
static bool wifiMapWarm = false;         // 暖 SoftAP 射频已在（可 hidden）
static bool wifiMapDriverReady = false;
static WebServer *wifiMapHttp = nullptr;
static WiFiServer *wifiMapTcp = nullptr;
static String wifiMapSsid = "";
static String wifiMapPsk = "";
static String wifiMapIp = "192.168.4.1";
String pendingWifiStatus = "";
static uint8_t wifiMapStartTries = 0;
static uint32_t wifiMapNextTryMs = 0;
static bool wifiMapStartFailed = false;

// 由 RadarEffects 之后提供的弱符号式回收钩子（FunctionDeclarations 声明）。
extern void wifiMapReclaimInternalHeap();

static void wifiMapLogHeap(const char *tag) {
  Serial.printf("WIFI MAP %s int_free=%u largest=%u spiram=%u warm=%d\n", tag,
                (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
                (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
                wifiMapWarm ? 1 : 0);
}

static void wifiMapBuildCreds() {
  uint8_t mac[6] = {0};
  esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
  char ssid[24];
  snprintf(ssid, sizeof(ssid), "MotoNav-%02X%02X", mac[4], mac[5]);
  wifiMapSsid = ssid;
  char psk[20];
  snprintf(psk, sizeof(psk), "%s%02X%02X", WIFI_MAP_PSK_PREFIX, mac[4], mac[5]);
  wifiMapPsk = psk;
}

static void wifiMapQueueStatus() {
  if (wifiMapRunning) {
    pendingWifiStatus = String("WIFI,1,") + wifiMapSsid + "," + wifiMapPsk + "," +
                        wifiMapIp + "," + String(WIFI_MAP_TCP_PORT);
  } else if (!wifiMapWanted) {
    pendingWifiStatus = "WIFI,0";
  } else if (wifiMapStartFailed) {
    pendingWifiStatus = "WIFI,0,err";
  }
  // 重试中：不发 err
}

static bool wifiMapApplyType3(int id, uint32_t total) {
  Bulk &b = bulks[3];
  if (!b.buf || total == 0 || total > b.cap) return false;
  b.id = id;
  b.total = total;
  b.parts = 1;
  b.got = 1;
  b.done = true;
  if (!parseRaster(b.buf, total)) {
    b.done = false;
    b.got = 0;
    memset(b.mask, 0, sizeof(b.mask));
    return false;
  }
  pendingAck = String("K,3,") + String(id);
  navDirty = true;
  return true;
}

enum { WM_IDLE = 0, WM_HDR, WM_PAYLOAD, WM_REPLY };
static int wifiMapState = WM_IDLE;
static WiFiClient wifiMapClient;
static uint8_t wifiMapHdr[10];
static uint32_t wifiMapHdrGot = 0;
static uint32_t wifiMapPayGot = 0;
static uint32_t wifiMapPayTotal = 0;
static int wifiMapPayId = 0;
static uint32_t wifiMapStartMs = 0;
static char wifiMapReply[24];

static void wifiMapFail(const char *code) {
  snprintf(wifiMapReply, sizeof(wifiMapReply), "E,%s\n", code);
  wifiMapState = WM_REPLY;
}

static void wifiMapDestroyServers() {
  if (wifiMapHttp) {
    wifiMapHttp->stop();
    delete wifiMapHttp;
    wifiMapHttp = nullptr;
  }
  if (wifiMapTcp) {
    wifiMapTcp->stop();
    delete wifiMapTcp;
    wifiMapTcp = nullptr;
  }
}

static bool wifiMapSoftApApply(bool hidden) {
  wifiMapBuildCreds();
  IPAddress ip(192, 168, 4, 1);
  IPAddress gw(192, 168, 4, 1);
  IPAddress mask(255, 255, 255, 0);
  WiFi.softAPConfig(ip, gw, mask);
  const int ch = hidden ? 1 : ((wifiMapStartTries & 1) ? 6 : 1);
  // softAP(ssid, psk, channel, ssid_hidden, max_connection)
  bool ok = WiFi.softAP(wifiMapSsid.c_str(), wifiMapPsk.c_str(), ch, hidden ? 1 : 0, hidden ? 1 : 2);
  if (!ok) {
    Serial.printf("WIFI MAP softAP apply fail hidden=%d try=%u ch=%d\n",
                  hidden ? 1 : 0, (unsigned)wifiMapStartTries, ch);
    return false;
  }
  wifiMapIp = WiFi.softAPIP().toString();
  if (wifiMapIp.length() == 0 || wifiMapIp == "0.0.0.0") {
    Serial.println("WIFI MAP softAP IP invalid");
    return false;
  }
  wifiMapWarm = true;
  return true;
}

// BLE 前：mode(AP)+hidden SoftAP，保持射频，绝不 esp_wifi_stop。
void wifiMapPreinitBeforeBle() {
  wifiMapLogHeap("pre-ble");
  WiFi.persistent(false);
  WiFi.setSleep(true);
  wifiMapBuildCreds();
  if (!WiFi.mode(WIFI_AP)) {
    Serial.println("WIFI MAP preinit WiFi.mode(AP) fail");
    wifiMapDriverReady = false;
    wifiMapWarm = false;
    wifiMapLogHeap("preinit-fail");
    return;
  }
  delay(30);
  if (!wifiMapSoftApApply(/*hidden=*/true)) {
    Serial.println("WIFI MAP preinit hidden softAP fail");
    wifiMapDriverReady = false;
    wifiMapWarm = false;
    wifiMapLogHeap("preinit-softap-fail");
    return;
  }
  wifiMapDriverReady = true;
  Serial.printf("WIFI MAP warm SoftAP hidden ssid=%s ip=%s\n",
                wifiMapSsid.c_str(), wifiMapIp.c_str());
  wifiMapLogHeap("preinit-ok");
}

static void wifiMapStop() {
  wifiMapDestroyServers();
  wifiMapRunning = false;
  wifiMapState = WM_IDLE;
  wifiMapStartTries = 0;
  wifiMapNextTryMs = 0;
  wifiMapStartFailed = false;
  // 回到 hidden 暖 AP，保持与 BLE 共存的射频路径
  if (wifiMapDriverReady) {
    if (!wifiMapSoftApApply(/*hidden=*/true)) {
      // 退隐失败也不 stop/deinit —— 避免再次打开时 DRAM/共存再炸
      Serial.println("WIFI MAP re-hide failed; leave as-is");
    }
  }
  Serial.println("WIFI MAP SoftAP hidden (warm kept)");
}

static bool wifiMapBringUpAp() {
  wifiMapLogHeap("bringup");
  WiFi.persistent(false);
  WiFi.setSleep(true);
  wifiMapReclaimInternalHeap();

  if (!wifiMapDriverReady) {
    if (!WiFi.mode(WIFI_AP)) {
      Serial.println("WIFI MAP mode(AP) fail (cold)");
      return false;
    }
    delay(40);
  } else if (WiFi.getMode() != WIFI_AP && WiFi.getMode() != WIFI_AP_STA) {
    if (!WiFi.mode(WIFI_AP)) {
      Serial.println("WIFI MAP mode(AP) restore fail");
      return false;
    }
  }

  // 暖路径：仅 unhide / 重配，不 stop
  if (!wifiMapSoftApApply(/*hidden=*/false)) {
    wifiMapLogHeap("softap-fail");
    return false;
  }
  return true;
}

static void wifiMapStartServers() {
  wifiMapDestroyServers();
  wifiMapHttp = new WebServer(80);
  if (!wifiMapHttp) {
    Serial.println("WIFI MAP http alloc fail");
    return;
  }
  wifiMapHttp->on("/", HTTP_GET, []() {
    wifiMapHttp->send(200, "text/plain",
                     String("MotoNav WiFi map exp\nssid=") + wifiMapSsid +
                     "\nip=" + wifiMapIp +
                     "\ntcp=" + String(WIFI_MAP_TCP_PORT) +
                     "\nframe=MN\\x01 type id totalBE payload\n");
  });
  wifiMapHttp->begin();

  wifiMapTcp = new WiFiServer(WIFI_MAP_TCP_PORT);
  if (!wifiMapTcp) {
    Serial.println("WIFI MAP tcp alloc fail");
    wifiMapDestroyServers();
    return;
  }
  wifiMapTcp->begin();
  wifiMapTcp->setNoDelay(true);
  Serial.printf("WIFI MAP servers up tcp=%d\n", WIFI_MAP_TCP_PORT);
}

static void wifiMapMarkRunningAndNotify() {
  wifiMapRunning = true;
  wifiMapStartFailed = false;
  wifiMapState = WM_IDLE;
  Serial.printf("WIFI MAP SoftAP visible ssid=%s ip=%s tcp=%d\n",
                wifiMapSsid.c_str(), wifiMapIp.c_str(), WIFI_MAP_TCP_PORT);
  wifiMapLogHeap("running");
  wifiMapQueueStatus();   // 先回 WIFI,1，再起服务器，避免 App 超时
}

static void wifiMapStart() {
  if (wifiMapRunning && wifiMapHttp && wifiMapTcp) {
    wifiMapQueueStatus();
    return;
  }
  // 暖 AP 已在且仅缺服务器：unhide + 服务器
  if (!wifiMapBringUpAp()) {
    wifiMapRunning = false;
    return;
  }
  wifiMapMarkRunningAndNotify();
  wifiMapStartServers();
  // 服务器失败仍保持 WIFI,1（热点已开）；探针/TCP 稍后重试
  if (!wifiMapHttp || !wifiMapTcp) {
    Serial.println("WIFI MAP servers incomplete; SoftAP still advertised");
  }
}

void wifiMapSetEnabled(bool on) {
  wifiMapWanted = on;
  if (on) {
    wifiMapStartFailed = false;
    wifiMapStartTries = 1;
    wifiMapNextTryMs = millis() + 120;
    if (wifiMapWarm && wifiMapDriverReady) {
      // 暖路径通常一次成功
      wifiMapStart();
      if (!wifiMapRunning) wifiMapNextTryMs = millis() + 300;
    }
  } else {
    wifiMapStop();
    wifiMapQueueStatus();
  }
}

void wifiMapQuery() { wifiMapQueueStatus(); }

void wifiMapTick() {
  if (wifiMapWanted && !wifiMapRunning && !wifiMapStartFailed) {
    uint32_t now = millis();
    if (now >= wifiMapNextTryMs && wifiMapStartTries < WIFI_MAP_START_TRIES) {
      wifiMapStartTries++;
      wifiMapStart();
      if (!wifiMapRunning) {
        wifiMapNextTryMs = now + 300u * wifiMapStartTries;
        if (wifiMapStartTries >= WIFI_MAP_START_TRIES) {
          wifiMapStartFailed = true;
          pendingWifiStatus = "WIFI,0,err";
          Serial.println("WIFI MAP softAP gave up");
          wifiMapLogHeap("gave-up");
        }
      }
    }
    return;
  }
  if (!wifiMapRunning) return;
  if (wifiMapHttp) wifiMapHttp->handleClient();
  if (!wifiMapTcp) return;
  uint32_t now = millis();

  switch (wifiMapState) {
    case WM_IDLE: {
      WiFiClient c = wifiMapTcp->available();
      if (c) {
        wifiMapClient = c;
        wifiMapState = WM_HDR;
        wifiMapHdrGot = 0;
        wifiMapStartMs = now;
      }
      break;
    }
    case WM_HDR: {
      int avail = wifiMapClient.available();
      if (avail > 0 && wifiMapHdrGot < 10) {
        int n = wifiMapClient.read(wifiMapHdr + wifiMapHdrGot,
                                   (size_t)min((uint32_t)avail, 10u - wifiMapHdrGot));
        if (n > 0) wifiMapHdrGot += (uint32_t)n;
      }
      if (wifiMapHdrGot >= 10) {
        if (wifiMapHdr[0] != 'M' || wifiMapHdr[1] != 'N' || wifiMapHdr[2] != 0x01) {
          wifiMapFail("magic");
          break;
        }
        int type = wifiMapHdr[3];
        int id = wifiMapHdr[4];
        uint32_t total = ((uint32_t)wifiMapHdr[5] << 24) | ((uint32_t)wifiMapHdr[6] << 16) |
                         ((uint32_t)wifiMapHdr[7] << 8) | (uint32_t)wifiMapHdr[8];
        if (type != 3 || !bulks[3].buf || total == 0 || total > bulks[3].cap) {
          wifiMapFail("args");
          break;
        }
        wifiMapPayId = id;
        wifiMapPayTotal = total;
        wifiMapPayGot = 0;
        wifiMapStartMs = now;
        wifiMapState = WM_PAYLOAD;
      } else if (now - wifiMapStartMs > 8000 || !wifiMapClient.connected()) {
        wifiMapFail("hdr");
      }
      break;
    }
    case WM_PAYLOAD: {
      uint32_t remain = wifiMapPayTotal - wifiMapPayGot;
      int avail = wifiMapClient.available();
      if (avail > 0 && remain > 0) {
        uint32_t chunk = (uint32_t)avail;
        if (chunk > remain) chunk = remain;
        if (chunk > 8192) chunk = 8192;
        int n = wifiMapClient.read(bulks[3].buf + wifiMapPayGot, (size_t)chunk);
        if (n > 0) wifiMapPayGot += (uint32_t)n;
      }
      if (wifiMapPayGot >= wifiMapPayTotal) {
        bulks[3].done = false;
        bulks[3].got = 0;
        memset(bulks[3].mask, 0, sizeof(bulks[3].mask));
        bool ok = wifiMapApplyType3(wifiMapPayId, wifiMapPayTotal);
        if (ok) snprintf(wifiMapReply, sizeof(wifiMapReply), "K,3,%d\n", wifiMapPayId);
        else snprintf(wifiMapReply, sizeof(wifiMapReply), "E,decode\n");
        wifiMapState = WM_REPLY;
      } else if (now - wifiMapStartMs > 60000 ||
                 (!wifiMapClient.connected() && avail <= 0)) {
        wifiMapFail("short");
      }
      break;
    }
    case WM_REPLY: {
      wifiMapClient.print(wifiMapReply);
      wifiMapClient.flush();
      wifiMapClient.stop();
      wifiMapState = WM_IDLE;
      break;
    }
    default:
      wifiMapState = WM_IDLE;
      break;
  }
}

bool wifiMapIsRunning() { return wifiMapRunning; }
bool wifiMapIsWarm() { return wifiMapWarm; }
