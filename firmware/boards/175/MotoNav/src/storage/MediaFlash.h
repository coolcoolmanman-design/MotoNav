#pragma once
// musicA.46：壁纸 (type 8) / 电子吧唧 (type 7) 落盘到已有 ffat（~9.9 MiB）。
// NVS 只存标量；位图走 FAT 文件。挂载失败时静默回退为 RAM-only（旧行为）。
#include <FFat.h>
#include <FS.h>

namespace MediaFlash {
static bool mounted = false;
static bool mountTried = false;

inline bool ensureMounted() {
  if (mounted) return true;
  if (mountTried && !mounted) return false;
  mountTried = true;
  if (FFat.begin(false)) { mounted = true; return true; }
  // 首次：允许格式化一次空分区
  if (FFat.begin(true)) { mounted = true; Serial.println("[FS] ffat formatted"); return true; }
  Serial.println("[FS] ffat mount failed");
  return false;
}

inline void ensureDir(const char *path) {
  if (!FFat.exists(path)) FFat.mkdir(path);
}

inline bool writeAtomic(const char *finalPath, const char *tmpPath, const uint8_t *data, size_t len) {
  if (!ensureMounted() || !data || !len) return false;
  ensureDir("/bg");
  ensureDir("/badge");
  ensureDir("/center");
  File f = FFat.open(tmpPath, FILE_WRITE);
  if (!f) return false;
  size_t n = f.write(data, len);
  f.close();
  if (n != len) { FFat.remove(tmpPath); return false; }
  FFat.remove(finalPath);
  if (!FFat.rename(tmpPath, finalPath)) { FFat.remove(tmpPath); return false; }
  return true;
}

inline bool readFile(const char *path, uint8_t **out, size_t *outLen) {
  *out = nullptr; *outLen = 0;
  if (!ensureMounted() || !FFat.exists(path)) return false;
  File f = FFat.open(path, FILE_READ);
  if (!f) return false;
  size_t n = f.size();
  if (n < 8 || n > (size_t)500000) { f.close(); return false; }
  uint8_t *buf = (uint8_t *)ps_malloc(n);
  if (!buf) { f.close(); return false; }
  size_t got = f.read(buf, n);
  f.close();
  if (got != n) { free(buf); return false; }
  *out = buf; *outLen = n;
  return true;
}

inline bool saveBgWire(const uint8_t *wire, size_t len) {
  return writeAtomic("/bg/custom.bgr", "/bg/custom.tmp", wire, len);
}

inline bool clearBg() {
  if (!ensureMounted()) return false;
  FFat.remove("/bg/custom.bgr");
  FFat.remove("/bg/custom.tmp");
  return true;
}

inline bool loadBgIntoParse(void (*parse)(const uint8_t *, uint32_t)) {
  uint8_t *buf = nullptr; size_t n = 0;
  if (!readFile("/bg/custom.bgr", &buf, &n)) return false;
  parse(buf, (uint32_t)n);
  free(buf);
  return bgCustomOn;
}

inline bool saveBadgeWire(int slot, const uint8_t *wire, size_t len) {
  if (slot < 0 || slot >= BADGE_SLOTS) return false;
  char finalP[32], tmpP[32];
  snprintf(finalP, sizeof(finalP), "/badge/slot%d.bgr", slot);
  snprintf(tmpP, sizeof(tmpP), "/badge/slot%d.tmp", slot);
  return writeAtomic(finalP, tmpP, wire, len);
}

inline bool clearBadgeSlot(int slot) {
  if (!ensureMounted() || slot < 0 || slot >= BADGE_SLOTS) return false;
  char finalP[32], tmpP[32];
  snprintf(finalP, sizeof(finalP), "/badge/slot%d.bgr", slot);
  snprintf(tmpP, sizeof(tmpP), "/badge/slot%d.tmp", slot);
  FFat.remove(finalP); FFat.remove(tmpP);
  return true;
}

inline bool clearAllBadges() {
  for (int i = 0; i < BADGE_SLOTS; ++i) clearBadgeSlot(i);
  return true;
}

inline int loadBadgesIntoParse(void (*parse)(const uint8_t *, uint32_t)) {
  int loaded = 0;
  for (int slot = 0; slot < BADGE_SLOTS; ++slot) {
    char path[32];
    snprintf(path, sizeof(path), "/badge/slot%d.bgr", slot);
    uint8_t *buf = nullptr; size_t n = 0;
    if (!readFile(path, &buf, &n)) continue;
    parse(buf, (uint32_t)n);
    free(buf);
    if (badgeSlotOk[slot]) ++loaded;
  }
  return loaded;
}

inline uint8_t badgeCacheMask() {
  uint8_t m = 0;
  for (int i = 0; i < BADGE_SLOTS; ++i) if (badgeSlotOk[i]) m |= (uint8_t)(1u << i);
  return m;
}

// musicA.50：中央自定义精灵 sleep/chase 双文件
inline const char *cspPath(int role) {
  return role == CSP_ROLE_CHASE ? "/center/chase.csp" : "/center/sleep.csp";
}
inline const char *cspTmpPath(int role) {
  return role == CSP_ROLE_CHASE ? "/center/chase.tmp" : "/center/sleep.tmp";
}

inline bool saveCspWire(int role, const uint8_t *wire, size_t len) {
  if (role != CSP_ROLE_SLEEP && role != CSP_ROLE_CHASE) return false;
  return writeAtomic(cspPath(role), cspTmpPath(role), wire, len);
}

inline bool clearCspRole(int role) {
  if (!ensureMounted()) return false;
  if (role == 0) {
    FFat.remove("/center/sleep.csp"); FFat.remove("/center/sleep.tmp");
    FFat.remove("/center/chase.csp"); FFat.remove("/center/chase.tmp");
    return true;
  }
  if (role != CSP_ROLE_SLEEP && role != CSP_ROLE_CHASE) return false;
  FFat.remove(cspPath(role));
  FFat.remove(cspTmpPath(role));
  return true;
}

inline int loadCspIntoParse(void (*parse)(const uint8_t *, uint32_t)) {
  int loaded = 0;
  for (int role = CSP_ROLE_SLEEP; role <= CSP_ROLE_CHASE; ++role) {
    uint8_t *buf = nullptr; size_t n = 0;
    if (!readFile(cspPath(role), &buf, &n)) continue;
    parse(buf, (uint32_t)n);
    free(buf);
    ++loaded;
  }
  return loaded;
}
} // namespace MediaFlash
