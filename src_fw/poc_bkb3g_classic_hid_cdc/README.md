# RP2350 BKB-3G Bluetooth Classic HID -> USB HID + CDC POC

This branch contains a deliberately small diagnostic POC derived from the repository's
`main` branch. It does not modify `main`.

## Goal

**Physical acceptance recorded:** the operator paired the GT T1 and typed a/s/d
on 2026-09-18 with `b04aaf1`. See [the durable solution and evidence record](../../docs/BKB3G_ACCEPTED_SOLUTION.md)
for the exact sequencing invariant, log excerpts, source SHA and acceptance limits.

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

## Post-bond HID stall candidate: deferred-hid-after-bond-v1

The latest physical log at base `09221f86a4960ab1dae303838d76243a89fad319`
shows successful SSP and dedicated bonding (`0x00`), then stops after HID CID
allocation. It does **not** yet demonstrate a connected HID profile or USB typing.

In Pico SDK 2.2.0's pinned BTstack
`501e6d2b86e6c92bfb9c390bcf55709938e25ac1`, `hci.c` emits the dedicated-bonding
result inside the disconnection handler, before resetting/removing the old ACL.
`hid_host_connect(REPORT)` can immediately start SDP through a synchronous
`sdp_client_register_query_callback`. Starting that work inside the bonding
callback therefore races the old connection's teardown.
Specifically, L2CAP moves to `WAIT_CONNECTION_COMPLETE` and sends Create Connection,
but HCI still sees the old `SENT_DISCONNECT` entry and suppresses that command as
`ERROR_CODE_ACL_CONNECTION_ALREADY_EXISTS`. The old entry is only removed later.

This candidate queues HID startup with `btstack_run_loop_execute_on_main_thread`.
The SDK async-context implementation executes the queued callback after event
dispatch returns. The pending-state guard prevents a duplicate outgoing launch
if an incoming connection has already taken over. Bond completion is checked
against the active target address. Level 2/NoInputNoOutput, flash TLV, the two-core
split, USB CDC and report normalization remain as before.

New logs identify the build, deferred launch, ACL connection/authentication/
encryption/disconnection and the SDP launch. A one-shot 30-second diagnostic
reports unfinished HID setup without starting another pairing attempt. This is
an observation timer, not a claim that the profile has failed or a radio timeout.
`SDP_ready=0` means the SDP client is busy; it does not alone prove packet exchange.

Expected additional markers (other events may be interleaved):

```text
BUILD: deferred-hid-after-bond-v1
BOND: HID start queued until HCI cleanup finishes
ACL: disconnection ...
CONNECT: deferred HID start after bonding event cleanup
SDP: HID host will query service 0x1124 ...
ACL: connection complete ...
HID: CONNECTION OPEN ...
HID report descriptor ...
POC READY ...
```

Validation: host GCC syntax checking with `-Wall -Wextra -Werror` against the
exact BTstack headers; production transition tests for deferred execution,
duplicate/foreign/failed completions, incoming takeover, immediate connect error,
and diagnostic behavior. A scratch-only negative control restoring synchronous
startup fails the old-ACL invariant. These are host tests with mocked hardware
APIs; they do not execute the controller or prove physical compatibility.
The initial implementation run did not build ARM firmware or test hardware.
The operator subsequently compiled it locally and confirmed pairing and a/s/d;
the linked acceptance record supersedes the initial pending status for that scope.

Optional host regression test (from this directory):

```sh
PICO_SDK_PATH="$HOME/pico/pico-sdk" bash tests/run_host_test.sh
```

Implementation references:

- [BTstack HCI lifecycle](https://github.com/bluekitchen/btstack/blob/501e6d2b86e6c92bfb9c390bcf55709938e25ac1/src/hci.c)
- [HID host](https://github.com/bluekitchen/btstack/blob/501e6d2b86e6c92bfb9c390bcf55709938e25ac1/src/classic/hid_host.c)
- [SDP client](https://github.com/bluekitchen/btstack/blob/501e6d2b86e6c92bfb9c390bcf55709938e25ac1/src/classic/sdp_client.c)
- [SDK queued callback implementation](https://github.com/raspberrypi/pico-sdk/blob/2.2.0/src/rp2_common/pico_btstack/btstack_run_loop_async_context.c)

## Current intentional limitations

- One target keyboard only.
- Target names are fixed to `Bluetooth keyboard 3.0` and `BKB-3G`.
- No GUI.
- No mouse.
- No remapping.
- No persistent selected-device UX.
- USB keyboard LED output is logged but is not sent back to the Bluetooth keyboard.
- Consumer/media usages are ignored in this first POC.
