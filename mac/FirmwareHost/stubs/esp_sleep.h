#pragma once
#include <cstdint>
#include "esp_system.h"

typedef enum {
  ESP_SLEEP_WAKEUP_UNDEFINED = 0,
  ESP_SLEEP_WAKEUP_ALL,
  ESP_SLEEP_WAKEUP_EXT0,
  ESP_SLEEP_WAKEUP_EXT1,
  ESP_SLEEP_WAKEUP_TIMER,
  ESP_SLEEP_WAKEUP_TOUCHPAD,
  ESP_SLEEP_WAKEUP_ULP,
  ESP_SLEEP_WAKEUP_GPIO,
  ESP_SLEEP_WAKEUP_UART
} esp_sleep_source_t;

typedef enum {
  ESP_SLEEP_WAKEUP_CAUSE_UNDEFINED = 0
} esp_sleep_wakeup_cause_t;

inline esp_sleep_wakeup_cause_t esp_sleep_get_wakeup_cause() {
  return (esp_sleep_wakeup_cause_t)ESP_SLEEP_WAKEUP_UNDEFINED;
}
inline esp_err_t esp_sleep_disable_wakeup_source(esp_sleep_source_t) { return ESP_OK; }
inline esp_err_t esp_sleep_enable_ext0_wakeup(int /*gpio*/, int /*level*/) { return ESP_OK; }
inline esp_err_t esp_sleep_enable_timer_wakeup(uint64_t /*us*/) { return ESP_OK; }
inline void esp_deep_sleep_start() {}
inline void esp_light_sleep_start() {}
