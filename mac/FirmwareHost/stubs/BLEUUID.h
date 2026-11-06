#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <cstdio>

class BLEUUID {
  std::string uuid_;
public:
  BLEUUID() = default;
  BLEUUID(const char *uuid) : uuid_(uuid ? uuid : "") {}
  BLEUUID(uint16_t uuid16) {
    char b[16];
    std::snprintf(b, sizeof(b), "%04x", (unsigned)uuid16);
    uuid_ = b;
  }
  BLEUUID(uint32_t uuid32) {
    char b[16];
    std::snprintf(b, sizeof(b), "%08x", (unsigned)uuid32);
    uuid_ = b;
  }
  const char *toString() const { return uuid_.c_str(); }
  bool equals(const BLEUUID &o) const { return uuid_ == o.uuid_; }
  bool operator==(const BLEUUID &o) const { return equals(o); }
};
