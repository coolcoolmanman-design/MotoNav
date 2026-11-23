/* Strong override of Arduino prebuilt weak esp_app_desc. app_id = project_name. */
#include "esp_app_desc.h"
#include <string.h>

#ifndef MOTONAV_APP_ID
#define MOTONAV_APP_ID "MotoNav"
#endif
#ifndef MOTONAV_APP_VERSION
#define MOTONAV_APP_VERSION "7.12.3-sdgoods"
#endif

__attribute__((section(".rodata_desc")))
esp_app_desc_t esp_app_desc = {
    .magic_word = ESP_APP_DESC_MAGIC_WORD,
    .secure_version = 0,
    .reserv1 = {0},
    .version = MOTONAV_APP_VERSION,
    .project_name = MOTONAV_APP_ID,
    .time = __TIME__,
    .date = __DATE__,
#if defined(IDF_VER)
    .idf_ver = IDF_VER,
#else
    .idf_ver = "arduino-esp32",
#endif
};
