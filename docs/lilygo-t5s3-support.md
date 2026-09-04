# LILYGO Screen-4.7-S3 V2.4 (ED047TC1)

This target is the **T5-4.7-S3 Capacitive Touch / Screen-4.7-S3 V2.4**, PCB date
2024-12-03, with ESP32-S3-WROOM-1-N16R8. It is not the later
T5S3-4.7-e-paper-PRO/Lite (H752) design.

Authoritative sources are the `esp32s3` branch of
[Xinyuan-LilyGO/LilyGo-EPD47](https://github.com/Xinyuan-LilyGO/LilyGo-EPD47),
pinned at `391b0e25d7a39897e3a00af34053250df031d699`, particularly
`README.md`, `src/utilities.h`, `src/ed047tc1.h`, and the repository's
`Screen-4.7-S3-V2.4 24-12-03` schematic. The official repository is GPL-3.0; binary distributions that link this driver must satisfy that licence and provide corresponding source.

## Exact contract

- ESP32-S3, 16 MiB QIO flash, 8 MiB OPI PSRAM.
- ED047TC1 960×540 raw-parallel display. Config register: DATA13, CLK12,
  STR0; scan: CKV38, STH40, CKH41; D0..D7 = 8,1,2,3,4,5,6,7.
- GT911: SDA18, SCL17, IRQ47, addresses 0x5D/0x14. The sensor is axis-swapped
  and Y-mirrored relative to the panel. GPIO47 cannot wake ESP32-S3 without the
  optional physical IRQ-to-GPIO10 jumper described upstream.
- PCF8563 RTC at 0x51 on the same I2C bus.
- SPI-SD: SCLK11, MISO16, MOSI15, CS42.
- Battery ADC14 through the upstream 2:1 divider.
- Side controls are GPIO21, BOOT/GPIO0, and hard RESET. GPIO21 is the usable
  active-low runtime/deep-sleep wake button. GPIO0 is also the display's CFG_STR
  and is therefore not configured as an application input after boot. RESET is
  not software-readable.
- No PCA9535, TPS65185, BQ27220/BQ25896, LoRa/GPS, or frontlight exists in this
  profile. In particular, GPIO11 is SD clock and must never be driven as PWM.

## Display integration

`LilyGoEpd47Driver` wraps the official library's `epd_init`, power and grayscale
frame APIs and reports `usesExternalBus() == true`. This is required because the
V2.4 board multiplexes panel controls through a 74HCT4094; LovyanGFX `Bus_EPD`
expects direct OE/LE/SPV/power GPIOs and is not electrically equivalent.

The FreeInk 1-bpp framebuffer is expanded into the official packed 4-bpp format
in PSRAM (even pixel in the low nibble). The existing LSB/MSB anti-alias planes
map to 0/5/10/15 grayscale nibbles. Deep sleep calls `epd_poweroff_all()`.

Build with `-DFREEINK_DEVICE_LILYGO=1` and pin the official dependency to the
commit above. `BoardT5S3::begin()` initializes only the exact shared I2C and
SPI-SD buses plus GPIO21 input; it deliberately does not touch GPIO0.
