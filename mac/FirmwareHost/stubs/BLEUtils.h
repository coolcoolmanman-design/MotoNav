#pragma once
#include <cstdint>
#include "BLEUUID.h"

#ifndef ESP_IO_CAP_OUT
#define ESP_IO_CAP_OUT 0x00
#endif
#ifndef ESP_IO_CAP_IO
#define ESP_IO_CAP_IO 0x01
#endif
#ifndef ESP_IO_CAP_IN
#define ESP_IO_CAP_IN 0x02
#endif
#ifndef ESP_IO_CAP_NONE
#define ESP_IO_CAP_NONE 0x03
#endif
#ifndef GENERIC_HID
#define GENERIC_HID 0x03C0
#endif

class BLEUtils {
public:
  static const char *buildHexData(char *buf, const uint8_t *data, uint32_t len) {
    if (!buf) return "";
    buf[0] = 0;
    (void)data; (void)len;
    return buf;
  }
};

#ifndef GENERIC_HID
#define GENERIC_HID 0x03C0
#endif
#ifndef ESP_IO_CAP_OUT
#define ESP_IO_CAP_OUT 0
#endif
#ifndef ESP_IO_CAP_IO
#define ESP_IO_CAP_IO 1
#endif
#ifndef ESP_IO_CAP_IN
#define ESP_IO_CAP_IN 2
#endif
#ifndef ESP_IO_CAP_NONE
#define ESP_IO_CAP_NONE 3
#endif
#ifndef ESP_IO_CAP_KBDISP
#define ESP_IO_CAP_KBDISP 4
#endif
