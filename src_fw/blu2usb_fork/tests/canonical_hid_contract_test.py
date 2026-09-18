#!/usr/bin/env python3
from pathlib import Path
import sys

root = Path(sys.argv[1])
main = (root / "main.c").read_text()
canonical = (root / "canonical_hid.c").read_text()
sources = (root / "canonical_source.h").read_text()
classic = (root / "classic_keyboard.c").read_text()
bus = (root / "bridge_bus.c").read_text()
usb = (root / "usb_hid.c").read_text()

required_sources = [
    "CANONICAL_SOURCE_CLASSIC_KEYBOARD",
    "CANONICAL_SOURCE_BLE_HOGP_KEYBOARD",
    "CANONICAL_SOURCE_BLE_HOGP_MOUSE",
    "CANONICAL_SOURCE_BLE_COMPOSITE_KEYBOARD",
    "CANONICAL_SOURCE_BLE_COMPOSITE_MOUSE",
    "CANONICAL_SOURCE_SYNTHETIC_REMAP",
]
for token in required_sources:
    assert token in sources, f"missing canonical source identity: {token}"
assert "CANONICAL_SOURCE_CAPACITY 16u" in sources

assert "canonical_hid_apply_keyboard_snapshot" in main
assert "canonical_hid_release_source" in main
assert "bridge_bus_take_release_sources" in main
assert "usb_hid_replace_keyboard_state" in main
assert "usb_hid_submit_keyboard" in main
assert "keyboard_input_snapshot_t snapshot" in main

assert '"usb_hid.h"' not in classic
assert "tud_" not in classic
assert "btstack" not in canonical.lower()
assert "tud_" not in canonical
assert '"tusb.h"' not in canonical

assert "g_release_source_mask" in bus
assert "payload[0]" in bus
assert "bridge_bus_take_release_sources" in bus
assert "usb_hid_release_all();" in main

submit_start = usb.index("bool usb_hid_submit_keyboard")
submit_end = usb.index("void usb_hid_task", submit_start)
submit_body = usb[submit_start:submit_end]
assert "usb_hid_release_keyboard" not in submit_body
assert "keyboard_report_queue_push" in submit_body

print("PASS: FORK-03 canonical ownership, 16-source capacity and source-aware recovery contract")
