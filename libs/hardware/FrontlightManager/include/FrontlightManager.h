#pragma once

// FreeInk SDK — frontlight manager.
//
// Drives a PWM frontlight described by BoardConfig::ACTIVE.frontlight. Inert on
// boards without one (e.g. Xteink X4/X3), so it is always safe to construct.
//
// Two topologies, selected purely by the board profile:
//   * Single channel (de-link primary LED, LilyGo backlight, Murphy): one PWM pin
//     (frontlight.gpio). setColorTemperature() is a no-op.
//   * Warm/cool pair (Xteink X4 Pro: cool=GPIO8, warm=GPIO9): two independent PWM
//     channels. Overall brightness is the total light; color temperature splits that
//     total between the cool (gpio) and warm (gpioWarm) strings, so brightness stays
//     roughly constant as the color shifts. setColorTemperature() drives the mix.

#include <Arduino.h>
#include <BoardConfig.h>

class FrontlightManager {
 public:
  // Bring up the PWM channel(s). No-op (returns true) if the board has no frontlight.
  // Returns false if the board declares a frontlight but a channel actually failed to
  // attach — callers should log this, since a channel that never attached still leaves
  // setBrightness()/setColorTemperature() writing PWM duty to a pin nothing is driving.
  bool begin();

  // Set brightness as a 0-100 percentage. 0 turns the light off. On a warm/cool board
  // this is the TOTAL brightness; the current color-temperature split is preserved.
  void setBrightness(uint8_t percent);

  // Convenience: fully off / restore last brightness.
  void off();
  void on();

  // Warm/cool mix, 0 = fully cool, 100 = fully warm, 50 = neutral. Only meaningful on a
  // two-channel board (hasColorTemperature()); a no-op on single-channel frontlights.
  void setColorTemperature(uint8_t warmPercent);

  bool present() const {
#if FREEINK_CAP_FRONTLIGHT
#if FREEINK_DEVICE_PICCO
    // Picco's dual-channel I2C LED driver is present only after begin() gets an ACK.
    if (BoardConfig::isPicco()) return _begun;
#endif
    return BoardConfig::ACTIVE.frontlight.gpio != BoardConfig::PIN_UNASSIGNED;
#else
    return false;  // frontlight code not compiled in (FREEINK_CAP_FRONTLIGHT=0)
#endif
  }

  // True when the board wires a second (warm) channel, so setColorTemperature() does
  // something. False on single-channel frontlights and on boards with none.
  bool hasColorTemperature() const {
#if FREEINK_CAP_FRONTLIGHT
#if FREEINK_DEVICE_PICCO
    if (BoardConfig::isPicco()) return _begun;  // warm + cool channels
#endif
    return BoardConfig::ACTIVE.frontlight.gpio != BoardConfig::PIN_UNASSIGNED &&
           BoardConfig::ACTIVE.frontlight.gpioWarm != BoardConfig::PIN_UNASSIGNED;
#else
    return false;
#endif
  }

  uint8_t brightness() const { return _brightness; }
  uint8_t colorTemperature() const { return _warmPercent; }

  // Per-channel attach results from the last begin(), for diagnostics (e.g. the
  // frontlight pin diagnostic activity). Both false before begin() is called.
  bool coolChannelAttachOk() const { return _coolAttachOk; }
  bool warmChannelAttachOk() const { return _warmAttachOk; }

 private:
#if FREEINK_CAP_FRONTLIGHT
  // Recompute and write both channels from _brightness + _warmPercent.
  void apply();
#if FREEINK_DEVICE_PICCO
  // Onyx Picco I2C LED driver at Picco::frontlightAddr (lifted from upstream SDK).
  bool piccoWrite(uint8_t reg, uint8_t value);
  bool piccoUpdate(uint8_t reg, uint8_t mask, uint8_t value);
  bool configurePicco();
  void applyPicco();
  bool _i2cConfigured = false;
#endif
#endif

  bool _begun = false;
  bool _coolAttachOk = false;
  bool _warmAttachOk = false;
  uint8_t _brightness = 0;
  uint8_t _lastBrightness = 50;
  uint8_t _warmPercent = 50;  // neutral by default
};
