#pragma once
#include "FreeRTOS.h"
#include <deque>
#include <vector>

struct HostQueue {
  std::mutex m;
  std::condition_variable cv;
  size_t itemSize = 0;
  size_t capacity = 0;
  std::deque<std::vector<uint8_t>> items;
};

typedef HostQueue *QueueHandle_t;

inline QueueHandle_t xQueueCreate(UBaseType_t len, UBaseType_t itemSize) {
  auto *q = new HostQueue();
  q->capacity = len;
  q->itemSize = itemSize;
  return q;
}
inline void vQueueDelete(QueueHandle_t q) { delete q; }

inline BaseType_t xQueueSend(QueueHandle_t q, const void *item, TickType_t ticks) {
  if (!q || !item) return pdFALSE;
  std::unique_lock<std::mutex> lock(q->m);
  auto waitOk = [&] {
    if (ticks == 0) return q->items.size() < q->capacity;
    if (ticks == portMAX_DELAY) {
      q->cv.wait(lock, [&] { return q->items.size() < q->capacity; });
      return true;
    }
    return q->cv.wait_for(lock, std::chrono::milliseconds(ticks),
                          [&] { return q->items.size() < q->capacity; });
  };
  if (!waitOk()) return pdFALSE;
  std::vector<uint8_t> bytes(q->itemSize);
  std::memcpy(bytes.data(), item, q->itemSize);
  q->items.push_back(std::move(bytes));
  q->cv.notify_one();
  return pdTRUE;
}
inline BaseType_t xQueueSendToBack(QueueHandle_t q, const void *item, TickType_t ticks) {
  return xQueueSend(q, item, ticks);
}
inline BaseType_t xQueueReceive(QueueHandle_t q, void *buf, TickType_t ticks) {
  if (!q || !buf) return pdFALSE;
  std::unique_lock<std::mutex> lock(q->m);
  auto waitOk = [&] {
    if (ticks == 0) return !q->items.empty();
    if (ticks == portMAX_DELAY) {
      q->cv.wait(lock, [&] { return !q->items.empty(); });
      return true;
    }
    return q->cv.wait_for(lock, std::chrono::milliseconds(ticks),
                          [&] { return !q->items.empty(); });
  };
  if (!waitOk()) return pdFALSE;
  auto &front = q->items.front();
  std::memcpy(buf, front.data(), q->itemSize);
  q->items.pop_front();
  q->cv.notify_one();
  return pdTRUE;
}
inline UBaseType_t uxQueueMessagesWaiting(QueueHandle_t q) {
  if (!q) return 0;
  std::lock_guard<std::mutex> lock(q->m);
  return (UBaseType_t)q->items.size();
}
