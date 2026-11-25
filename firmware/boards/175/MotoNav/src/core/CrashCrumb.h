#pragma once
// musicA.138：重启现场面包屑。RTC_NOINIT 内存在软重启 / panic / 看门狗 / 掉电欠压后都还在（断电后清零），
// 开机时打印并存进 NVS，手机发 "#RR?" 能读回「上一次为什么重启、死在哪个阶段」。
#include <esp_system.h>
#include <esp_heap_caps.h>
#include <Preferences.h>

RTC_NOINIT_ATTR static uint32_t crumbMagic;
RTC_NOINIT_ATTR static uint32_t crumbStage;     // 最近一次记录的阶段（见下表）
RTC_NOINIT_ATTR static uint32_t crumbFrames;    // 地图帧计数
RTC_NOINIT_ATTR static uint32_t crumbMinHeap;   // 内部堆历史最低（字节）
RTC_NOINIT_ATTR static uint32_t crumbMs;        // 记录阶段时的 millis()
// 阶段：0..10 = MapFrame 里 profMark 的序号（刚做完这一步）；100/101 = 推屏任务 开始/结束；
//       300/301 = 第二核心画底图 开始/结束；200 = 地图帧开始。
static inline void crumb(uint32_t stage) { crumbMagic = 0xC0FFEE42u; crumbStage = stage; crumbMs = millis(); }
static inline void crumbHeap() {
  uint32_t f = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  if (crumbMinHeap == 0 || f < crumbMinHeap) crumbMinHeap = f;
}

static String crumbLast = "";   // 上次重启的一行摘要，"#RR?" 回这一行
RTC_NOINIT_ATTR static uint32_t crumbWho;     // musicA.144：主动重启的发起者（见 CRUMB_WHO_*），0 = 非主动
static const char *crumbReasonName(int r) {
  switch (r) {
    case ESP_RST_UNKNOWN: return "unknown"; case ESP_RST_POWERON: return "poweron";
    case ESP_RST_EXT: return "ext";         case ESP_RST_SW: return "sw";
    case ESP_RST_PANIC: return "panic";     case ESP_RST_INT_WDT: return "int_wdt";
    case ESP_RST_TASK_WDT: return "task_wdt"; case ESP_RST_WDT: return "wdt";
    case ESP_RST_DEEPSLEEP: return "deepsleep";
    case ESP_RST_BROWNOUT: return "brownout";
    case ESP_RST_SDIO: return "sdio";       case ESP_RST_USB: return "usb";
    case ESP_RST_JTAG: return "jtag";
    case ESP_RST_EFUSE: return "efuse";     case ESP_RST_PWR_GLITCH: return "pwr_glitch";
    case ESP_RST_CPU_LOCKUP: return "cpu_lockup";
    default: return "other";
  }
}
// 主动重启的发起者。musicA.144：以前 sw 重启一律不存档，"导航中重启"查不出来源，现在带上标签一起存。
enum {
  CRUMB_WHO_NONE = 0, CRUMB_WHO_FASTFLUSH = 1, CRUMB_WHO_QSPI = 2, CRUMB_WHO_WIFIMAP = 3,
  CRUMB_WHO_POWERKEY = 4
};
static const char *crumbWhoName(uint32_t w) {
  switch (w) {
    case CRUMB_WHO_FASTFLUSH: return "fastflush"; case CRUMB_WHO_QSPI: return "qspi";
    case CRUMB_WHO_WIFIMAP: return "wifimap";     case CRUMB_WHO_POWERKEY: return "powerkey";
    default: return "-";
  }
}
static inline void crumbWhoSet(uint32_t who) { crumbMagic = 0xC0FFEE42u; crumbWho = who; }
// setup() 里在打印 Reset reason 之后调用一次
static void crumbBootReport() {
  const int r = (int)esp_reset_reason();
  if (r == ESP_RST_POWERON || crumbMagic != 0xC0FFEE42u) { crumbStage = 0; crumbFrames = 0; crumbMinHeap = 0; crumbMs = 0; crumbWho = 0; }
  crumbLast = String("RR,") + crumbReasonName(r) + "," + String(crumbStage) + "," + String(crumbFrames) + "," +
              String(crumbMinHeap) + "," + String(crumbMs) + "," + crumbWhoName(crumbWho);
  Serial.println(String("[CRUMB] ") + crumbLast + "  (reason,stage,frames,minHeap,ms,who)");
  // musicA.144：只跳过正常上电（每次断电开机都会刷一条，会把真正要查的那条挤掉）。
  // sw 也要存——固件主动重启（WiFi 预装 / 关快刷 / 改 qspi / PWR 菜单）全走 sw，之前正是被漏掉的那一类。
  if (r != ESP_RST_POWERON) {
    Preferences pr; pr.begin("motonav", false); pr.putString("lastrr", crumbLast); pr.end();
  }
  crumbMagic = 0xC0FFEE42u; crumbMinHeap = 0; crumbFrames = 0;
}
// "#RR?" 的回复：优先上次异常重启的存档
static String crumbQueryReply() {
  Preferences pr; pr.begin("motonav", true); String s = pr.getString("lastrr", ""); pr.end();
  return s.length() ? s : crumbLast;
}
