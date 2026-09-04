#pragma once

#include <stddef.h>
#include <stdint.h>

namespace freeink::lilygo_epd47 {

// LilyGo's official ED047TC1 framebuffer packs the even pixel in the low
// nibble and the odd pixel high. Quantize Gray8 to all 16 native levels.
inline uint8_t grayNibble(uint8_t gray8) {
  return static_cast<uint8_t>((static_cast<uint16_t>(gray8) + 8U) / 17U);
}

inline void packGray8Row(const uint8_t* gray8, uint8_t* dst, size_t width) {
  for (size_t x = 0; x < width; x += 2) {
    const uint8_t lo = grayNibble(gray8[x]);
    const uint8_t hi = (x + 1 < width) ? grayNibble(gray8[x + 1]) : 0x0F;
    dst[x / 2] = static_cast<uint8_t>(lo | (hi << 4));
  }
}

// Legacy FreeInk AA is encoded as a B/W base plus two 1-bpp planes. Keep that
// 1-bpp UI path compatible while native Gray8 callers retain all 16 levels.
inline uint8_t aaNibble(uint8_t base, uint8_t lsb, uint8_t msb, uint8_t mask) {
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
      const uint8_t first = aaNibble(b, l, m, firstMask);
      const uint8_t second = aaNibble(b, l, m, secondMask);
      dst[bx * 4 + pair] = static_cast<uint8_t>(first | (second << 4));
    }
  }
}

struct RefreshRect {
  uint16_t x;
  uint16_t y;
  uint16_t width;
  uint16_t height;
  bool refresh;
  bool partial;
};

// Fast updates use the smallest byte-aligned changed rectangle. Clean/half
// updates, or a missing previous frame, deliberately select the full panel.
inline RefreshRect planRefresh1bpp(const uint8_t* frame, const uint8_t* previous,
                                   uint16_t widthBytes, uint16_t height,
                                   bool fast) {
  const RefreshRect full{0, 0, static_cast<uint16_t>(widthBytes * 8U), height,
                         true, false};
  if (!fast || !previous) return full;

  uint16_t minByte = widthBytes;
  uint16_t maxByte = 0;
  uint16_t minY = height;
  uint16_t maxY = 0;
  for (uint16_t y = 0; y < height; ++y) {
    for (uint16_t bx = 0; bx < widthBytes; ++bx) {
      const size_t i = static_cast<size_t>(y) * widthBytes + bx;
      if (frame[i] == previous[i]) continue;
      if (bx < minByte) minByte = bx;
      if (bx > maxByte) maxByte = bx;
      if (y < minY) minY = y;
      if (y > maxY) maxY = y;
    }
  }
  if (minY == height) return {0, 0, 0, 0, false, true};
  return {static_cast<uint16_t>(minByte * 8U), minY,
          static_cast<uint16_t>((maxByte - minByte + 1U) * 8U),
          static_cast<uint16_t>(maxY - minY + 1U), true, true};
}

}  // namespace freeink::lilygo_epd47
