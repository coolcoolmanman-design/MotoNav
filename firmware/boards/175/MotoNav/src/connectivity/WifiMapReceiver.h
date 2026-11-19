#pragma once
// 试验：圆屏 SoftAP + TCP 整包收 type3（不用 WebServer body/String，避免二进制截断）。
// musicA.95 自 git 历史移植；musicA.97 重试；musicA.113 回 .96 干净路径后实测仍 0x3001——
//   真正根因是内部 RAM 见底：BLE 连接态 int_free=32.7KB / largest=18.4KB，
//   而 esp_wifi init 需要更大连续块（首次尝试耗 17.7KB 后失败且回滚不干净）。
//   历史能开热点的 v7.12.10-wifi.2 app 仅 1.8MB，现行 3.6MB 功能把内部堆吃光了。
// musicA.114 结构性腾内存：
//   ① 6 个独立 tinfl 解压器（≈66KB 内部 RAM）合并为 motonavSharedInflater() 1 个（省 ≈55KB）；
//   ② 车道胶囊 laneCol/laneA（42KB）与地图 tmpX/tmpY（32KB）改走 PSRAM 优先（省 ≈74KB）；
//   ③ bringUp 前仍释放导航可选缓存（vigKeep 12.9K / ringFast 12.5K）。
// musicA.116 修 .115 空转 + 自毁循环：
//   ① .115 的 ringFastColors 只在内部空闲 > bytes+32KB(≈45KB) 时才会分配，BLE 连接态
//      只剩 32.7KB，它根本不存在 → reclaim 释放 0 字节。改为同时释放 ringTables 的
//      rq2r（≈5.8KB，ringTablesInit 按需重建）并在加速窗口内抑制重建；
//   ② 失败路径不再调用 wifiMapRestoreInternalCaches()（只有用户主动关热点才恢复），
//      避免服务器分配失败后下一帧把 12.5KB 吃回去；
//   ③ 放弃后 15s 冷却 + 固件重试窗口内的重复 #WIFI,1 直接忽略，别再 24s 打 7 次
//      init/deinit 把内部堆搅碎（0x3001 循环的来源）。
// musicA.117 真修复：BLE 连上后内部堆已经装不下 WiFi 驱动（实测 reclaim 释放 0 字节），
//   所以驱动改在 BLEDevice::init() 之前预初始化（boot 时堆还连续），并一直保持 init 状态：
//   ① NVS motonav/wifi_ap 记住用户是否用过「WiFi 地图加速」；只有为 true 才在 boot 预 init，
//      不用加速的用户（默认）完全不受影响；
//   ② 首次 #WIFI,1 时还没有预 init → 存标志 + 回 WIFI,0,err（App 保持重试）+ 重启，
//      重启后 preinit 成功，App 重连会补发 #WIFI,1，这次直接 start 就行；
//   ③ 预 init 后关热点只 esp_wifi_stop()，绝不用 softAPdisconnect(true)/WiFi.mode(WIFI_OFF)
//      —— 那会走 wifiLowLevelDeinit() 把驱动 deinit 掉，再开又要大块 DRAM（.110 的坑）。
// 控制：#WIFI,1 / #WIFI,0 / #WIFI?
// 探测：GET http://192.168.4.1/
// 灌图：TCP 192.168.4.1:7443
//   帧头 10B：'M''N' 0x01 | type:u8 | id:u8 | total:u32be | 预留 0
//   应答：ASCII "K,3,id\n" 或 "E,code\n"；成功时同时 BLE 发 K,3,id
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <Preferences.h>
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
#ifndef WIFI_MAP_FAIL_COOLDOWN_MS
#define WIFI_MAP_FAIL_COOLDOWN_MS 15000   // 放弃后冷却，期间重复 #WIFI,1 直接回 err，不再 init/deinit
#endif
#ifndef WIFI_MAP_SELFTEST
#define WIFI_MAP_SELFTEST 0               // 仅本地联调：开机自动开关热点验证预 init
#endif

