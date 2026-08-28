#include "FrontlightManager.h"

#if FREEINK_CAP_FRONTLIGHT
namespace {
constexpr uint32_t maxDuty(uint8_t bits) { return (1u << bits) - 1u; }

// Perception-weighted percent -> duty, gamma 1.6554: 16-bit fixed-point table
// of round(65535 * (pct/100)^1.6554). This is deliberately used only for the
// X4 Pro: 1% maps to the dimmest 10-bit PWM step and every percentage remains
// distinct, while established mappings on every other board stay byte-for-byte
// equivalent to the legacy linear path below.
static constexpr uint16_t X4_PRO_GAMMA_TABLE[101] = {
    0,     32,    101,   197,   318,   460,   622,   803,   1001,  1217,  1449,  1697,  1960,  2237,  2529,
    2835,  3155,  3488,  3834,  4193,  4565,  4949,  5345,  5753,  6173,  6604,  7047,  7502,  7967,  8444,
    8931,  9429,  9938,  10457, 10987, 11527, 12077, 12638, 13208, 13789, 14379, 14979, 15588, 16208, 16836,
    17474, 18122, 18779, 19445, 20120, 20804, 21497, 22200, 22911, 23631, 24360, 25097, 25843, 26598, 27362,
    28134, 28914, 29703, 30500, 31306, 32120, 32942, 33772, 34611, 35457, 36312, 37175, 38045, 38924, 39811,
    40705, 41608, 42518, 43436, 44361, 45295, 46236, 47185, 48141, 49105, 50076, 51055, 52042, 53036, 54037,
    55046, 56062, 57086, 58117, 59155, 60200, 61253, 62313, 63380, 64454, 65535};

constexpr uint32_t x4ProPerceptualDuty(uint32_t pct, uint32_t full) {
  if (pct == 0) return 0;
  if (pct > 100) pct = 100;
  const uint32_t duty = (full * X4_PRO_GAMMA_TABLE[pct] + 32767u) / 65535u;
  return duty ? duty : 1u;
}

// Fixed LEDC channels for the Arduino-ESP32 2.x path (3.x keys by GPIO and allocates
// channels itself). Frontlight owns 0 (cool/primary) and 1 (warm); no other SDK LEDC
// user on a frontlight board takes these (the Buzzer uses the 3.x gpio-keyed API).
constexpr uint8_t LEDC_CH_COOL = 0;
constexpr uint8_t LEDC_CH_WARM = 1;

// Turn a 0-100 percentage into a duty, honoring active level. `pct` is pre-clamped.
uint32_t dutyFor(uint32_t pct, uint32_t full, bool activeHigh) {
  uint32_t duty = (pct * full) / 100u;
  return activeHigh ? duty : full - duty;
}

#if defined(ARDUINO) && ESP_ARDUINO_VERSION_MAJOR >= 3
bool attachChannel(int8_t gpio, uint8_t /*ch*/, uint32_t freq, uint8_t bits) {
  return ledcAttach(gpio, freq, bits);
}
void writeChannel(int8_t gpio, uint8_t /*ch*/, uint32_t duty) { ledcWrite(gpio, duty); }
#else
bool attachChannel(int8_t gpio, uint8_t ch, uint32_t freq, uint8_t bits) {
  ledcSetup(ch, freq, bits);
  return ledcAttachPin(gpio, ch);
}
void writeChannel(int8_t /*gpio*/, uint8_t ch, uint32_t duty) { ledcWrite(ch, duty); }
#endif
}  // namespace
#endif

bool FrontlightManager::begin() {
#if FREEINK_CAP_FRONTLIGHT
  const auto& fl = BoardConfig::ACTIVE.frontlight;
  if (fl.gpio == BoardConfig::PIN_UNASSIGNED) return true;  // no frontlight on this board

  _coolAttachOk = attachChannel(fl.gpio, LEDC_CH_COOL, fl.pwmFrequency, fl.pwmResolutionBits);
  _warmAttachOk = fl.gpioWarm == BoardConfig::PIN_UNASSIGNED
                      ? true  // no second channel on this board — not a failure
                      : attachChannel(fl.gpioWarm, LEDC_CH_WARM, fl.pwmFrequency, fl.pwmResolutionBits);
  _begun = true;
  setBrightness(0);
  return _coolAttachOk && _warmAttachOk;
#else
  return true;
#endif
}

#if FREEINK_CAP_FRONTLIGHT
void FrontlightManager::apply() {
  const auto& fl = BoardConfig::ACTIVE.frontlight;
  if (!_begun || fl.gpio == BoardConfig::PIN_UNASSIGNED) return;

  const uint32_t full = maxDuty(fl.pwmResolutionBits);
  const bool dual = fl.gpioWarm != BoardConfig::PIN_UNASSIGNED;

  if (BoardConfig::isX4Pro()) {
    // Convert brightness to PWM precision before splitting it between the two
    // strings. Splitting a 1% request in percentage space makes both halves
    // zero; splitting the duty preserves the X4 Pro's real minimum step. The
    // rounded warm share plus its cool remainder always equals totalDuty.
    const uint32_t totalDuty = x4ProPerceptualDuty(_brightness, full);
    const uint32_t warmDuty = dual ? (totalDuty * _warmPercent + 50u) / 100u : 0u;
    const uint32_t coolDuty = totalDuty - warmDuty;
    writeChannel(fl.gpio, LEDC_CH_COOL, fl.activeHigh ? coolDuty : full - coolDuty);
    if (dual) {
      writeChannel(fl.gpioWarm, LEDC_CH_WARM, fl.activeHigh ? warmDuty : full - warmDuty);
    }
    return;
  }

  // Single channel: the primary carries the whole brightness. Warm/cool pair: split the
  // total brightness between cool and warm by the color-temperature mix, so overall
  // brightness stays ~constant as the color shifts (warm 0 = all cool, 100 = all warm).
  const uint32_t coolPct = dual ? (static_cast<uint32_t>(_brightness) * (100u - _warmPercent)) / 100u
                                : _brightness;
  writeChannel(fl.gpio, LEDC_CH_COOL, dutyFor(coolPct, full, fl.activeHigh));

  if (dual) {
    const uint32_t warmPct = (static_cast<uint32_t>(_brightness) * _warmPercent) / 100u;
    writeChannel(fl.gpioWarm, LEDC_CH_WARM, dutyFor(warmPct, full, fl.activeHigh));
  }
}
#endif

void FrontlightManager::setBrightness(uint8_t percent) {
#if FREEINK_CAP_FRONTLIGHT
  if (percent > 100) percent = 100;
  _brightness = percent;
  if (percent > 0) _lastBrightness = percent;
  apply();
#else
  (void)percent;
#endif
}

void FrontlightManager::off() { setBrightness(0); }
void FrontlightManager::on() { setBrightness(_lastBrightness); }

void FrontlightManager::setColorTemperature(uint8_t warmPercent) {
#if FREEINK_CAP_FRONTLIGHT
  _warmPercent = warmPercent > 100 ? 100 : warmPercent;
  // Only re-drives hardware when a warm channel exists; on single-channel boards this just
  // records the request (apply() ignores _warmPercent without a second channel).
  apply();
#else
  (void)warmPercent;
#endif
}
