#include "FrontlightManager.h"

#if FREEINK_DEVICE_PICCO
#include <Wire.h>
#endif

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
#if FREEINK_DEVICE_PICCO
  if (BoardConfig::isPicco()) {
    // Shared I2C bus (with touch); no dedicated enable GPIO. Probe the operating
    // address so an absent/unexpected part is not exposed as a capability.
    Wire.begin(BoardConfig::Picco::i2cSda, BoardConfig::Picco::i2cScl, BoardConfig::Picco::i2cHz);
    Wire.beginTransmission(BoardConfig::Picco::frontlightAddr);
    if (Wire.endTransmission() != 0) return false;
    _begun = _coolAttachOk = _warmAttachOk = true;
    _brightness = 0;
    _i2cConfigured = false;
    return true;
  }
#endif
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

#if FREEINK_CAP_FRONTLIGHT && FREEINK_DEVICE_PICCO
// Onyx Picco frontlight: a dual-channel warm/cool I2C LED driver at 0x38 on the
// shared I2C bus. Recovered from the retail firmware (Ghidra); standard 8-bit
// sub-address register model:
//   reg0  = master enable  (bit2/0x04 = cool on, bit1/0x02 = warm on; bit7 = standby, cleared on shutdown)
//   reg1  = mode           (low 3 bits)         reg2 = 0x38 config      reg0x50 = 0x03
//   reg3  = cool PWM        reg4 = warm PWM      reg5/6 = per-channel current limit (5-bit)
// Brightness/color-temperature come from a 10x10x4 LUT the firmware indexes as
// base + 40*colorTemp + 4*brightness, each entry {cool_pwm, warm_pwm, cool_cur,
// warm_cur}. The table below is byte-verbatim from the vendor image (base
// 0x3c78d4c4, confirmed via the l32r in FUN_42008d44). NOTE: the firmware's
// brightness axis is 10 discrete steps; entries [*][0]/[*][1] are anomalous
// (non-monotonic) while [*][2..9] form a clean ramp, so the SDK's 1..100% maps
// onto brightness indices 2..9 to keep the response monotonic. CONFIDENCE:
// register protocol high; exact UI index mapping medium — validate/tune on hardware.
namespace {
constexpr uint8_t PICCO_FL_LUT[400] = {
    136,1,100,0,11,2,150,0,1,0,24,0,14,0,31,0,34,0,31,0,55,0,31,0,75,0,31,0,95,0,31,0,116,0,31,0,136,0,31,0,  // colorTemp 0
    157,0,31,0,177,0,31,0,1,1,19,0,10,3,31,1,30,1,31,8,50,1,31,19,71,8,31,31,91,28,31,31,112,48,31,31,132,69,31,31,  // colorTemp 1
    152,89,31,31,173,110,31,31,1,3,17,2,5,1,31,10,25,1,31,22,46,12,31,31,66,32,31,31,86,53,31,31,107,73,31,31,127,94,31,31,  // colorTemp 2
    148,114,31,31,168,134,31,31,1,2,14,7,1,1,29,18,20,6,31,31,40,27,31,31,61,47,31,31,81,67,31,31,101,88,31,31,122,108,31,31,  // colorTemp 3
    142,129,31,31,162,149,31,31,2,1,10,12,1,1,23,26,13,16,31,31,34,37,31,31,54,57,31,31,74,78,31,31,95,98,31,31,115,118,31,31,  // colorTemp 4
    136,139,31,31,156,159,31,31,1,2,7,16,1,4,17,31,5,24,31,31,26,45,31,31,46,65,31,31,66,86,31,31,87,106,31,31,107,126,31,31,  // colorTemp 5
    128,147,31,31,148,167,31,31,3,1,3,21,2,11,11,31,1,31,25,31,15,51,31,31,36,72,31,31,56,92,31,31,76,113,31,31,97,133,31,31,  // colorTemp 6
    117,153,31,31,138,174,31,31,2,1,0,26,2,16,5,31,2,37,14,31,1,57,31,31,21,77,31,31,42,98,31,31,62,118,31,31,82,138,31,31,  // colorTemp 7
    103,159,31,31,123,179,31,31,1,1,0,26,1,21,0,31,1,41,4,31,1,62,12,31,1,82,26,31,17,102,31,31,37,123,31,31,58,143,31,31,  // colorTemp 8
    78,164,31,31,98,184,31,31,0,5,0,31,0,25,0,31,0,46,0,31,0,66,0,31,0,86,0,31,0,107,0,31,0,127,0,31,0,147,0,31,  // colorTemp 9
};
}  // namespace

