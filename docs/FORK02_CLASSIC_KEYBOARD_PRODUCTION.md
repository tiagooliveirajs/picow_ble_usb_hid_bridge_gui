# FORK-02 — production Classic Keyboard candidate

Status: **IMPLEMENTED — PHYSICAL ACCEPTANCE PENDING**

This gate moves the physically accepted GT T1 / BKB-3G Bluetooth Classic HID path into the FORK production composition without copying the POC's CDC/debug product personality or creating a second Bluetooth runtime.

## Architecture

- `bt_runtime.c` remains the single Core1 CYW43/BTstack lifecycle owner.
- `classic_keyboard.c` is a profile adapter only. It never initializes CYW43, L2CAP or HCI power.
- `keyboard_input.h` is the transport-neutral Keyboard boundary. It already reserves distinct source identities for Classic HID, BLE HOGP, BLE Composite and Synthetic input so FORK-02 does not make the product architecture Classic-only.
- Classic reports are parsed inside the adapter using the remote HID descriptor; HID transaction framing/report IDs do not leak into Core0 or TinyUSB.
- Core0 receives Keyboard snapshots through the bounded bridge bus and submits fixed 8-byte USB Keyboard reports through the sole `usb_hid` owner.
- Production USB remains the fixed G06 Mouse + Keyboard identity and has no CDC/UART diagnostic dependency.

## Pairing invariant retained

The accepted sequence remains normative:

1. Classic inquiry and remote-name resolution match `Bluetooth keyboard 3.0` / `BKB-3G`, never a fixed Bluetooth address.
2. Dedicated bonding uses `gap_dedicated_bonding(target, 0)` (Level 2/no MITM requirement).
3. SSP is `NoInputNoOutput`, general bonding, MITM not required, with Just Works auto-accept.
4. Successful `GAP_EVENT_DEDICATED_BONDING_COMPLETED` only queues `start_hid_after_bonding` with `btstack_run_loop_execute_on_main_thread` and returns.
5. Only that queued callback reaches `hid_host_connect(..., HID_PROTOCOL_MODE_REPORT, ...)`.
6. READY is published only after HID connection opens and a non-empty descriptor becomes available.

The accepted POC source remains unchanged and its positive/negative stale-ACL regression is rerun by FORK-02 CI.

## Bounded recovery

- Classic inquiry is bounded by the BTstack inquiry duration.
- Individual remote-name resolution has a 5 s watchdog.
- dedicated bonding has a 15 s watchdog.
- HID open/descriptor acquisition has a 15 s watchdog.
- failures release Keyboard state and retry after a bounded delay.
- transport-neutral commands exist for cancel and retry; later UI gates can invoke them without calling BTstack directly.
- disconnect/queue failure is release-safe and cannot intentionally preserve a stale Keyboard report.

## Acceptance boundary

Software build/host tests do not constitute physical PASS. FORK-02 remains open until the operator flashes the exact candidate UF2 and reports the numbered physical scenarios from the planner. Mouse coexistence, canonical multi-source ownership, BLE Keyboard and BLE Mouse parity are later gates and are not claimed here.
