# FORK-01 production composition and ownership foundation

## Gate scope

FORK-01 creates the first production-composition target for the BLU2USB reconstruction while preserving the physically accepted Classic POC as an untouched regression reference. It does not implement production Classic HID pairing/typing, BLE Mouse forwarding, HAT/LCD behavior or persistence semantics owned by later gates.

## Base and branch

- predecessor gate head: `daf8dd1ed9654ac230a7618a5e087fbde0ad6acb`;
- implementation branch: `fork/fork-01-production-foundation`;
- POC acceptance head retained as historical physical authority: `857fd66e64d7ca4c24586d962063c5d43e925eee`;
- physically accepted Classic implementation retained unchanged: `b04aaf146844cead484e2a4191a4b07806bdf6f6`.

## Production topology

### Core0

Core0 owns:

- board/TinyUSB servicing;
- application projection/service;
- the product-storage mutation boundary;
- consumption of Bluetooth status/input messages;
- release-all fallback when a release-sensitive BT publication cannot enter the bounded queue.

### Core1

Core1 owns exactly one CYW43/BTstack runtime for BLE + Classic. It uses an explicit 8 KiB stack, registers for the SDK flash-safe protocol, initializes the common dual-mode stack and executes the BTstack run loop. No application file initializes CYW43 or calls raw BTstack primitives.

### Cross-core communication

`bridge_bus` provides two fixed-capacity SPSC queues with C11 atomic sequence counters:

- Core0 -> Core1 commands;
- Core1 -> Core0 status/input events.

There is no heap allocation. Overflow is observable. A release-sensitive BT->Core0 overflow sets an independent atomic release latch so Core0 can neutralize USB ownership even if the event queue is saturated.

### USB ownership

`usb_hid` is the only TinyUSB owner. The production target disables SDK USB stdio and UART stdio and configures:

- two HID interfaces;
- zero CDC;
- zero MSC;
- zero MIDI;
- zero vendor interfaces.

The G06 VID/PID/revision/string/interface identity is reused as a no-debug foundation, but FORK-04 remains the gate that physically validates the exact fixed USB contract.

### Flash/storage boundary

Core0 initializes the product storage owner and future product mutations must enter through `storage_owner_flash_safe_execute`. Core1 independently joins the same Pico SDK flash-safe protocol before BTstack can persist link keys or BLE credentials. This establishes one product-storage entry point without creating a second Bluetooth runtime or a raw flash writer in application code.

## Classic invariant preservation

The accepted POC `src_fw/poc_bkb3g_classic_hid_cdc/classic_hid.c` is not edited in this gate. FORK-01 CI reruns the FORK-00 positive deferred-HID regression and synchronous-start negative control against the pinned BTstack revision. Production migration of `gap_dedicated_bonding` + deferred `hid_host_connect` is intentionally deferred to FORK-02.

## Physical-test decision

FORK-01 does not modify the physically accepted POC Classic path. The new production target is a composition/build foundation and does not yet claim production Keyboard functionality. Therefore the gate's conditional physical Classic smoke is not triggered. A UF2 is still built and retained as software-build evidence, but it is not promoted as a physically accepted product candidate.
