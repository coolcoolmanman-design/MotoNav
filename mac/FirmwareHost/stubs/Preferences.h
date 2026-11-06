#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <mutex>
#include "WString.h"

// HOST_SIM in-memory NVS stand-in (process lifetime only).
class Preferences {
  std::string ns_;
  bool ro_ = true;
  bool open_ = false;

  static std::mutex &mtx() {
    static std::mutex m;
    return m;
  }
  static std::unordered_map<std::string, std::string> &store() {
    static std::unordered_map<std::string, std::string> s;
    return s;
  }
  static std::string keyOf(const std::string &ns, const char *key) {
    return ns + "/" + (key ? key : "");
  }
  template <typename T>
  bool putBytesT(const char *key, const T &v) {
    if (!open_ || ro_ || !key) return false;
    std::lock_guard<std::mutex> lock(mtx());
    store()[keyOf(ns_, key)] = std::string((const char *)&v, sizeof(T));
    return true;
  }
  template <typename T>
  T getBytesT(const char *key, T def) const {
    if (!open_ || !key) return def;
    std::lock_guard<std::mutex> lock(mtx());
    auto it = store().find(keyOf(ns_, key));
    if (it == store().end() || it->second.size() != sizeof(T)) return def;
    T out{};
    std::memcpy(&out, it->second.data(), sizeof(T));
    return out;
  }

public:
  bool begin(const char *name, bool readOnly = false) {
    ns_ = name ? name : "default";
    ro_ = readOnly;
    open_ = true;
    return true;
  }
  void end() { open_ = false; }
  bool clear() {
    if (!open_ || ro_) return false;
    std::lock_guard<std::mutex> lock(mtx());
    std::string prefix = ns_ + "/";
    for (auto it = store().begin(); it != store().end();) {
      if (it->first.rfind(prefix, 0) == 0) it = store().erase(it);
      else ++it;
    }
    return true;
  }
  bool remove(const char *key) {
    if (!open_ || ro_ || !key) return false;
    std::lock_guard<std::mutex> lock(mtx());
    return store().erase(keyOf(ns_, key)) > 0;
  }
  bool isKey(const char *key) const {
    if (!open_ || !key) return false;
    std::lock_guard<std::mutex> lock(mtx());
    return store().find(keyOf(ns_, key)) != store().end();
  }

  // ESP Preferences returns written byte count (0 on failure). FW compares to sizeof(...).
  size_t putBool(const char *key, bool v) {
    return putBytesT(key, (uint8_t)(v ? 1 : 0)) ? sizeof(uint8_t) : 0;
  }
  size_t putChar(const char *key, int8_t v) { return putBytesT(key, v) ? sizeof(v) : 0; }
  size_t putUChar(const char *key, uint8_t v) { return putBytesT(key, v) ? sizeof(v) : 0; }
  size_t putShort(const char *key, int16_t v) { return putBytesT(key, v) ? sizeof(v) : 0; }
  size_t putUShort(const char *key, uint16_t v) { return putBytesT(key, v) ? sizeof(v) : 0; }
  size_t putInt(const char *key, int32_t v) { return putBytesT(key, v) ? sizeof(v) : 0; }
  size_t putUInt(const char *key, uint32_t v) { return putBytesT(key, v) ? sizeof(v) : 0; }
  size_t putLong(const char *key, int32_t v) { return putBytesT(key, v) ? sizeof(v) : 0; }
  size_t putULong(const char *key, uint32_t v) { return putBytesT(key, v) ? sizeof(v) : 0; }
  size_t putLong64(const char *key, int64_t v) { return putBytesT(key, v) ? sizeof(v) : 0; }
  size_t putULong64(const char *key, uint64_t v) { return putBytesT(key, v) ? sizeof(v) : 0; }
  size_t putFloat(const char *key, float v) { return putBytesT(key, v) ? sizeof(v) : 0; }
  size_t putDouble(const char *key, double v) { return putBytesT(key, v) ? sizeof(v) : 0; }
  size_t putString(const char *key, const char *v) {
    if (!open_ || ro_ || !key) return 0;
    std::lock_guard<std::mutex> lock(mtx());
    store()[keyOf(ns_, key)] = v ? v : "";
    return store()[keyOf(ns_, key)].size();
  }
  size_t putBytes(const char *key, const void *value, size_t len) {
    if (!open_ || ro_ || !key) return 0;
    std::lock_guard<std::mutex> lock(mtx());
    store()[keyOf(ns_, key)] = std::string((const char *)value, len);
    return len;
  }

  bool getBool(const char *key, bool def = false) const {
    return getBytesT<uint8_t>(key, (uint8_t)(def ? 1 : 0)) != 0;
  }
  int8_t getChar(const char *key, int8_t def = 0) const { return getBytesT(key, def); }
  uint8_t getUChar(const char *key, uint8_t def = 0) const { return getBytesT(key, def); }
  int16_t getShort(const char *key, int16_t def = 0) const { return getBytesT(key, def); }
  uint16_t getUShort(const char *key, uint16_t def = 0) const { return getBytesT(key, def); }
  int32_t getInt(const char *key, int32_t def = 0) const { return getBytesT(key, def); }
  uint32_t getUInt(const char *key, uint32_t def = 0) const { return getBytesT(key, def); }
  int32_t getLong(const char *key, int32_t def = 0) const { return getBytesT(key, def); }
  uint32_t getULong(const char *key, uint32_t def = 0) const { return getBytesT(key, def); }
  int64_t getLong64(const char *key, int64_t def = 0) const { return getBytesT(key, def); }
  uint64_t getULong64(const char *key, uint64_t def = 0) const { return getBytesT(key, def); }
  float getFloat(const char *key, float def = 0) const { return getBytesT(key, def); }
  double getDouble(const char *key, double def = 0) const { return getBytesT(key, def); }
  String getString(const char *key, const char *def = "") const {
    if (!open_ || !key) return String(def);
    std::lock_guard<std::mutex> lock(mtx());
    auto it = store().find(keyOf(ns_, key));
    if (it == store().end()) return String(def);
    return String(it->second.c_str());
  }
  size_t getBytesLength(const char *key) const {
    if (!open_ || !key) return 0;
    std::lock_guard<std::mutex> lock(mtx());
    auto it = store().find(keyOf(ns_, key));
    return it == store().end() ? 0 : it->second.size();
  }
  size_t getBytes(const char *key, void *buf, size_t maxLen) const {
    if (!open_ || !key || !buf || !maxLen) return 0;
    std::lock_guard<std::mutex> lock(mtx());
    auto it = store().find(keyOf(ns_, key));
    if (it == store().end()) return 0;
    size_t n = it->second.size() < maxLen ? it->second.size() : maxLen;
    std::memcpy(buf, it->second.data(), n);
    return n;
  }
};
