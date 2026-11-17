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

static bool wifiMapReadExact(WiFiClient &c, uint8_t *dst, uint32_t need, uint32_t waitMs) {
  uint32_t got = 0;
  uint32_t deadline = millis() + waitMs;
  while (got < need && millis() < deadline) {
    int avail = c.available();
    if (avail <= 0) {
      if (!c.connected()) break;
      delay(1);
      yield();
      continue;
    }
    int n = c.read(dst + got, (size_t)min((uint32_t)avail, need - got));
    if (n <= 0) {
      delay(1);
      yield();
      continue;
    }
    got += (uint32_t)n;
  }
  return got == need;
}

static void wifiMapServeTcp(WiFiClient &c) {
  uint8_t hdr[10];
  // 头 10 字节：约 8s；载荷按大小再给时间（约 50KB/s 下限）
  if (!wifiMapReadExact(c, hdr, 10, 8000)) {
    c.print("E,hdr\n");
    c.stop();
    Serial.println("WIFI MAP tcp hdr fail");
    return;
  }
  if (hdr[0] != 'M' || hdr[1] != 'N' || hdr[2] != 0x01) {
    c.print("E,magic\n");
    c.stop();
    Serial.println("WIFI MAP tcp magic");
    return;
  }
  int type = hdr[3];
  int id = hdr[4];
  // 10B 布局：0-1 magic, 2 ver, 3 type, 4 id, 5..8 total BE, 9 预留=0
  uint32_t total = ((uint32_t)hdr[5] << 24) | ((uint32_t)hdr[6] << 16) |
                   ((uint32_t)hdr[7] << 8) | (uint32_t)hdr[8];

  if (type != 3 || id < 0 || id > 255 || !bulks[3].buf || total == 0 || total > bulks[3].cap) {
    c.print("E,args\n");
    c.stop();
    Serial.printf("WIFI MAP tcp args type=%d id=%d total=%lu\n", type, id, (unsigned long)total);
    return;
  }

  uint32_t payloadWait = 8000 + (total / 20);  // ~20KB/s 保底
  if (payloadWait > 60000) payloadWait = 60000;
  if (!wifiMapReadExact(c, bulks[3].buf, total, payloadWait)) {
    c.print("E,short\n");
    c.stop();
    Serial.printf("WIFI MAP tcp short need=%lu\n", (unsigned long)total);
    return;
  }

  bulks[3].done = false;
  bulks[3].got = 0;
  memset(bulks[3].mask, 0, sizeof(bulks[3].mask));
  bool ok = wifiMapApplyType3(id, total);
  if (ok) {
    c.print(pendingAck);
    c.print("\n");
  } else {
    c.print("E,decode\n");
  }
  c.flush();
  c.stop();
  Serial.printf("WIFI MAP tcp id=%d bytes=%lu ok=%d\n", id, (unsigned long)total, ok ? 1 : 0);
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
  if (wifiMapTcp) {
    WiFiClient c = wifiMapTcp->available();
    if (c) wifiMapServeTcp(c);
  }
}

bool wifiMapIsRunning() { return wifiMapRunning; }
