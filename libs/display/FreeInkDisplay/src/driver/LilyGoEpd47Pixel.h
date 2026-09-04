#pragma once

#include <stddef.h>
#include <stdint.h>

namespace freeink::lilygo_epd47 {

// FreeInk planes are 1-bpp MSB-first (1=white). LilyGo's official ED047TC1
// framebuffer packs the even pixel in the low nibble and the odd pixel high.
inline uint8_t grayNibble(uint8_t base, uint8_t lsb, uint8_t msb, uint8_t mask) {
  if (base & mask) return 0x0F;
  if (msb & mask) return (lsb & mask) ? 0x05 : 0x0A;
  return (lsb & mask) ? 0x05 : 0x00;
}

inline void packRow4bpp(const uint8_t* base, const uint8_t* lsb,
                        const uint8_t* msb, uint8_t* dst, size_t widthBytes) {
  for (size_t bx = 0; bx < widthBytes; ++bx) {
    const uint8_t b = base[bx];
    const uint8_t l = lsb ? lsb[bx] : 0;
    const uint8_t m = msb ? msb[bx] : 0;
    for (uint8_t pair = 0; pair < 4; ++pair) {
      const uint8_t firstMask = static_cast<uint8_t>(0x80U >> (pair * 2));
      const uint8_t secondMask = static_cast<uint8_t>(firstMask >> 1);
      const uint8_t first = grayNibble(b, l, m, firstMask);
      const uint8_t second = grayNibble(b, l, m, secondMask);
      dst[bx * 4 + pair] = static_cast<uint8_t>(first | (second << 4));
    }
  }
}

}  // namespace freeink::lilygo_epd47
