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

void fill4bpp(const uint8_t* base, const uint8_t* lsb, const uint8_t* msb) {
  for (uint16_t y = 0; y < g_height; ++y) {
    const size_t planeOffset = static_cast<size_t>(y) * g_widthBytes;
    const size_t grayOffset = planeOffset * 4;
    lilygo_epd47::packRow4bpp(base + planeOffset,
                              lsb ? lsb + planeOffset : nullptr,
                              msb ? msb + planeOffset : nullptr,
                              g_gray + grayOffset, g_widthBytes);
  }
}

void pushFrame(bool turnOff) {
  epd_poweron();
  epd_draw_grayscale_image(epd_full_screen(), g_gray);
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
  (void)prev;
  (void)mode;
#if FREEINK_DRIVER_LILYGO_EPD47
  fill4bpp(fb, nullptr, nullptr);
  pushFrame(turnOff);
#else
  (void)fb;
  (void)turnOff;
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
  fill4bpp(fb, g_lsb, g_msb);
  pushFrame(turnOff);
#else
  (void)fb;
  (void)turnOff;
#endif
}

void LilyGoEpd47Driver::cleanupGrayscaleBuffers(EpdBus& bus,
                                                 const uint8_t* bw) {
  (void)bus;
#if FREEINK_DRIVER_LILYGO_EPD47
  if (bw) fill4bpp(bw, nullptr, nullptr);
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
