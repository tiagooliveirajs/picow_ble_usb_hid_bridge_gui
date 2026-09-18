# GT T1 / BKB-3G: physically accepted Classic HID solution

**Accepted by the operator on 2026-09-18.** The user reported that pairing
succeeded and pressing **a, s, d** typed those letters on the PC. The supplied
log shows their press and release reports reaching the USB keyboard path.

## Immutable implementation

- Repository: `tiagooliveirajs/picow_ble_usb_hid_bridge_gui`
- Branch: `poc/rp2350-classic-hid-usb-cdc` (main is not changed).
- Accepted implementation: [`b04aaf146844cead484e2a4191a4b07806bdf6f6`](https://github.com/tiagooliveirajs/picow_ble_usb_hid_bridge_gui/commit/b04aaf146844cead484e2a4191a4b07806bdf6f6)
- Firmware marker: `deferred-hid-after-bond-v1`.
- Hardware: Pico 2 W / RP2350; GT T1 code 40062, address `20:20:01:60:0B:94`.
- SDK: 2.2.0; BTstack `501e6d2b86e6c92bfb9c390bcf55709938e25ac1`.
- The operator compiled locally. No hash of that physical UF2 was supplied;
  do not invent one or extend this acceptance to another build or repository.

## Required sequence — do not lose this ordering

1. Classic inquiry and remote-name match `Bluetooth keyboard 3.0` / `BKB-3G`.
2. Dedicated bonding with `gap_dedicated_bonding(address, 0)` (Level 2).
3. SSP `NoInputNoOutput`, MITM not required, bonding, Just Works acceptance.
4. On successful `GAP_EVENT_DEDICATED_BONDING_COMPLETED`, **queue** HID startup
   with `btstack_run_loop_execute_on_main_thread` and return from the event.
5. Only in the queued callback call `hid_host_connect(..., HID_PROTOCOL_MODE_REPORT, ...)`.
6. Wait for HID open and descriptor; parse HID report IDs inside the adapter.
7. Forward keyboard usages through fixed USB keyboard ownership.

BTstack emits dedicated-bonding completion *inside* the disconnection handler,
before the old ACL state is reset/removed. Synchronous HID startup immediately
starts SDP/L2CAP against that stale ACL. Moving to a queued callback is the
essential correction; a successful CID allocation alone is not HID connection.
Do not replace it with a guessed sleep or restore synchronous startup.

Preserve the verified execution envelope when migrating: one Bluetooth runtime
on Core1 with an explicit 8 KiB stack, USB serviced on Core0, cross-core messages,
and flash-safe participation so TLV link-key writes can proceed. Product-specific
USB descriptors/UI and canonical ownership must be integrated at their own
boundaries; the diagnostic CDC interface is a POC feature, not a requirement for
the production product.

## Physical evidence (verbatim excerpts from the user's log)

```text
[0000000007] BUILD: deferred-hid-after-bond-v1
[0000053277] NAME: 20:20:01:60:0B:94 -> 'Bluetooth keyboard 3.0'
[0000053781] PAIRING: complete status=0x00
[0000053827] ACL: authentication status=0x00 handle=0x000b
[0000053851] ACL: encryption status=0x00 handle=0x000b enabled=1
[0000053937] BOND: dedicated bonding complete status=0x00
[0000053938] BOND: Level 2 bonding complete (no MITM guarantee)
[0000053938] BOND: HID start queued until HCI cleanup finishes
[0000053938] ACL: disconnection status=0x00 handle=0x000b reason=0x16
[0000053939] CONNECT: deferred HID start after bonding event cleanup
[0000053939] CONNECT: bonding complete; opening Bluetooth Classic HID host connection
[0000053939] SDP: HID host will query service 0x1124; client_ready=1
[0000053940] CONNECT: HID cid allocated 0x0001
[0000054429] ACL: connection complete addr=20:20:01:60:0B:94 status=0x00 handle=0x000b
[0000054736] HID: sniff-subrating parameters received
[0000054763] ACL: authentication status=0x00 handle=0x000b
[0000054785] ACL: encryption status=0x00 handle=0x000b enabled=1
[0000054857] HID: CONNECTION OPEN cid=0x0001
[0000054857] HID report descriptor (263 bytes)
[0000054864] POC READY: Classic HID connected; USB keyboard path active
[0000067853] CLASSIC HID input (10 bytes)
[0000067853]   0000: a1 01 00 00 04 00 00 00 00 00
[0000067855] USB-KBD: mod=00 keys=04 00 00 00 00 00
[0000068011] CLASSIC HID input (10 bytes)
[0000068011]   0000: a1 01 00 00 00 00 00 00 00 00
[0000068012] USB-KBD: mod=00 keys=00 00 00 00 00 00
[0000068618] CLASSIC HID input (10 bytes)
[0000068618]   0000: a1 01 00 00 16 00 00 00 00 00
[0000068620] USB-KBD: mod=00 keys=16 00 00 00 00 00
[0000068776] CLASSIC HID input (10 bytes)
[0000068776]   0000: a1 01 00 00 00 00 00 00 00 00
[0000068777] USB-KBD: mod=00 keys=00 00 00 00 00 00
[0000069068] CLASSIC HID input (10 bytes)
[0000069068]   0000: a1 01 00 00 07 00 00 00 00 00
[0000069070] USB-KBD: mod=00 keys=07 00 00 00 00 00
[0000069248] CLASSIC HID input (10 bytes)
[0000069248]   0000: a1 01 00 00 00 00 00 00 00 00
[0000069250] USB-KBD: mod=00 keys=00 00 00 00 00 00
```

`0x16` on the deliberate bonding disconnect is local-host termination, not a
pairing failure. Sniff-subrating is informational. The report with ID `03` is
not a keyboard-usage report and is intentionally ignored.

## Acceptance limits

Proven here: discovery, Level 2 pairing/bonding, deferred reconnect, 263-byte HID
descriptor, USB typing a/s/d and their releases. Not yet proven by this log:
modifiers, broad keyboard layout coverage, cold-boot reconnect, simultaneous BLE
Mouse, HAT integration, synthetic Escape coexistence or the blu2usb G07 build.
Those need the new product candidate's own physical acceptance.

Host regression: `src_fw/poc_bkb3g_classic_hid_cdc/tests/run_host_test.sh`.
The synchronous-start negative control must fail. The implementation README
links the pinned upstream HCI, SDP, HID host and SDK run-loop sources.
