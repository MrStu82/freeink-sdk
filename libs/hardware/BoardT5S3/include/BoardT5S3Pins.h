#pragma once

#include <Arduino.h>

// LILYGO Screen-4.7-S3 V2.4, 2024-12-03.
// Source: Xinyuan-LilyGO/LilyGo-EPD47 esp32s3 @ 391b0e25.
#define T5S3_WIDTH 960
#define T5S3_HEIGHT 540
#define T5S3_LOGICAL_WIDTH 540
#define T5S3_LOGICAL_HEIGHT 960

#define T5S3_BUTTON 21
#define T5S3_BOOT_CFG_STR 0
#define T5S3_BATTERY_ADC 14

#define T5S3_SCL 17
#define T5S3_SDA 18
#define T5S3_I2C_FREQ 400000
#define T5S3_GT911_ADDR 0x5D
#define T5S3_GT911_ADDR_ALT 0x14
#define T5S3_PCF8563_ADDR 0x51
#define T5S3_TOUCH_INT 47

#define T5S3_SPI_MISO 16
#define T5S3_SPI_MOSI 15
#define T5S3_SPI_SCLK 11
#define T5S3_SD_CS 42
