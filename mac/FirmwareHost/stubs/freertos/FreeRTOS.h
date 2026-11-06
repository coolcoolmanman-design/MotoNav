#pragma once
#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>

#ifndef pdTRUE
#define pdTRUE 1
#endif
#ifndef pdFALSE
#define pdFALSE 0
#endif
#ifndef pdPASS
#define pdPASS pdTRUE
#endif
#ifndef pdFAIL
#define pdFAIL pdFALSE
#endif
#ifndef portMAX_DELAY
#define portMAX_DELAY ((uint32_t)0xffffffffu)
#endif
#ifndef portTICK_PERIOD_MS
#define portTICK_PERIOD_MS 1
#endif

typedef void *TaskHandle_t;
typedef int BaseType_t;
typedef uint32_t TickType_t;
typedef uint32_t UBaseType_t;

inline TickType_t pdMS_TO_TICKS(uint32_t ms) { return ms; }

struct portMUX_TYPE {
  std::mutex m;
};
// Brace-init only — std::mutex is neither copyable nor movable.
#define portMUX_INITIALIZER_UNLOCKED {}

inline void portENTER_CRITICAL(portMUX_TYPE *mux) {
  if (mux) mux->m.lock();
}
inline void portEXIT_CRITICAL(portMUX_TYPE *mux) {
  if (mux) mux->m.unlock();
}
inline void portENTER_CRITICAL_ISR(portMUX_TYPE *mux) { portENTER_CRITICAL(mux); }
inline void portEXIT_CRITICAL_ISR(portMUX_TYPE *mux) { portEXIT_CRITICAL(mux); }

inline void vTaskDelay(TickType_t ticks) {
  std::this_thread::sleep_for(std::chrono::milliseconds(ticks));
}
inline void vTaskDelayUntil(TickType_t *prev, TickType_t inc) {
  if (!prev) return;
  *prev += inc;
  std::this_thread::sleep_for(std::chrono::milliseconds(inc));
}
inline TickType_t xTaskGetTickCount() {
  using clock = std::chrono::steady_clock;
  static const auto t0 = clock::now();
  return (TickType_t)std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - t0).count();
}
inline void vTaskPrioritySet(TaskHandle_t, UBaseType_t) {}
inline void vTaskDelete(TaskHandle_t) {}
inline TaskHandle_t xTaskGetCurrentTaskHandle() { return nullptr; }

using TaskFunction_t = void (*)(void *);

inline BaseType_t xTaskCreate(TaskFunction_t fn, const char * /*name*/, uint32_t /*stack*/,
                              void *arg, UBaseType_t /*prio*/, TaskHandle_t *out) {
  if (!fn) return pdFAIL;
  std::thread([fn, arg]() { fn(arg); }).detach();
  if (out) *out = (TaskHandle_t)1;
  return pdPASS;
}

inline BaseType_t xTaskCreatePinnedToCore(TaskFunction_t fn, const char *name, uint32_t stack,
                                          void *arg, UBaseType_t prio, TaskHandle_t *out,
                                          BaseType_t /*core*/) {
  return xTaskCreate(fn, name, stack, arg, prio, out);
}
