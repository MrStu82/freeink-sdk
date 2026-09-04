#include <BoardT5S3.h>

#include <SPI.h>
#include <Wire.h>

namespace BoardT5S3 {

void beginI2C() {
  Wire.begin(T5S3_SDA, T5S3_SCL);
  Wire.setClock(T5S3_I2C_FREQ);
  Wire.setTimeOut(50);
}

void prepareSdBus() {
  pinMode(T5S3_SD_CS, OUTPUT);
  digitalWrite(T5S3_SD_CS, HIGH);
  SPI.begin(T5S3_SPI_SCLK, T5S3_SPI_MISO, T5S3_SPI_MOSI, T5S3_SD_CS);
}

void begin() {
  beginI2C();
  pinMode(T5S3_BUTTON, INPUT_PULLUP);
  prepareSdBus();
}

bool probeI2CAddress(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

bool probeTouch() {
  return probeI2CAddress(T5S3_GT911_ADDR) ||
         probeI2CAddress(T5S3_GT911_ADDR_ALT);
}

}  // namespace BoardT5S3
