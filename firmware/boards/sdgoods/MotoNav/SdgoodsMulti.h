#pragma once
// Minimal MULTI-mode contract for Arduino guest firmware (no sdgoods_board link).
// Equivalent intent to sdgoods_return_to_launcher(): set boot=factory + restart
// only when this image is a launcher-managed app (running from ota_N and factory
// holds SDGOODS_LAUNCHER).

#include <Arduino.h>
#include <string.h>
#include <esp_app_desc.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_system.h>

#ifndef SDGOODS_LAUNCHER_APP_ID
#define SDGOODS_LAUNCHER_APP_ID "SDGOODS_LAUNCHER"
#endif

static inline bool sdgoodsDescIsLauncher(const esp_app_desc_t *d) {
  return d && d->project_name[0] &&
         strcmp(d->project_name, SDGOODS_LAUNCHER_APP_ID) == 0;
}

static inline bool sdgoodsFactoryHoldsLauncher() {
  const esp_partition_t *f = esp_partition_find_first(
      ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, nullptr);
  if (!f) return false;
  esp_app_desc_t d{};
  if (esp_ota_get_partition_description(f, &d) != ESP_OK) return false;
  return sdgoodsDescIsLauncher(&d);
}

/** True when running from an ota slot on a device whose factory holds the launcher. */
static inline bool sdgoodsIsManagedApp() {
  const esp_partition_t *run = esp_ota_get_running_partition();
  if (!run || run->subtype == ESP_PARTITION_SUBTYPE_APP_FACTORY) return false;
  return sdgoodsFactoryHoldsLauncher();
}

/**
 * Return to launcher (factory) and restart. Matches sdgoods_return_to_launcher()
 * permission gate: refuse (no restart) unless this is a managed MULTI guest.
 * @param dimDisplay optional brightness callback before restart (may be nullptr)
 */
static inline bool sdgoodsReturnToLauncher(void (*dimDisplay)() = nullptr) {
  if (!sdgoodsIsManagedApp()) {
    const esp_app_desc_t *d = esp_app_get_description();
    const esp_partition_t *run = esp_ota_get_running_partition();
    Serial.printf(
        "return-to-launcher refused: not a launcher-managed app "
        "(boot=%s subtype=0x%02x app='%s')\n",
        run && run->label ? run->label : "?",
        run ? (unsigned)run->subtype : 0u,
        (d && d->project_name[0]) ? d->project_name : "unknown");
    return false;
  }
  const esp_partition_t *factory = esp_partition_find_first(
      ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, nullptr);
  if (!factory) {
    Serial.println("return-to-launcher: factory partition not found");
    return false;
  }
  esp_err_t err = esp_ota_set_boot_partition(factory);
  if (err != ESP_OK) {
    Serial.printf("return-to-launcher set_boot failed: %s\n", esp_err_to_name(err));
    return false;
  }
  Serial.println("return-to-launcher: boot=factory, restarting");
  if (dimDisplay) dimDisplay();
  esp_restart();
  return true;  // unreachable
}