static bool wifiMapWanted = false;  // 由 App #WIFI,1 触发（真实场景）
static bool wifiMapRunning = false;
static bool wifiMapDriverReady = false;      // musicA.117：boot 预 init 成功，驱动常驻
static bool wifiMapPreinitAttempted = false; // musicA.117：本 boot 已走过预 init（无论成败）
static WebServer *wifiMapHttp = nullptr;
static WiFiServer *wifiMapTcp = nullptr;
static String wifiMapSsid = "";
static String wifiMapPsk = "";
static String wifiMapIp = "192.168.4.1";
String pendingWifiStatus = "";
static uint8_t wifiMapStartTries = 0;
static uint32_t wifiMapNextTryMs = 0;
static bool wifiMapStartFailed = false;
static uint32_t wifiMapFailAt = 0;   // musicA.116：放弃时刻，用于冷却，别让 App 的重发把 init/deinit 打成一串

static void wifiMapLogHeap(const char *tag) {
  Serial.printf("WIFI MAP %s int_free=%u largest=%u spiram=%u\n", tag,
                (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
                (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}

// ---- musicA.117：NVS 持久标志（是否用过「WiFi 地图加速」）--------------------
static bool wifiMapFlagLoad() {
  Preferences p;
  bool v = false;
  if (p.begin("motonav", true)) {
    v = p.getBool("wifi_ap", false);
    p.end();
  }
  return v;
}

static void wifiMapFlagSave(bool v) {
  Preferences p;
  if (p.begin("motonav", false)) {
    p.putBool("wifi_ap", v);
    p.end();
  }
}

// 必须在 BLEDevice::init() 之前调用：此刻内部堆还连续，先把 WiFi 驱动占下并常驻。
// 没用过加速（标志 false，默认）时什么都不做，不影响只用蓝牙的用户。
void wifiMapPreinitBeforeBle() {
  if (!wifiMapFlagLoad()) {
    Serial.println("WIFI MAP preinit skipped (flag off)");
    return;
  }
  wifiMapPreinitAttempted = true;   // 本 boot 已尝试预 init → 首次使能不重启，避免死循环
  wifiMapLogHeap("pre-ble");
  WiFi.persistent(false);
  WiFi.setSleep(true);
  // 走 Arduino 路径装驱动：置位 lowLevelInitDone，之后 softAP 不会再 esp_wifi_init。
  if (!WiFi.mode(WIFI_AP)) {
    Serial.println("WIFI MAP preinit WiFi.mode(AP) fail");
    wifiMapLogHeap("preinit-fail");
    return;
  }
  // 只停射频，保留驱动（勿 softAPdisconnect(true)/mode(OFF)：会 deinit 驱动）。
  esp_err_t st = esp_wifi_stop();
  if (st != ESP_OK && st != ESP_ERR_WIFI_NOT_STARTED) {
    Serial.printf("WIFI MAP preinit stop=%d\n", (int)st);
    wifiMapLogHeap("preinit-stop-fail");
    return;
  }
  wifiMapDriverReady = true;
  wifiMapLogHeap("preinit-ok");
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
  // musicA.118: BLE 的分步解码可能还占着 rasterBackBuf；直接 parseRaster 会和它抢同一块缓冲，
  // 得到半新半旧的底图（楼块轮廓丢失 / 整块空白）。先取消，和 BLE 路径使用同一份前后缓冲。
  cancelPendingRaster();
  b.id = id;
  b.total = total;
  b.parts = 1;
  b.got = 1;
  b.done = true;
  const int theme = themeIdx;
  const bool carry = rasterBuildingCarryOn;
  if (!parseRaster(b.buf, total)) {
    b.done = false;
    b.got = 0;
    memset(b.mask, 0, sizeof(b.mask));
    Serial.printf("WIFI MAP raster reject id=%d bytes=%lu theme=%d carry=%d\n",
                  id, (unsigned long)total, theme, (int)carry);
    return false;
  }
  Serial.printf("WIFI MAP raster ok id=%d bytes=%lu theme=%d carry=%d bld=%d\n",
                id, (unsigned long)total, theme, (int)carry, (int)rasterCarriesBuildings);
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
static int wifiMapHeaderTheme = -1;
static char wifiMapReply[24];

// musicA.118: #t 换风格 / #BC 换建筑开关时，丢弃还在路上的旧 TCP 图块。
// 否则旧风格的数据会在状态清空后才落盘，表现为「原版地图空白」或建筑层丢失。
void wifiMapCancelPendingTransfer() {
  if (wifiMapState == WM_IDLE) return;
  if (wifiMapClient) wifiMapClient.stop();
  wifiMapState = WM_IDLE;
  wifiMapHdrGot = 0;
  wifiMapPayGot = 0;
  wifiMapPayTotal = 0;
  wifiMapPayId = -1;
  wifiMapHeaderTheme = -1;
  Serial.println("WIFI MAP pending raster dropped");
}

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

static void wifiMapStopInternal(bool restoreCaches) {
  wifiMapDestroyServers();
  if (wifiMapClient) wifiMapClient.stop();
  if (wifiMapDriverReady) {
    // musicA.117：驱动是 boot 预 init 的，只能停射频。走 WiFi.mode(WIFI_OFF) /
    // softAPdisconnect(true) 会 wifiLowLevelDeinit()，再开就又要大块 DRAM（0x3001）。
    if (wifiMapRunning) {
      esp_err_t st = esp_wifi_stop();
      if (st != ESP_OK && st != ESP_ERR_WIFI_NOT_STARTED) {
        Serial.printf("WIFI MAP stop=%d\n", (int)st);
      }
    }
    wifiMapRunning = false;
    wifiMapState = WM_IDLE;
    wifiMapStartTries = 0;
    wifiMapNextTryMs = 0;
    wifiMapStartFailed = false;
    if (restoreCaches) wifiMapRestoreInternalCaches();
    Serial.println("WIFI MAP SoftAP off (driver kept)");
    return;
  }
  if (wifiMapRunning || WiFi.getMode() != WIFI_OFF) {
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);                                // musicA.113：干净 deinit（回 .96 路径）
  }
  wifiMapRunning = false;
  wifiMapState = WM_IDLE;
  wifiMapStartTries = 0;
  wifiMapNextTryMs = 0;
  wifiMapStartFailed = false;
  // musicA.116：只有用户主动关热点时才恢复缓存。此前的失败路径（服务器分配失败）
  // 也走到这里，会把 ringFastTried 复位，下一帧立刻把 12.5KB 吃回去，越试越差。
  if (restoreCaches) wifiMapRestoreInternalCaches();
  Serial.println("WIFI MAP SoftAP off");
}

static void wifiMapStop() { wifiMapStopInternal(true); }

static bool wifiMapBringUpAp() {
  wifiMapBuildCreds();
  wifiMapLogHeap("bringup");
  WiFi.persistent(false);
  WiFi.setSleep(true);

  if (wifiMapDriverReady) {
    // musicA.117：驱动在 BLEDevice::init() 之前就装好了，这里只开射频。
    // 不再 reclaim、不再 esp_wifi_init —— BLE 连上后内部堆装不下驱动（.116 实测）。
    esp_err_t st = esp_wifi_start();
    if (st != ESP_OK) {
      Serial.printf("WIFI MAP esp_wifi_start=%d\n", (int)st);
      wifiMapLogHeap("start-fail");
      return false;
    }
    if (!WiFi.mode(WIFI_AP)) {
      Serial.println("WIFI MAP mode(AP) fail (preinit)");
      esp_wifi_stop();
      return false;
    }
  } else {
    // musicA.113：没有预 init 的兜底路径。BLE 已连接、真实导航态下 vigKeep(12.9KB)
    // +ringFast(12.5KB) 占用内部 RAM，先释放再赌一次（通常仍会失败，所以 .117 才改预 init）。
    wifiMapReclaimInternalHeap();
    wifiMapLogHeap("after-reclaim");

    // musicA.96 干净路径：一次性装驱动；不预 init、不 esp_wifi_start，避免反复
    // WiFi.mode(AP) 触发 `Failed to deinit Wi-Fi driver (0x3001)` 循环。
    if (!WiFi.mode(WIFI_AP)) {
      Serial.println("WIFI MAP mode(AP) fail (bringup)");
      wifiMapLogHeap("mode-fail");
      return false;
    }
    delay(30);
  }

  IPAddress ip(192, 168, 4, 1);
  IPAddress gw(192, 168, 4, 1);
  IPAddress mask(255, 255, 255, 0);
  WiFi.softAPConfig(ip, gw, mask);

  const int ch = (wifiMapStartTries & 1) ? 6 : 1;
  bool ok = WiFi.softAP(wifiMapSsid.c_str(), wifiMapPsk.c_str(), ch, 0, 2);
  if (!ok) {
    Serial.printf("WIFI MAP softAP fail try=%u ch=%d\n", (unsigned)wifiMapStartTries, ch);
    wifiMapLogHeap("softap-fail");
    if (wifiMapDriverReady) esp_wifi_stop();   // 保留驱动，等下次重试
    else WiFi.mode(WIFI_OFF);
    return false;
  }
  wifiMapIp = WiFi.softAPIP().toString();
  if (wifiMapIp.length() == 0 || wifiMapIp == "0.0.0.0") {
    Serial.println("WIFI MAP softAP IP invalid");
    if (wifiMapDriverReady) esp_wifi_stop();
    else WiFi.mode(WIFI_OFF);
    return false;
  }
  return true;
}

static void wifiMapStartServers() {
  wifiMapDestroyServers();
  wifiMapHttp = new WebServer(80);
  if (!wifiMapHttp) {
    Serial.println("WIFI MAP http alloc fail");
    wifiMapStopInternal(false);          // musicA.116：失败不恢复雷达缓存，否则下一帧又吃回 12.5KB
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
    wifiMapStopInternal(false);          // musicA.116：同上，失败路径不恢复缓存
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
    if (wifiMapRunning) {
      wifiMapQueueStatus();
      return;
    }
    if (!wifiMapDriverReady && !wifiMapPreinitAttempted) {
      // musicA.117：本 boot 没预 init（首次使用加速）。此刻 BLE 已连、内部堆已碎，
      // 装不下 WiFi 驱动（.116 实测 reclaim 释放 0 字节）。存标志 + 回 err（App 仍
      // 在重试窗口内会继续重发）+ 重启；重启后 boot 在 BLE 之前预 init，一次就开。
      wifiMapFlagSave(true);
      pendingWifiStatus = "WIFI,0,err";
      Serial.println("WIFI MAP first enable: rebooting for preinit");
      Serial.flush();
      delay(200);
      ESP.restart();
      return;
    }
    const uint32_t now = millis();
    // musicA.116：App 每 3.5s 重发一次 #WIFI,1，共 7 次。此前每次都把计数清零
    // 并立刻再 esp_wifi_init 一次，24s 内打出 7 次 init/deinit，内部堆越搅越碎
    // （0x3001 循环的来源）。现在：失败后冷却期内直接回 err；固件自己的重试
    // 窗口开着时，重复命令直接忽略——重试交给固件。
    if (wifiMapStartFailed) {
      if (wifiMapFailAt && now - wifiMapFailAt < WIFI_MAP_FAIL_COOLDOWN_MS) {
        wifiMapQueueStatus();                    // 冷却中：回 WIFI,0,err，不碰驱动
        return;
      }
      wifiMapStartFailed = false;                // 冷却结束，允许再试一次
      wifiMapStartTries = 0;
    }
    if (wifiMapStartTries > 0 && wifiMapStartTries < WIFI_MAP_START_TRIES &&
        (int32_t)(now - wifiMapNextTryMs) < 0) {
      return;                                    // 固件正在退避重试，重复命令忽略
    }
    wifiMapStartTries = 1;
    // 错开 BLE 命令同拍：首次稍延后，主循环再开
    wifiMapNextTryMs = now + 180;
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
          wifiMapFailAt = millis();                  // musicA.116：开始冷却，挡住 App 的连发
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
        wifiMapHeaderTheme = themeIdx;
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
        bool ok = false;
        if (wifiMapHeaderTheme >= 0 && wifiMapHeaderTheme != themeIdx) {
          Serial.printf("WIFI MAP tcp stale style hdr=%d now=%d\n", wifiMapHeaderTheme, themeIdx);
          snprintf(wifiMapReply, sizeof(wifiMapReply), "E,style\n");
        } else {
          ok = wifiMapApplyType3(wifiMapPayId, wifiMapPayTotal);
          if (ok) snprintf(wifiMapReply, sizeof(wifiMapReply), "K,3,%d\n", wifiMapPayId);
          else snprintf(wifiMapReply, sizeof(wifiMapReply), "E,decode\n");
        }
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
