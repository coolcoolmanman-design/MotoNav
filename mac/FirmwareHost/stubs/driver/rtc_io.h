#pragma once
#include <cstdint>
#include "esp_system.h"

typedef enum {
  GPIO_NUM_NC = -1,
  GPIO_NUM_0 = 0,
  GPIO_NUM_1 = 1,
  GPIO_NUM_11 = 11,
  GPIO_NUM_MAX = 48
} gpio_num_t;

typedef enum {
  RTC_GPIO_MODE_INPUT_ONLY = 0,
  RTC_GPIO_MODE_OUTPUT_ONLY,
  RTC_GPIO_MODE_INPUT_OUTPUT,
  RTC_GPIO_MODE_DISABLED
} rtc_gpio_mode_t;

inline esp_err_t rtc_gpio_init(gpio_num_t) { return ESP_OK; }
inline esp_err_t rtc_gpio_deinit(gpio_num_t) { return ESP_OK; }
inline esp_err_t rtc_gpio_set_direction(gpio_num_t, rtc_gpio_mode_t) { return ESP_OK; }
inline esp_err_t rtc_gpio_pullup_en(gpio_num_t) { return ESP_OK; }
inline esp_err_t rtc_gpio_pullup_dis(gpio_num_t) { return ESP_OK; }
inline esp_err_t rtc_gpio_pulldown_en(gpio_num_t) { return ESP_OK; }
inline esp_err_t rtc_gpio_pulldown_dis(gpio_num_t) { return ESP_OK; }
inline int rtc_gpio_get_level(gpio_num_t) { return 1; }
inline esp_err_t rtc_gpio_set_level(gpio_num_t, uint32_t) { return ESP_OK; }
