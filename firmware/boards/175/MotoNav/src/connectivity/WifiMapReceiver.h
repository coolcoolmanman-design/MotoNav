#pragma once
// 试验：圆屏 SoftAP + TCP 整包收 type3（不用 WebServer body/String，避免二进制截断）。
// musicA.95 自 git 历史移植；musicA.97 重试；musicA.113：
//   回到 musicA.96 的干净路径（用户确认 .96/.98 能正常开热点）：不在 BLE 前预 init，
//   #WIFI,1 时一次 WiFi.mode(AP)+softAP() 起热点，关热点回 WIFI_OFF（完全 deinit）。
//   .110/.112 的 preinit「装驱动再拆」会让 bringUp 反复 WiFi.mode(AP) 触发
//   `Failed to deinit Wi-Fi driver (0x3001)` 循环、堆从 18KB 掉到 7KB，是失败根因。
//   bringUp 前仅释放导航可选缓存（vigKeep/ringFast），给驱动腾连续内部块。
// 控制：#WIFI,1 / #WIFI,0 / #WIFI?
// 探测：GET http://192.168.4.1/
// 灌图：TCP 192.168.4.1:7443
//   帧头 10B：'M''N' 0x01 | type:u8 | id:u8 | total:u32be | 预留 0
//   应答：ASCII "K,3,id\n" 或 "E,code\n"；成功时同时 BLE 发 K,3,id
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
#define WIFI_MAP_START_TRIES 6
#endif

static bool wifiMapWanted = false;  // 由 App #WIFI,1 触发（真实场景）
static bool wifiMapRunning = false;
static WebServer *wifiMapHttp = nullptr;
static WiFiServer *wifiMapTcp = nullptr;
static String wifiMapSsid = "";
static String wifiMapPsk = "";
static String wifiMapIp = "192.168.4.1";
String pendingWifiStatus = "";
static uint8_t wifiMapStartTries = 0;
static uint32_t wifiMapNextTryMs = 0;
static bool wifiMapStartFailed = false;

