#pragma once

// FreeInk SDK — boot-time recovery hatch.
//
// The stock Xteink (and most ESP32) second-stage bootloader can't read buttons —
// it just boots whatever otadata selects. So "hold a combo at reset to fall back
// to the recovery firmware" can only be honoured by the firmware that actually
// boots. This is that check, made shareable: call recovery::checkBootCombo() as
// the VERY FIRST thing in setup() of every firmware you want to be escapable
// (the recovery flasher itself, the editor, the reader, ...).
//
// Convention: the recovery / "escape hatch" firmware lives in OTA slot 0 (ota_0,
// the default upload offset 0x10000). Held at reset, the combo Back + Up repoints
// otadata at slot 0 and reboots into it.
//
// It is always safe to call unconditionally and early — it does nothing unless
// ALL of these hold: the combo is pressed, slot 0 contains a valid app image, and
// the caller isn't already running from slot 0 (so inside the recovery firmware
// it's a no-op). When it does act it reboots and never returns.
//
// What it can't do: escape a firmware that crashes in ROM / early SDK init before
// this call is reached. (A corrupt app *image* is still caught for free — the
// bootloader falls back to the other OTA slot on its own.) A truly unconditional
// GPIO recovery would require a custom second-stage bootloader, which the recovery
// firmware deliberately never reflashes.
//
// NOT BUILDABLE AS-IS ON XTEINK X4 PRO (2026-08-09): the Back + Up combo above
// depends on an ADC resistor ladder (Back=GPIO1, Up=GPIO2) that the X4 Pro board
// doc (freeink-sdk/docs/xteink-x4pro-support.md) explicitly documents as vestigial
// firmware, not wired on this variant. The X4 Pro has no Back button and no Up
// button in any form — only Left(GPIO0)/Right(GPIO7)/Power(GPIO3) digital buttons
// plus a capacitive Home key via GT911. checkBootCombo() is also not called
// anywhere in the X4 Pro app today (dead code in that image). Any recovery combo
// for this board has to be built from its real buttons — don't assume this file
// already covers it. The ESP32 ROM serial bootloader (GPIO0 low at reset) remains
// the working escape hatch for a bad flash in the meantime; this gap is not urgent.

namespace freeink {
namespace recovery {

// Read the recovery combo and, if held, switch to OTA slot 0 and reboot. Returns
// immediately (no reboot) in every other case.
void checkBootCombo();

}  // namespace recovery
}  // namespace freeink
