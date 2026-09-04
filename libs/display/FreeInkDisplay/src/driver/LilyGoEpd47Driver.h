#pragma once

#include "PanelDriver.h"

namespace freeink {

class LilyGoEpd47Driver : public PanelDriver {
 public:
  uint32_t spiHz() const override { return 0; }
  BusyPolarity busyPolarity() const override { return BusyPolarity::ActiveLow; }
  bool usesExternalBus() const override { return true; }
  PanelGeometry geometry() const override;

  void begin(EpdBus& bus) override;
  void deepSleep(EpdBus& bus) override;
  void display(EpdBus& bus, const uint8_t* fb, const uint8_t* prev,
               RefreshMode mode, bool turnOff) override;
  void displayWindow(EpdBus& bus, const uint8_t* fb, const uint8_t* prev,
                     uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                     bool turnOff) override;

  bool supportsStripGrayscale() const override { return true; }
  bool supportsNativeGray8() const override { return true; }
  void displayGray8(EpdBus& bus, const uint8_t* gray8, uint16_t stride,
                    RefreshMode mode, bool turnOff) override;
  void copyGrayscaleLsb(EpdBus& bus, const uint8_t* lsb) override;
  void copyGrayscaleMsb(EpdBus& bus, const uint8_t* msb) override;
  void writeGrayscalePlaneStrip(EpdBus& bus, GrayPlane plane,
                                const uint8_t* rows, uint16_t yStart,
                                uint16_t numRows) override;
  void displayGray(EpdBus& bus, const uint8_t* fb, bool turnOff,
                   const unsigned char* lut, bool factoryMode) override;
  void cleanupGrayscaleBuffers(EpdBus& bus, const uint8_t* bw) override;
};

PanelDriver& lilyGoEpd47Driver();

}  // namespace freeink
