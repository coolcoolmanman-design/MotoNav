#pragma once
// 试验：圆屏 SoftAP + TCP 整包收 type3（不用 WebServer body/String，避免二进制截断）。
// musicA.95 自 git 历史移植（v7.12.10-wifi.2 最终态 + MZ1 帧适配）。
// 控制：#WIFI,1 / #WIFI,0 / #WIFI?
// 探测：GET http://192.168.4.1/
// 灌图：TCP 192.168.4.1:7443
//   帧头 10B：'M''N' 0x01 | type:u8 | id:u8 | total:u32be | 预留 0
//   应答：ASCII "K,3,id\n" 或 "E,code\n"；成功时同时 BLE 发 K,3,id
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClient.h>
#include <WiFiServer.h>

#ifndef WIFI_MAP_PSK_PREFIX
#define WIFI_MAP_PSK_PREFIX "motoNav"
#endif
#ifndef WIFI_MAP_TCP_PORT
#define WIFI_MAP_TCP_PORT 7443
#endif

static bool wifiMapWanted = false;
static bool wifiMapRunning = false;
static WebServer *wifiMapHttp = nullptr;
static WiFiServer *wifiMapTcp = nullptr;
static String wifiMapSsid = "";
static String wifiMapPsk = "";
static String wifiMapIp = "192.168.4.1";
String pendingWifiStatus = "";

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
  } else {
    pendingWifiStatus = wifiMapWanted ? "WIFI,0,err" : "WIFI,0";
  }
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
// musicA.95：早期版本在主循环里同步 read 整包（最长阻塞 60s），导航中会冻屏 + BLE 断供。
// 改为分帧状态机：每 tick 最多读 8KB，读完再回包，主循环随时可退出。
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
    WiFi.mode(WIFI_OFF);
  }
  wifiMapRunning = false;
  Serial.println("WIFI MAP SoftAP off");
}

static void wifiMapStart() {
  if (wifiMapRunning && wifiMapHttp && wifiMapTcp) {
    wifiMapQueueStatus();
    return;
  }
  wifiMapDestroyServers();
  wifiMapBuildCreds();
  WiFi.mode(WIFI_AP);
  bool ok = WiFi.softAP(wifiMapSsid.c_str(), wifiMapPsk.c_str(), 1, 0, 4);
  if (!ok) {
    Serial.println("WIFI MAP softAP failed");
    WiFi.mode(WIFI_OFF);
    wifiMapRunning = false;
    pendingWifiStatus = "WIFI,0,err";
    return;
  }
  wifiMapIp = WiFi.softAPIP().toString();

  wifiMapHttp = new WebServer(80);
  wifiMapHttp->on("/", HTTP_GET, []() {
    wifiMapHttp->send(200, "text/plain",
                     String("MotoNav WiFi map exp\nssid=") + wifiMapSsid +
                     "\nip=" + wifiMapIp +
                     "\ntcp=" + String(WIFI_MAP_TCP_PORT) +
                     "\nframe=MN\\x01 type id totalBE payload\n");
  });
  wifiMapHttp->begin();

  wifiMapTcp = new WiFiServer(WIFI_MAP_TCP_PORT);
  wifiMapTcp->begin();
  wifiMapTcp->setNoDelay(true);

  wifiMapRunning = true;
  Serial.printf("WIFI MAP SoftAP ssid=%s ip=%s tcp=%d\n",
                wifiMapSsid.c_str(), wifiMapIp.c_str(), WIFI_MAP_TCP_PORT);
  wifiMapQueueStatus();
}

void wifiMapSetEnabled(bool on) {
  wifiMapWanted = on;
  if (on) wifiMapStart();
  else {
    wifiMapStop();
    wifiMapQueueStatus();
  }
}

void wifiMapQuery() { wifiMapQueueStatus(); }

void wifiMapTick() {
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
        if (chunk > 8192) chunk = 8192;          // 每 tick 最多 8KB，主循环不卡
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
