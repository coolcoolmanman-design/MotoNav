#pragma once
// HOST_SIM: ESP32 Arduino HAL surface used by IRAM_ATTR sites and misc helpers.
#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif
#ifndef DRAM_ATTR
#define DRAM_ATTR
#endif

#ifndef ESP_OK
#define ESP_OK 0
#endif
#ifndef ESP_FAIL
#define ESP_FAIL -1
#endif

typedef int esp_err_t;
