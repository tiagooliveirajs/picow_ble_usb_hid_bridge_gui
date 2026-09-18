# FORK-04 — Fixed production USB Mouse + Keyboard identity

FORK-04 freezes and validates the production USB contract inherited from the physically accepted BLU2USB G06 baseline.

## Authority

Behavioral baseline: `tiagooliveirajs/blu2usb` SHA `7eee024ad4ee726c5a85ffa2f32b9f47187878af`.

Relevant G06 artifacts:

- `docs/technical/02-g04-fixed-usb-validation.md` blob `757bb60ab3d12b78c7bc5897ca919075fa243825`;
- `src/usb_hid/usb_descriptors.c` blob `694966674de34cce1370c51360a7f8022c5803c6`;
- `include/blu2usb/usb_hid/usb_hid.h` blob `01f5d4588ecd3ce8ec4fc67772f7d5503193f252`.

## Frozen production identity

- VID `0xCAFE`;
- PID `0x4010`;
- device revision `0x0100`;
- manufacturer `BLU2USB`;
- product `BLU2USB Mouse + Keyboard`;
- interface 0: HID Mouse;
- interface 1: HID Keyboard;
- exactly two HID interfaces;
- no CDC, MSC, MIDI or vendor USB interface;
- USB stdio disabled;
- UART stdio disabled;
- no runtime `tud_disconnect()` / `tud_connect()` re-enumeration workaround.

USB initialization occurs on Core0 before the Bluetooth Core1 runtime is launched. Therefore Mouse and Keyboard interfaces exist from boot independently of Bluetooth topology. Classic Keyboard connection, disconnection, pairing and typing must never mutate the USB identity.

The fixed Keyboard interface remains present even when no physical Keyboard is connected so later Synthetic Escape can share the same canonical Keyboard target without changing descriptors.

## Implementation strategy

FORK-01 already introduced the correct descriptor topology while establishing the production target. FORK-04 does not redesign that working runtime. Instead it makes the contract mechanically non-accidental:

- an immutable `usb_hid_identity_t` manifest is compiled into the product;
- compile-time assertions reject VID/PID/revision/interface drift;
- host tests verify exact strings and interface numbers;
- source-contract tests verify class counts, Mouse/Keyboard descriptors, endpoint ordering, boot independence and absence of dynamic re-enumeration calls;
- inherited FORK-03 canonical/Classic regression tests remain mandatory;
- Pico 2 W cross-build produces the exact physical candidate.

## Physical acceptance boundary

FORK-04 requires operator validation of the exact committed UF2. Acceptance must prove that the host sees the same fixed composite Mouse+Keyboard identity with no Bluetooth peer, during Classic connection/disconnection, and after a normal Pico unplug/replug. Software/build success alone does not accept this gate.
