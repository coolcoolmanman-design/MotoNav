#pragma once
#include <cstdint>
#include <cstring>
#include "esp_system.h"

typedef enum {
  SPI1_HOST = 0,
  SPI2_HOST = 1,
  SPI3_HOST = 2
} spi_host_device_t;

typedef enum {
  SPI_CLK_SRC_DEFAULT = 0
} spi_clock_source_t;

#ifndef SPI_DEVICE_HALFDUPLEX
#define SPI_DEVICE_HALFDUPLEX (1 << 0)
#endif
#ifndef SPI_TRANS_MODE_QIO
#define SPI_TRANS_MODE_QIO (1 << 0)
#endif
#ifndef SPI_TRANS_VARIABLE_CMD
#define SPI_TRANS_VARIABLE_CMD (1 << 1)
#endif
#ifndef SPI_TRANS_VARIABLE_ADDR
#define SPI_TRANS_VARIABLE_ADDR (1 << 2)
#endif
#ifndef SPI_TRANS_VARIABLE_DUMMY
#define SPI_TRANS_VARIABLE_DUMMY (1 << 3)
#endif

struct spi_device_t;
typedef spi_device_t *spi_device_handle_t;

typedef struct {
  uint8_t command_bits;
  uint8_t address_bits;
  uint8_t dummy_bits;
  uint8_t mode;
  spi_clock_source_t clock_source;
  int clock_speed_hz;
  int spics_io_num;
  uint32_t flags;
  int queue_size;
  void *pre_cb;
  void *post_cb;
} spi_device_interface_config_t;

typedef struct {
  uint32_t flags;
  uint16_t cmd;
  uint64_t addr;
  size_t length;
  size_t rxlength;
  void *user;
  union {
    const void *tx_buffer;
    uint8_t tx_data[4];
  };
  union {
    void *rx_buffer;
    uint8_t rx_data[4];
  };
} spi_transaction_t;

typedef struct {
  spi_transaction_t base;
  uint8_t command_bits;
  uint8_t address_bits;
  uint8_t dummy_bits;
} spi_transaction_ext_t;

inline esp_err_t spi_bus_add_device(spi_host_device_t, const spi_device_interface_config_t *,
                                   spi_device_handle_t *handle) {
  if (handle) *handle = (spi_device_handle_t)1;
  return ESP_OK;
}
inline esp_err_t spi_bus_remove_device(spi_device_handle_t) { return ESP_OK; }
inline esp_err_t spi_device_queue_trans(spi_device_handle_t, spi_transaction_t *, uint32_t) {
  return ESP_OK;
}
inline esp_err_t spi_device_get_trans_result(spi_device_handle_t, spi_transaction_t **, uint32_t) {
  return ESP_OK;
}
inline esp_err_t spi_device_transmit(spi_device_handle_t, spi_transaction_t *) { return ESP_OK; }
inline esp_err_t spi_device_acquire_bus(spi_device_handle_t, uint32_t) { return ESP_OK; }
inline void spi_device_release_bus(spi_device_handle_t) {}
