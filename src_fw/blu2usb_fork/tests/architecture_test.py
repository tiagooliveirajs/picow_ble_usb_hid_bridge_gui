#!/usr/bin/env python3
from pathlib import Path
import sys

root = Path(sys.argv[1])
cmake = (root / "CMakeLists.txt").read_text()
main = (root / "main.c").read_text()
bt = (root / "bt_runtime.c").read_text()
usb = (root / "usb_hid.c").read_text()
desc = (root / "usb_descriptors.c").read_text()
tusb = (root / "tusb_config.h").read_text()

required = [
    "pico_btstack_ble",
    "pico_btstack_classic",
    "pico_btstack_cyw43",
    "pico_cyw43_arch_threadsafe_background",
    "pico_multicore",
    "tinyusb_device",
]
for token in required:
    assert token in cmake, f"missing production dependency: {token}"

assert "pico_enable_stdio_usb(blu2usb_fork_foundation 0)" in cmake
assert "pico_enable_stdio_uart(blu2usb_fork_foundation 0)" in cmake
assert "CFG_TUD_CDC 0" in tusb
assert "CFG_TUD_HID 2" in tusb
assert "CFG_TUD_MSC 0" in tusb
assert "CFG_TUD_MIDI 0" in tusb
assert "CFG_TUD_VENDOR 0" in tusb

for forbidden in ("btstack.h", "cyw43_arch", "hid_host_connect", "gap_dedicated_bonding"):
    assert forbidden not in main, f"application owns Bluetooth primitive: {forbidden}"

assert '"tusb.h"' not in main, "application owns TinyUSB directly"
assert "cyw43_arch_init" in bt
assert "btstack_run_loop_execute" in bt
assert "l2cap_init" in bt and "sm_init" in bt and "gatt_client_init" in bt
assert "SSP_IO_CAPABILITY_NO_INPUT_NO_OUTPUT" in bt
assert "SSP_IO_AUTHREQ_MITM_PROTECTION_NOT_REQUIRED_GENERAL_BONDING" in bt
assert "tud_task" in usb
assert "tud_disconnect" not in usb + desc + main
assert "tud_connect" not in usb + desc + main
assert "BLU2USB_USB_HID_MOUSE_INTERFACE 0u" in (root / "usb_hid.h").read_text()
assert "BLU2USB_USB_HID_KEYBOARD_INTERFACE 1u" in (root / "usb_hid.h").read_text()
assert "CORE1_STACK_BYTES 8192u" in main

# Product storage uses the SDK flash-safe coordination boundary and is Core0-only.
storage = (root / "storage_owner.c").read_text()
assert "flash_safe_execute_core_init" in storage
assert "flash_safe_execute(" in storage
assert "get_core_num() != 0u" in storage

print("PASS: FORK-01 architecture ownership, bounded bus, no-debug USB and flash boundary")
