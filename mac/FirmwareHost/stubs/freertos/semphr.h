#pragma once
#include "FreeRTOS.h"
#include <memory>

struct HostSemaphore {
  std::mutex m;
  std::condition_variable cv;
  int count = 0;
  bool binary = false;
};

typedef HostSemaphore *SemaphoreHandle_t;

inline SemaphoreHandle_t xSemaphoreCreateBinary() {
  auto *s = new HostSemaphore();
  s->binary = true;
  s->count = 0;
  return s;
}
inline SemaphoreHandle_t xSemaphoreCreateMutex() {
  auto *s = new HostSemaphore();
  s->binary = false;
  s->count = 1;
  return s;
}
inline SemaphoreHandle_t xSemaphoreCreateCounting(UBaseType_t /*max*/, UBaseType_t initial) {
  auto *s = new HostSemaphore();
  s->binary = false;
  s->count = (int)initial;
  return s;
}
inline void vSemaphoreDelete(SemaphoreHandle_t s) { delete s; }

inline BaseType_t xSemaphoreTake(SemaphoreHandle_t s, TickType_t ticks) {
  if (!s) return pdFALSE;
  std::unique_lock<std::mutex> lock(s->m);
  if (ticks == 0) {
    if (s->count <= 0) return pdFALSE;
    --s->count;
    return pdTRUE;
  }
  if (ticks == portMAX_DELAY) {
    s->cv.wait(lock, [&] { return s->count > 0; });
    --s->count;
    return pdTRUE;
  }
  bool ok = s->cv.wait_for(lock, std::chrono::milliseconds(ticks), [&] { return s->count > 0; });
  if (!ok) return pdFALSE;
  --s->count;
  return pdTRUE;
}

inline BaseType_t xSemaphoreGive(SemaphoreHandle_t s) {
  if (!s) return pdFALSE;
  {
    std::lock_guard<std::mutex> lock(s->m);
    if (s->binary) s->count = 1;
    else ++s->count;
  }
  s->cv.notify_one();
  return pdTRUE;
}

inline BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t s, BaseType_t * /*woken*/) {
  return xSemaphoreGive(s);
}
inline BaseType_t xSemaphoreTakeFromISR(SemaphoreHandle_t s, BaseType_t * /*woken*/) {
  return xSemaphoreTake(s, 0);
}
