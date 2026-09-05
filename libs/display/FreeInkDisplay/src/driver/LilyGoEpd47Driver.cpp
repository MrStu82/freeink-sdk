#include "LilyGoEpd47Driver.h"

#include <BoardConfig.h>

#include <assert.h>
#include <cstring>

#if FREEINK_DRIVER_LILYGO_EPD47
#include <epd_driver.h>
#include <esp_heap_caps.h>
#endif

#include "LilyGoEpd47Pixel.h"

namespace freeink {
namespace {
#if FREEINK_DRIVER_LILYGO_EPD47
uint8_t* g_gray = nullptr;
uint8_t* g_lsb = nullptr;
uint8_t* g_msb = nullptr;
uint16_t g_widthBytes = 0;
uint16_t g_height = 0;

void allocateBuffers(uint16_t widthBytes, uint16_t height) {
  g_widthBytes = widthBytes;
  g_height = height;
  const size_t planeBytes = static_cast<size_t>(widthBytes) * height;
  const size_t grayBytes = planeBytes * 4;
  if (!g_gray) g_gray = static_cast<uint8_t*>(heap_caps_malloc(grayBytes, MALLOC_CAP_SPIRAM));
  if (!g_lsb) g_lsb = static_cast<uint8_t*>(heap_caps_calloc(planeBytes, 1, MALLOC_CAP_SPIRAM));
  if (!g_msb) g_msb = static_cast<uint8_t*>(heap_caps_calloc(planeBytes, 1, MALLOC_CAP_SPIRAM));
  assert(g_gray && g_lsb && g_msb);
}

void fill4bpp(const uint8_t* base, const uint8_t* lsb, const uint8_t* msb,
              const lilygo_epd47::RefreshRect& rect) {
  const uint16_t rectWidthBytes = rect.width / 8;
  for (uint16_t row = 0; row < rect.height; ++row) {
    const size_t planeOffset = static_cast<size_t>(rect.y + row) * g_widthBytes + rect.x / 8;
    const size_t grayOffset = static_cast<size_t>(row) * rectWidthBytes * 4;
    lilygo_epd47::packRow4bpp(base + planeOffset,
                              lsb ? lsb + planeOffset : nullptr,
                              msb ? msb + planeOffset : nullptr,
                              g_gray + grayOffset, rectWidthBytes);
  }
}

void fillGray8(const uint8_t* gray8, uint16_t stride) {
  for (uint16_t y = 0; y < g_height; ++y) {
    lilygo_epd47::packGray8Row(gray8 + static_cast<size_t>(y) * stride,
                               g_gray + static_cast<size_t>(y) * g_widthBytes * 4,
                               static_cast<size_t>(g_widthBytes) * 8);
  }
}

void fillGray8Window(const uint8_t* gray8, uint16_t stride, uint16_t width,
                     uint16_t height) {
  for (uint16_t row = 0; row < height; ++row) {
    lilygo_epd47::packGray8Row(gray8 + static_cast<size_t>(row) * stride,
                               g_gray + static_cast<size_t>(row) * (width / 2U),
                               width);
  }
}

void pushArea(const lilygo_epd47::RefreshRect& rect, bool turnOff) {
  if (!rect.refresh) {
    if (turnOff) epd_poweroff_all();
    return;
  }
  const Rect_t area{rect.x, rect.y, rect.width, rect.height};
  epd_poweron();
  // The official BLACK_ON_WHITE draw assumes a white target. Clear only the
  // byte-aligned changed area for Fast; clean modes select the full panel.
  epd_clear_area(area);
  epd_draw_grayscale_image(area, g_gray);
  if (turnOff) epd_poweroff_all();
  else epd_poweroff();
}
#endif
}  // namespace

PanelGeometry LilyGoEpd47Driver::geometry() const {
  const uint16_t width = BoardConfig::ACTIVE.displayWidth;
  const uint16_t height = BoardConfig::ACTIVE.displayHeight;
  const uint16_t widthBytes = width / 8;
  return {width, height, widthBytes,
          static_cast<uint32_t>(widthBytes) * height};
}

void LilyGoEpd47Driver::begin(EpdBus& bus) {
  (void)bus;
#if FREEINK_DRIVER_LILYGO_EPD47
  epd_init();
  const PanelGeometry g = geometry();
  allocateBuffers(g.widthBytes, g.height);
#endif
}

void LilyGoEpd47Driver::display(EpdBus& bus, const uint8_t* fb,
                                const uint8_t* prev, RefreshMode mode,
                                bool turnOff) {
  (void)bus;
#if FREEINK_DRIVER_LILYGO_EPD47
  const auto plan = lilygo_epd47::planRefresh1bpp(
      fb, prev, g_widthBytes, g_height, mode == RefreshMode::Fast);
  if (plan.refresh) fill4bpp(fb, nullptr, nullptr, plan);
  pushArea(plan, turnOff);
#else
  (void)fb;
  (void)prev;
  (void)mode;
  (void)turnOff;
#endif
}

void LilyGoEpd47Driver::displayWindow(EpdBus& bus, const uint8_t* fb,
                                      const uint8_t* prev, uint16_t x,
                                      uint16_t y, uint16_t w, uint16_t h,
                                      bool turnOff) {
  (void)bus;
  (void)prev;
#if FREEINK_DRIVER_LILYGO_EPD47
  if (!fb || x >= g_widthBytes * 8U || y >= g_height || w == 0 || h == 0) return;
  const uint16_t x0 = static_cast<uint16_t>(x & ~7U);
  uint16_t x1 = static_cast<uint16_t>((x + w + 7U) & ~7U);
  if (x1 > g_widthBytes * 8U) x1 = g_widthBytes * 8U;
  if (h > g_height - y) h = g_height - y;
  const lilygo_epd47::RefreshRect plan{x0, y,
      static_cast<uint16_t>(x1 - x0), h, true, true};
  fill4bpp(fb, nullptr, nullptr, plan);
  pushArea(plan, turnOff);
#else
  (void)fb; (void)x; (void)y; (void)w; (void)h; (void)turnOff;
#endif
}

void LilyGoEpd47Driver::displayGray8(EpdBus& bus, const uint8_t* gray8,
                                     uint16_t stride, RefreshMode mode,
                                     bool turnOff) {
  (void)bus;
  (void)mode;
#if FREEINK_DRIVER_LILYGO_EPD47
  if (!gray8 || stride < g_widthBytes * 8U) return;
  fillGray8(gray8, stride);
  const lilygo_epd47::RefreshRect full{0, 0,
      static_cast<uint16_t>(g_widthBytes * 8U), g_height, true, false};
  pushArea(full, turnOff);
#else
  (void)gray8; (void)stride; (void)turnOff;
#endif
}

bool LilyGoEpd47Driver::displayGray8Window(EpdBus& bus, const uint8_t* gray8,
                                           uint16_t stride, uint16_t x,
                                           uint16_t y, uint16_t w, uint16_t h,
                                           bool turnOff) {
  (void)bus;
#if FREEINK_DRIVER_LILYGO_EPD47
  if (!gray8 || w == 0 || h == 0 || (x & 1U) || (w & 1U) || stride < w ||
      x >= g_widthBytes * 8U || y >= g_height || w > g_widthBytes * 8U - x ||
      h > g_height - y) {
    return false;
  }
  fillGray8Window(gray8, stride, w, h);
  const lilygo_epd47::RefreshRect area{x, y, w, h, true, true};
  pushArea(area, turnOff);
  return true;
#else
  (void)gray8; (void)stride; (void)x; (void)y; (void)w; (void)h; (void)turnOff;
  return false;
#endif
}

void LilyGoEpd47Driver::copyGrayscaleLsb(EpdBus& bus, const uint8_t* lsb) {
  (void)bus;
#if FREEINK_DRIVER_LILYGO_EPD47
  if (lsb) std::memcpy(g_lsb, lsb, static_cast<size_t>(g_widthBytes) * g_height);
#else
  (void)lsb;
#endif
}

void LilyGoEpd47Driver::copyGrayscaleMsb(EpdBus& bus, const uint8_t* msb) {
  (void)bus;
#if FREEINK_DRIVER_LILYGO_EPD47
  if (msb) std::memcpy(g_msb, msb, static_cast<size_t>(g_widthBytes) * g_height);
#else
  (void)msb;
#endif
}

void LilyGoEpd47Driver::writeGrayscalePlaneStrip(EpdBus& bus, GrayPlane plane,
                                                  const uint8_t* rows,
                                                  uint16_t yStart,
                                                  uint16_t numRows) {
  (void)bus;
#if FREEINK_DRIVER_LILYGO_EPD47
  if (!rows || yStart >= g_height) return;
  if (numRows > g_height - yStart) numRows = g_height - yStart;
  uint8_t* target = plane == GrayPlane::Lsb ? g_lsb : g_msb;
  std::memcpy(target + static_cast<size_t>(yStart) * g_widthBytes, rows,
              static_cast<size_t>(numRows) * g_widthBytes);
#else
  (void)plane;
  (void)rows;
  (void)yStart;
  (void)numRows;
#endif
}

void LilyGoEpd47Driver::displayGray(EpdBus& bus, const uint8_t* fb,
                                    bool turnOff, const unsigned char* lut,
                                    bool factoryMode) {
  (void)bus;
  (void)lut;
  (void)factoryMode;
#if FREEINK_DRIVER_LILYGO_EPD47
  const lilygo_epd47::RefreshRect full{0, 0,
      static_cast<uint16_t>(g_widthBytes * 8U), g_height, true, false};
  fill4bpp(fb, g_lsb, g_msb, full);
  pushArea(full, turnOff);
#else
  (void)fb;
  (void)turnOff;
#endif
}

void LilyGoEpd47Driver::cleanupGrayscaleBuffers(EpdBus& bus,
                                                 const uint8_t* bw) {
  (void)bus;
#if FREEINK_DRIVER_LILYGO_EPD47
  if (bw) {
    const lilygo_epd47::RefreshRect full{0, 0,
        static_cast<uint16_t>(g_widthBytes * 8U), g_height, true, false};
    fill4bpp(bw, nullptr, nullptr, full);
  }
#else
  (void)bw;
#endif
}

void LilyGoEpd47Driver::deepSleep(EpdBus& bus) {
  (void)bus;
#if FREEINK_DRIVER_LILYGO_EPD47
  epd_poweroff_all();
#endif
}

PanelDriver& lilyGoEpd47Driver() {
  static LilyGoEpd47Driver instance;
  return instance;
}

}  // namespace freeink
