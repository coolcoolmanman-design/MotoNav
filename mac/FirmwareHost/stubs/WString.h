#pragma once
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <cctype>

class String {
  std::string s_;
public:
  String() = default;
  String(const char *c) : s_(c ? c : "") {}
  String(const String &o) = default;
  String(String &&o) noexcept = default;
  String(char c) : s_(1, c) {}
  String(unsigned char v, unsigned /*base*/ = 10) { char b[32]; std::snprintf(b, sizeof(b), "%u", (unsigned)v); s_ = b; }
  String(int v, unsigned /*base*/ = 10) { char b[32]; std::snprintf(b, sizeof(b), "%d", v); s_ = b; }
  String(unsigned int v, unsigned /*base*/ = 10) { char b[32]; std::snprintf(b, sizeof(b), "%u", v); s_ = b; }
  String(long v, unsigned /*base*/ = 10) { char b[32]; std::snprintf(b, sizeof(b), "%ld", v); s_ = b; }
  String(unsigned long v, unsigned /*base*/ = 10) { char b[32]; std::snprintf(b, sizeof(b), "%lu", v); s_ = b; }
  String(float v, unsigned dec = 2) {
    char b[64];
    std::snprintf(b, sizeof(b), "%.*f", (int)dec, (double)v);
    s_ = b;
  }
  String(double v, unsigned dec = 2) {
    char b[64];
    std::snprintf(b, sizeof(b), "%.*f", (int)dec, v);
    s_ = b;
  }
  String(const std::string &o) : s_(o) {}

  String &operator=(const String &o) = default;
  String &operator=(String &&o) noexcept = default;
  String &operator=(const char *c) { s_ = c ? c : ""; return *this; }
  String &operator=(char c) { s_.assign(1, c); return *this; }

  unsigned int length() const { return (unsigned)s_.size(); }
  bool isEmpty() const { return s_.empty(); }
  const char *c_str() const { return s_.c_str(); }
  char charAt(unsigned i) const { return i < s_.size() ? s_[i] : 0; }
  char operator[](unsigned i) const { return charAt(i); }
  char &operator[](unsigned i) {
    if (i >= s_.size()) s_.resize(i + 1, '\0');
    return s_[i];
  }

  void clear() { s_.clear(); }
  void reserve(unsigned n) { s_.reserve(n); }

  bool concat(const char *c) { if (c) s_ += c; return true; }
  bool concat(const String &o) { s_ += o.s_; return true; }
  bool concat(char c) { s_ += c; return true; }
  bool concat(int v) { char b[32]; std::snprintf(b, sizeof(b), "%d", v); s_ += b; return true; }
  bool concat(unsigned v) { char b[32]; std::snprintf(b, sizeof(b), "%u", v); s_ += b; return true; }
  bool concat(long v) { char b[32]; std::snprintf(b, sizeof(b), "%ld", v); s_ += b; return true; }
  bool concat(unsigned long v) { char b[32]; std::snprintf(b, sizeof(b), "%lu", v); s_ += b; return true; }
  bool concat(float v) { char b[64]; std::snprintf(b, sizeof(b), "%g", (double)v); s_ += b; return true; }

  String &operator+=(const String &o) { concat(o); return *this; }
  String &operator+=(const char *c) { concat(c); return *this; }
  String &operator+=(char c) { concat(c); return *this; }
  String &operator+=(int v) { concat(v); return *this; }
  String &operator+=(unsigned v) { concat(v); return *this; }
  String &operator+=(long v) { concat(v); return *this; }
  String &operator+=(unsigned long v) { concat(v); return *this; }

