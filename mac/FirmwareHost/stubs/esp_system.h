#pragma once
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <stdlib.h>

#ifndef ESP_OK
#define ESP_OK 0
#endif
#ifndef ESP_FAIL
#define ESP_FAIL -1
#endif
#ifndef ESP_ERR_NO_MEM
#define ESP_ERR_NO_MEM 0x101
#endif

typedef int esp_err_t;

typedef enum {
  ESP_RST_UNKNOWN = 0,
  ESP_RST_POWERON,
  ESP_RST_EXT,
  ESP_RST_SW,
  ESP_RST_PANIC,
  ESP_RST_INT_WDT,
  ESP_RST_TASK_WDT,
  ESP_RST_WDT,
  ESP_RST_DEEPSLEEP,
  ESP_RST_BROWNOUT,
  ESP_RST_SDIO
} esp_reset_reason_t;

inline esp_reset_reason_t esp_reset_reason() { return ESP_RST_POWERON; }
inline void esp_restart() {}

inline uint32_t esp_random() {
  return ((uint32_t)::rand() << 16) ^ (uint32_t)::rand() ^ (uint32_t)::time(nullptr);
}
inline void esp_fill_random(void *buf, size_t len) {
  auto *p = (uint8_t *)buf;
  for (size_t i = 0; i < len; ++i) p[i] = (uint8_t)(esp_random() & 0xff);
}

// Heap caps stand-ins
#ifndef MALLOC_CAP_EXEC
#define MALLOC_CAP_EXEC (1 << 0)
#endif
#ifndef MALLOC_CAP_32BIT
#define MALLOC_CAP_32BIT (1 << 1)
#endif
#ifndef MALLOC_CAP_8BIT
#define MALLOC_CAP_8BIT (1 << 2)
#endif
#ifndef MALLOC_CAP_DMA
#define MALLOC_CAP_DMA (1 << 3)
#endif
#ifndef MALLOC_CAP_PID2
#define MALLOC_CAP_PID2 (1 << 4)
#endif
#ifndef MALLOC_CAP_INTERNAL
#define MALLOC_CAP_INTERNAL (1 << 11)
#endif
#ifndef MALLOC_CAP_DEFAULT
#define MALLOC_CAP_DEFAULT (1 << 12)
#endif
#ifndef MALLOC_CAP_SPIRAM
#define MALLOC_CAP_SPIRAM (1 << 10)
#endif

inline void *heap_caps_malloc(size_t size, uint32_t /*caps*/) { return std::malloc(size); }
inline void *heap_caps_calloc(size_t n, size_t size, uint32_t /*caps*/) { return std::calloc(n, size); }
inline void heap_caps_free(void *p) { std::free(p); }
inline void *heap_caps_realloc(void *p, size_t size, uint32_t /*caps*/) { return std::realloc(p, size); }
inline void *heap_caps_aligned_alloc(size_t alignment, size_t size, uint32_t /*caps*/) {
#if defined(__APPLE__) || defined(__linux__)
  void *p = nullptr;
  if (alignment < sizeof(void *)) alignment = sizeof(void *);
  if (posix_memalign(&p, alignment, size) != 0) return nullptr;
  return p;
#else
  (void)alignment;
  return std::malloc(size);
#endif
}
inline size_t heap_caps_get_free_size(uint32_t /*caps*/) { return 64u * 1024u * 1024u; }
inline size_t heap_caps_get_largest_free_block(uint32_t /*caps*/) { return 32u * 1024u * 1024u; }
inline size_t heap_caps_get_minimum_free_size(uint32_t /*caps*/) { return 16u * 1024u * 1024u; }
