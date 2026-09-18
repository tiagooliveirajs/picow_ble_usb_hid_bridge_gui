# RP2350 BKB-3G Bluetooth Classic HID -> USB HID + CDC POC

This branch contains a deliberately small diagnostic POC derived from the repository's
`main` branch. It does not modify `main`.

## Goal

Prove this path on Raspberry Pi Pico 2 W / RP2350:

```text
BKB-3G / "Bluetooth keyboard 3.0"
        |
        | Bluetooth Classic HID (BR/EDR)
        v
Pico 2 W / RP2350
        |
        +---- USB HID Keyboard ---> host input
        |
        +---- USB CDC ACM --------> /dev/ttyACM* debug logs
```

No UART adapter is required.

## Why this POC is different

- Bluetooth side is **Classic HID Host**, not BLE HOGP.
- USB identity is fixed from boot as a composite **CDC ACM + HID Keyboard**.
- The remote HID descriptor is parsed on the Pico and keyboard usages are normalized to
  a standard 8-byte USB keyboard report.
- There is no dynamic USB HID descriptor and no mid-pairing USB re-enumeration.
- BTstack runs on Core 1 with an explicit 8 KiB stack.
- TinyUSB and all CDC writes run on Core 0.
- Logs generated on either core are buffered in a cross-core queue and emitted over CDC.

## Build

Use Pico SDK 2.2.0.

```sh
export PICO_SDK_PATH="$HOME/pico/pico-sdk"

cd src_fw/poc_bkb3g_classic_hid_cdc
rm -rf build

cmake -S . -B build -G Ninja \
  -DPICO_BOARD=pico2_w \
  -DCMAKE_BUILD_TYPE=Debug

cmake --build build -j"$(nproc)"
```

Output:

```text
build/bkb3g_classic_hid_cdc.uf2
```

## Flash

1. Unplug the Pico 2 W.
2. Hold BOOTSEL.
3. Connect USB while holding BOOTSEL.
4. Copy `build/bkb3g_classic_hid_cdc.uf2` to the `RP2350` mass-storage volume.
5. The board reboots as a composite HID + CDC device.

## Logs without UART

After boot, Linux should expose a CDC ACM device:

```sh
ls -l /dev/ttyACM*
```

Open it with:

```sh
picocom -b 115200 /dev/ttyACM0
```

The CDC baud setting is nominal because USB CDC is not a physical UART.

Boot logs are buffered until a CDC terminal connects. If the device number is not
`ttyACM0`, use the path reported by `dmesg` or `journalctl -kf`.

## Pairing test

The tested keyboard is **Goldentec GT T1, code 40062**, advertised over Bluetooth as
`BKB-3G` / `Bluetooth keyboard 3.0`.

Before the next clean pairing test, erase the keyboard's own remembered Bluetooth
devices by holding **FN+ESC for 5 seconds**. Then:

1. Open the CDC log console first.
2. Put the BKB-3G into pairing mode using FN+1, FN+2, or FN+3 until its white LED blinks.
3. Watch for `FOUND:`, `MATCH:`, `BOND:`, `SSP:`, `HID: CONNECTION OPEN`,
   and `HID report descriptor`.
4. This model is configured for **Level 2 / MITM not required** bonding. A normal run
   should not require a displayed passkey.
5. When `POC READY` appears, open a text editor and test:
   - letters
   - Shift
   - Space
   - Enter
   - key release behavior

The POC also logs each received Classic HID input packet and the normalized USB
keyboard state.

## Current intentional limitations

- One target keyboard only.
- Target names are fixed to `Bluetooth keyboard 3.0` and `BKB-3G`.
- No GUI.
- No mouse.
- No remapping.
- No persistent selected-device UX.
- USB keyboard LED output is logged but is not sent back to the Bluetooth keyboard.
- Consumer/media usages are ignored in this first POC.
