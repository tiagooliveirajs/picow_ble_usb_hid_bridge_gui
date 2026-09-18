# BLU2USB fork production foundation (FORK-01)

This directory is the production composition introduced by FORK-01. It is intentionally parallel to `../poc_bkb3g_classic_hid_cdc`; the physically accepted POC remains an immutable regression reference while production modules are reconstructed behind explicit ownership boundaries.

## Ownership

- **Core0**: TinyUSB, application projection/service, product-storage entry point.
- **Core1**: the single CYW43/BTstack owner for both BLE and Bluetooth Classic.
- **Cross-core traffic**: two fixed-capacity SPSC rings in `bridge_bus.c`; no dynamic allocation.
- **USB**: only `usb_hid.c` + `usb_descriptors.c` include/use TinyUSB. Production exposes HID Mouse + HID Keyboard only; no CDC, UART stdio, MSC, MIDI or vendor debug interface.
- **Bluetooth**: only `bt_runtime.c` initializes CYW43/BTstack. The application never calls raw BTstack primitives.
- **Storage**: `storage_owner.c` is the Core0-only entry point for future product flash mutations and routes them through the Pico SDK flash-safe protocol. Core1 also registers for flash-safe participation before BTstack may persist credentials.

## Deliberate FORK-01 limits

This gate establishes composition, ownership and buildability. It does **not** migrate Classic HID pairing into the production target yet; that is FORK-02. Therefore the accepted POC `classic_hid.c` is not modified by FORK-01.

The production BT runtime initializes the common dual-mode stack (L2CAP, BLE SM/GATT client, Classic SDP/GAP policy) and maintains the accepted Classic security envelope (`NoInputNoOutput`, bonding without a MITM requirement). HID profile startup remains protected by the unchanged FORK-00 positive/negative regression tests until FORK-02 moves that sequence behind the production adapter.

The fixed G06 USB identity is reused now so the production target never grows a temporary debug personality. Physical fixed-USB acceptance remains FORK-04; its early presence here is a software composition choice, not a claim that FORK-04 passed.

## Release safety

`bridge_bus_publish_bt_event(..., true)` is for input/state transitions whose loss could otherwise strand a held USB target. If the bounded BT->Core0 ring is full, a separate atomic release latch is set. Core0 consumes that latch before normal queued events and asks `usb_hid` to submit neutral Mouse and Keyboard reports. Later HID gates may strengthen ordering, source identity and snapshots without weakening this fallback.

## Validation

Host:

```sh
bash tests/run_host_tests.sh
```

Pico 2 W:

```sh
cmake -S . -B build -G Ninja -DPICO_BOARD=pico2_w -DPICO_SDK_PATH=/path/to/pico-sdk
cmake --build build --target blu2usb_fork_foundation
```

FORK-01 CI additionally reruns the FORK-00 deferred-HID regression against BTstack `501e6d2b86e6c92bfb9c390bcf55709938e25ac1`, verifies the accepted POC `classic_hid.c` is unchanged from FORK-00, cross-builds a non-empty Pico 2 W UF2 and publishes the UF2 plus SHA-256 as a workflow artifact.