static void wifiMapLogHeap(const char *tag) {
  Serial.printf("WIFI MAP %s int_free=%u largest=%u spiram=%u\n", tag,
                (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
                (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}

static void wifiMapBuildCreds() {
  uint8_t mac[6] = {0};
  esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
  char ssid[24];
  snprintf(ssid, sizeof(ssid), "MotoNav-%02X%02X", mac[4], mac[5]);
  wifiMapSsid = ssid;
  char psk[20];
  // WPA2 要求 8–63 字节；motoNav + 4 hex = 11
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
  // 仍在重试：不发 err，避免 App 把「正在打开」当成永久失败
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

// ---- 非阻塞 TCP 收包状态机 --------------------------------------------------
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

static void wifiMapStop() {
  wifiMapDestroyServers();
  if (wifiMapRunning || WiFi.getMode() != WIFI_OFF) {
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);                                // musicA.113：干净 deinit（回 .96 路径）
  }
  wifiMapRunning = false;
  wifiMapState = WM_IDLE;
  wifiMapStartTries = 0;
  wifiMapNextTryMs = 0;
  wifiMapStartFailed = false;
  Serial.println("WIFI MAP SoftAP off");
}

static bool wifiMapBringUpAp() {
  wifiMapBuildCreds();
  wifiMapLogHeap("bringup");
  // musicA.113：BLE 已连接、真实导航态下 vigKeep(12.9KB)+ringFast(12.5KB) 占用
  // 内部 RAM，把最大连续块从 ~18KB 压到 ~5KB，WiFi 驱动 init 装不下。
  // 起热点前先释放这两个可选缓存（后续雷达绘制会自动降级/重建），腾出连续块。
  wifiMapReclaimInternalHeap();
  wifiMapLogHeap("after-reclaim");
  WiFi.persistent(false);
  WiFi.setSleep(true);

  // musicA.96 干净路径：一次性装驱动；不预 init、不 esp_wifi_start，避免反复
  // WiFi.mode(AP) 触发 `Failed to deinit Wi-Fi driver (0x3001)` 循环。
  if (!WiFi.mode(WIFI_AP)) {
    Serial.println("WIFI MAP mode(AP) fail (bringup)");
    wifiMapLogHeap("mode-fail");
    return false;
  }
  delay(30);

  IPAddress ip(192, 168, 4, 1);
  IPAddress gw(192, 168, 4, 1);
  IPAddress mask(255, 255, 255, 0);
  WiFi.softAPConfig(ip, gw, mask);

  const int ch = (wifiMapStartTries & 1) ? 6 : 1;
  bool ok = WiFi.softAP(wifiMapSsid.c_str(), wifiMapPsk.c_str(), ch, 0, 2);
  if (!ok) {
    Serial.printf("WIFI MAP softAP fail try=%u ch=%d\n", (unsigned)wifiMapStartTries, ch);
    wifiMapLogHeap("softap-fail");
    WiFi.mode(WIFI_OFF);
    return false;
  }
  wifiMapIp = WiFi.softAPIP().toString();
  if (wifiMapIp.length() == 0 || wifiMapIp == "0.0.0.0") {
    Serial.println("WIFI MAP softAP IP invalid");
    WiFi.mode(WIFI_OFF);
    return false;
  }
  return true;
}

static void wifiMapStartServers() {
  wifiMapDestroyServers();
  wifiMapHttp = new WebServer(80);
  if (!wifiMapHttp) {
    Serial.println("WIFI MAP http alloc fail");
    wifiMapStop();
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
    wifiMapStop();
    return;
  }
  wifiMapTcp->begin();
  wifiMapTcp->setNoDelay(true);

  wifiMapRunning = true;
  wifiMapStartFailed = false;
  wifiMapState = WM_IDLE;
  Serial.printf("WIFI MAP SoftAP ssid=%s ip=%s tcp=%d\n",
                wifiMapSsid.c_str(), wifiMapIp.c_str(), WIFI_MAP_TCP_PORT);
  wifiMapLogHeap("running");
  wifiMapQueueStatus();
}

static void wifiMapStart() {
  if (wifiMapRunning && wifiMapHttp && wifiMapTcp) {
    wifiMapQueueStatus();
    return;
  }
  if (!wifiMapBringUpAp()) {
    wifiMapRunning = false;
    return;
  }
  wifiMapStartServers();
}

void wifiMapSetEnabled(bool on) {
  wifiMapWanted = on;
  if (on) {
    wifiMapStartFailed = false;
    wifiMapNextTryMs = 0;
    wifiMapStartTries = 1;
    // 错开 BLE 命令同拍：首次稍延后，主循环再开
    wifiMapNextTryMs = millis() + 180;
    // 若已延后，本拍不立刻 bring-up；仍允许已 running 时直接回执
    if (wifiMapRunning) {
      wifiMapQueueStatus();
      return;
    }
    // 立即尝试一次（预 init 成功时通常一次就成）
    wifiMapStart();
    if (!wifiMapRunning) {
      wifiMapNextTryMs = millis() + 350;
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
        wifiMapNextTryMs = now + 280u * wifiMapStartTries;
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
          Serial.println("WIFI MAP tcp magic");
          break;
        }
        int type = wifiMapHdr[3];
        int id = wifiMapHdr[4];
        uint32_t total = ((uint32_t)wifiMapHdr[5] << 24) | ((uint32_t)wifiMapHdr[6] << 16) |
                         ((uint32_t)wifiMapHdr[7] << 8) | (uint32_t)wifiMapHdr[8];
        if (type != 3 || !bulks[3].buf || total == 0 || total > bulks[3].cap) {
          wifiMapFail("args");
          Serial.printf("WIFI MAP tcp args type=%d id=%d total=%lu\n", type, id,
                        (unsigned long)total);
          break;
        }
        wifiMapPayId = id;
        wifiMapPayTotal = total;
        wifiMapPayGot = 0;
        wifiMapStartMs = now;
        wifiMapState = WM_PAYLOAD;
      } else if (now - wifiMapStartMs > 8000) {
        wifiMapFail("hdr");
        Serial.println("WIFI MAP tcp hdr timeout");
      } else if (!wifiMapClient.connected()) {
        wifiMapFail("hdr");
        Serial.println("WIFI MAP tcp closed on hdr");
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
        Serial.printf("WIFI MAP tcp id=%d bytes=%lu ok=%d\n", wifiMapPayId,
                      (unsigned long)wifiMapPayTotal, ok ? 1 : 0);
      } else if (now - wifiMapStartMs > 60000) {
        wifiMapFail("short");
        Serial.printf("WIFI MAP tcp short need=%lu\n", (unsigned long)wifiMapPayTotal);
      } else if (!wifiMapClient.connected() && avail <= 0) {
        wifiMapFail("short");
        Serial.println("WIFI MAP tcp closed on payload");
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