  friend String operator+(const String &a, const String &b) { String r(a); r += b; return r; }
  friend String operator+(const String &a, const char *b) { String r(a); r += b; return r; }
  friend String operator+(const char *a, const String &b) { String r(a); r += b; return r; }
  friend String operator+(const String &a, char b) { String r(a); r += b; return r; }
  friend String operator+(char a, const String &b) { String r; r += a; r += b; return r; }
  friend String operator+(const String &a, int b) { String r(a); r += b; return r; }
  friend String operator+(const String &a, unsigned b) { String r(a); r += b; return r; }
  friend String operator+(const String &a, long b) { String r(a); r += b; return r; }
  friend String operator+(const String &a, unsigned long b) { String r(a); r += b; return r; }

  bool equals(const String &o) const { return s_ == o.s_; }
  bool equals(const char *c) const { return s_ == (c ? c : ""); }
  bool operator==(const String &o) const { return equals(o); }
  bool operator==(const char *c) const { return equals(c); }
  bool operator!=(const String &o) const { return !equals(o); }
  bool operator!=(const char *c) const { return !equals(c); }

  int compareTo(const String &o) const { return s_.compare(o.s_); }
  bool startsWith(const String &prefix) const {
    return s_.size() >= prefix.s_.size() && s_.compare(0, prefix.s_.size(), prefix.s_) == 0;
  }
  bool startsWith(const char *prefix) const {
    if (!prefix) return true;
    size_t n = std::strlen(prefix);
    return s_.size() >= n && s_.compare(0, n, prefix) == 0;
  }
  bool endsWith(const String &suffix) const {
    return s_.size() >= suffix.s_.size() &&
           s_.compare(s_.size() - suffix.s_.size(), suffix.s_.size(), suffix.s_) == 0;
  }

  int indexOf(char c, unsigned from = 0) const {
    auto p = s_.find(c, from);
    return p == std::string::npos ? -1 : (int)p;
  }
  int indexOf(const String &o, unsigned from = 0) const {
    auto p = s_.find(o.s_, from);
    return p == std::string::npos ? -1 : (int)p;
  }
  int lastIndexOf(char c) const {
    auto p = s_.rfind(c);
    return p == std::string::npos ? -1 : (int)p;
  }
  String substring(unsigned from) const {
    if (from >= s_.size()) return String();
    return String(s_.substr(from));
  }
  String substring(unsigned from, unsigned to) const {
    if (from >= s_.size()) return String();
    if (to > s_.size()) to = (unsigned)s_.size();
    if (to < from) return String();
    return String(s_.substr(from, to - from));
  }
  void remove(unsigned index) {
    if (index < s_.size()) s_.erase(index);
  }
  void remove(unsigned index, unsigned count) {
    if (index < s_.size()) s_.erase(index, count);
  }
  void replace(const String &from, const String &to) {
    if (from.s_.empty()) return;
    size_t pos = 0;
    while ((pos = s_.find(from.s_, pos)) != std::string::npos) {
      s_.replace(pos, from.s_.size(), to.s_);
      pos += to.s_.size();
    }
  }
  void toLowerCase() {
    for (char &c : s_) c = (char)std::tolower((unsigned char)c);
  }
  void toUpperCase() {
    for (char &c : s_) c = (char)std::toupper((unsigned char)c);
  }
  void trim() {
    size_t a = 0, b = s_.size();
    while (a < b && std::isspace((unsigned char)s_[a])) ++a;
    while (b > a && std::isspace((unsigned char)s_[b - 1])) --b;
    s_ = s_.substr(a, b - a);
  }

  long toInt() const { return std::strtol(s_.c_str(), nullptr, 10); }
  float toFloat() const { return (float)std::strtod(s_.c_str(), nullptr); }
  double toDouble() const { return std::strtod(s_.c_str(), nullptr); }

  void toCharArray(char *buf, unsigned size, unsigned index = 0) const {
    if (!buf || size == 0) return;
    if (index >= s_.size()) { buf[0] = 0; return; }
    size_t n = s_.size() - index;
    if (n >= size) n = size - 1;
    std::memcpy(buf, s_.c_str() + index, n);
    buf[n] = 0;
  }

  operator const char *() const { return c_str(); }
};
