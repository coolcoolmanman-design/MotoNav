#pragma once
#ifndef HOST_SIM
#define HOST_SIM 1
#endif

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <cctype>
#include <ctime>
#include <chrono>
#include <thread>
#include <algorithm>
#include <string>

#include "WString.h"
#include "Print.h"
#include "Stream.h"
#include "HardwareSerial.h"

#ifndef HIGH
#define HIGH 1
#endif
#ifndef LOW
#define LOW 0
#endif
#ifndef INPUT
#define INPUT 0x01
#endif
#ifndef OUTPUT
#define OUTPUT 0x02
#endif
#ifndef INPUT_PULLUP
#define INPUT_PULLUP 0x03
#endif
#ifndef FALLING
#define FALLING 0x02
#endif
#ifndef RISING
#define RISING 0x03
#endif
#ifndef CHANGE
#define CHANGE 0x01
#endif
#ifndef LED_BUILTIN
#define LED_BUILTIN 0
#endif

#ifndef PI
#define PI 3.14159265358979323846
#endif

#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif
#ifndef DRAM_ATTR
#define DRAM_ATTR
#endif
#ifndef ICACHE_FLASH_ATTR
#define ICACHE_FLASH_ATTR
#endif
#ifndef PROGMEM
#define PROGMEM
#endif
#ifndef PGM_P
#define PGM_P const char *
#endif
#ifndef pgm_read_byte
#define pgm_read_byte(addr) (*(const unsigned char *)(addr))
#endif
#ifndef pgm_read_word
#define pgm_read_word(addr) (*(const unsigned short *)(addr))
#endif
#ifndef pgm_read_dword
#define pgm_read_dword(addr) (*(const unsigned long *)(addr))
#endif
#ifndef memcpy_P
#define memcpy_P memcpy
#endif
#ifndef strcpy_P
#define strcpy_P strcpy
#endif
#ifndef strlen_P
#define strlen_P strlen
#endif
#ifndef F
#define F(str) (str)
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_heap_caps.h"

// Arduino-style min/max macros (promote mixed integer widths like on ESP cores).
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
using std::abs;

inline unsigned long millis() {
  using clock = std::chrono::steady_clock;
  static const auto t0 = clock::now();
  return (unsigned long)std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - t0).count();
}

inline uint32_t micros() {
  using clock = std::chrono::steady_clock;
  static const auto t0 = clock::now();
  return (uint32_t)std::chrono::duration_cast<std::chrono::microseconds>(clock::now() - t0).count();
}

inline void delay(uint32_t ms) {
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

inline void delayMicroseconds(uint32_t us) {
  std::this_thread::sleep_for(std::chrono::microseconds(us));
}

inline void yield() { std::this_thread::yield(); }

inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}
inline int digitalRead(int) { return HIGH; }
inline int analogRead(int) { return 0; }
inline void analogWrite(int, int) {}
inline void attachInterrupt(int, void (*)(), int) {}
inline void detachInterrupt(int) {}
inline long map(long x, long in_min, long in_max, long out_min, long out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
inline long random(long maxv) { return maxv <= 0 ? 0 : (long)(::rand() % (int)maxv); }
inline long random(long minv, long maxv) {
  if (maxv <= minv) return minv;
  return minv + (long)(::rand() % (int)(maxv - minv));
}
inline void randomSeed(unsigned long s) { ::srand((unsigned)s); }

#ifndef bit
#define bit(b) (1UL << (b))
#endif
#ifndef bitRead
#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#endif
#ifndef bitSet
#define bitSet(value, bit) ((value) |= (1UL << (bit)))
#endif
#ifndef bitClear
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
#endif
#ifndef bitWrite
#define bitWrite(value, bit, bitvalue) (bitvalue ? bitSet(value, bit) : bitClear(value, bit))
#endif

#ifndef constrain
#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))
#endif

#ifndef sq
#define sq(x) ((x) * (x))
#endif

#ifndef DEG_TO_RAD
#define DEG_TO_RAD 0.017453292519943295769236907684886
#endif
#ifndef RAD_TO_DEG
#define RAD_TO_DEG 57.295779513082320876798154814105
#endif

struct EspClass {
  void restart() {}
  uint32_t getFreeHeap() const { return 64u * 1024u * 1024u; }
  uint32_t getHeapSize() const { return 64u * 1024u * 1024u; }
  uint32_t getPsramSize() const { return 8u * 1024u * 1024u; }
  uint32_t getFreePsram() const { return 8u * 1024u * 1024u; }
  uint8_t getCpuFreqMHz() const { return 240; }
};
static EspClass ESP;

inline void *ps_malloc(size_t n) { return std::malloc(n); }
inline void *ps_calloc(size_t n, size_t sz) { return std::calloc(n, sz); }
inline void *ps_realloc(void *p, size_t n) { return std::realloc(p, n); }

#include "esp32-hal.h"