bool FrontlightManager::piccoWrite(const uint8_t reg, const uint8_t value) {
  Wire.beginTransmission(BoardConfig::Picco::frontlightAddr);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool FrontlightManager::piccoUpdate(const uint8_t reg, const uint8_t mask, const uint8_t value) {
  Wire.beginTransmission(BoardConfig::Picco::frontlightAddr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(BoardConfig::Picco::frontlightAddr, static_cast<uint8_t>(1), static_cast<uint8_t>(true)) != 1) return false;
  const uint8_t current = Wire.read();
  return piccoWrite(reg, static_cast<uint8_t>((current & ~mask) | (value & mask)));
}

bool FrontlightManager::configurePicco() {
  // The LM3630A-style controller needs time to leave software sleep before
  // brightness/current writes (also used by the Linux lm3630a driver). Only
  // pay this settling delay on the first on after boot/off, not slider ticks.
  if (!piccoUpdate(0x00, 0x80, 0x00)) {
    log_e("Picco frontlight wake failed");
    return false;
  }
  delay(2);
  // Recovered power-on sequence (FUN_42008cc4): filter/mode/config registers.
  bool ok = piccoWrite(0x50, 0x03);
  ok = piccoUpdate(0x01, 0x07, 0x00) && ok;
  ok = piccoWrite(0x02, 0x38) && ok;
  _i2cConfigured = ok;
  if (!ok) log_e("Picco frontlight configuration failed");
  return ok;
}

void FrontlightManager::applyPicco() {
  if (_brightness == 0) {
    // Off: clear both channel-enable bits, then set reg0 bit 7 (standby), as
    // stock's sleep path FUN_42008f64 does. The on path clears it again.
    piccoUpdate(0x00, 0x14, 0x00);
    piccoUpdate(0x00, 0x0b, 0x00);
    piccoUpdate(0x00, 0x80, 0x80);
    _i2cConfigured = false;
    return;
  }
  if (!_i2cConfigured && !configurePicco()) return;

  // Map SDK brightness% (1..100) -> LUT brightness index 2..9 (the monotonic ramp)
  // and warmPercent (0..100) -> colorTemp index 0..9.
  const uint8_t br = static_cast<uint8_t>(2 + (static_cast<uint16_t>(_brightness - 1) * 7 + 50) / 100);
  const uint8_t ct = static_cast<uint8_t>((static_cast<uint16_t>(_warmPercent) * 9 + 50) / 100);
  const uint8_t* e = &PICCO_FL_LUT[ct * 40 + br * 4];
  const uint8_t coolPwm = e[0], warmPwm = e[1], coolCur = static_cast<uint8_t>(e[2] & 0x1f),
                warmCur = static_cast<uint8_t>(e[3] & 0x1f);

  piccoWrite(0x03, coolPwm);  // cool brightness
  piccoWrite(0x04, warmPwm);  // warm brightness
  piccoUpdate(0x05, 0x1f, coolCur);
  piccoUpdate(0x06, 0x1f, warmCur);
  piccoUpdate(0x00, 0x14, (coolPwm || coolCur) ? 0x04 : 0x00);  // cool channel enable
  piccoUpdate(0x00, 0x0b, (warmPwm || warmCur) ? 0x02 : 0x00);  // warm channel enable
}
#endif

#if FREEINK_CAP_FRONTLIGHT
void FrontlightManager::apply() {
  const auto& fl = BoardConfig::ACTIVE.frontlight;
#if FREEINK_DEVICE_PICCO
  if (BoardConfig::isPicco()) {
    if (_begun) applyPicco();
    return;
  }
#endif
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
