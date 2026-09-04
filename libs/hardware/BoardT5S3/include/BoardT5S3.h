#pragma once

#include <Arduino.h>
#include "BoardT5S3Pins.h"

namespace BoardT5S3 {
void begin();
void beginI2C();
void prepareSdBus();
// Single bounded I2C transaction; Wire's 50 ms timeout is set by beginI2C().
bool probeI2CAddress(uint8_t address);
bool probeTouch();
}  // namespace BoardT5S3
