#pragma once
#include "BoardSdgoodsInit.h"

/** ST77916 bring-up: reuse Waveshare 1.85B vendor init tables until sdgoods MADCTL/TE is verified on hardware. */
class BoardSdgoodsPanel: public Arduino_ST77916 {
public:
  BoardSdgoodsPanel(Arduino_DataBus* bus):Arduino_ST77916(bus,LCD_RESET,0,true,PANEL_WIDTH,PANEL_HEIGHT){}
protected:
  void tftInit() override {
    pinMode(LCD_RESET,OUTPUT);digitalWrite(LCD_RESET,LOW);delay(20);digitalWrite(LCD_RESET,HIGH);delay(120);
    uint8_t id[4]={};
    spi_device_interface_config_t config={};
    config.command_bits=8;config.address_bits=24;config.clock_speed_hz=5000000;
    config.spics_io_num=-1;config.flags=SPI_DEVICE_HALFDUPLEX;config.queue_size=1;
    spi_device_handle_t reader=nullptr;
    bool readOK=false;
    if(spi_bus_add_device(SPI2_HOST,&config,&reader)==ESP_OK) {
      spi_transaction_t transaction={};transaction.flags=SPI_TRANS_USE_RXDATA;
      transaction.cmd=0x0B;transaction.addr=0x000400;transaction.rxlength=32;
      spi_device_acquire_bus(reader,portMAX_DELAY);digitalWrite(LCD_CS,LOW);
      readOK=spi_device_polling_transmit(reader,&transaction)==ESP_OK;
      digitalWrite(LCD_CS,HIGH);spi_device_release_bus(reader);
      if(readOK)memcpy(id,transaction.rx_data,4);
      spi_bus_remove_device(reader);
    }
    int variant=LCD_INIT_VARIANT;
    if(variant!=1 && variant!=2)variant=(readOK && id[0]==0 && id[1]==2 && id[2]==0x7F && id[3]==0x7F)?2:1;
    Serial.printf("sdgoods panel ID %02X %02X %02X %02X, init=%d%s\n",id[0],id[1],id[2],id[3],variant,readOK?"":" (read unavailable)");
    if(variant==2)_bus->batchOperation(boardsdgoods_init_2,sizeof(boardsdgoods_init_2));
    else _bus->batchOperation(boardsdgoods_init_1,sizeof(boardsdgoods_init_1));
  }
};
