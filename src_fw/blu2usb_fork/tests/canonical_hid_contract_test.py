#!/usr/bin/env python3
from pathlib import Path
import sys

root = Path(sys.argv[1])
main = (root / "main.c").read_text()
canonical = (root / "canonical_hid.c").read_text()
sources = (root / "canonical_source.h").read_text()
keyboard = (root / "keyboard_input.h").read_text()
classic = (root / "classic_keyboard.c").read_text()
bus = (root / "bridge_bus.c").read_text()
usb = (root / "usb_hid.c").read_text()

required_kinds = [
    "CANONICAL_SOURCE_KIND_CLASSIC_KEYBOARD",
    "CANONICAL_SOURCE_KIND_BLE_HOGP_KEYBOARD",
    "CANONICAL_SOURCE_KIND_BLE_HOGP_MOUSE",
    "CANONICAL_SOURCE_KIND_BLE_COMPOSITE_KEYBOARD",
    "CANONICAL_SOURCE_KIND_BLE_COMPOSITE_MOUSE",
    "CANONICAL_SOURCE_KIND_SYNTHETIC_REMAP",
]
for token in required_kinds:
    assert token in sources, f"missing canonical source kind: {token}"
assert "CANONICAL_SOURCE_CAPACITY 16u" in sources
assert "uint16_t instance" in sources
assert "canonical_source_equal" in sources
assert "source_instance_lo" in keyboard and "source_instance_hi" in keyboard

assert "canonical_hid_apply_keyboard_snapshot" in main
assert "canonical_hid_release_source" in main
assert "bridge_bus_take_release_sources" in main
assert "usb_hid_replace_keyboard_state" in main
assert "usb_hid_submit_keyboard" in main

# Accepted Classic transport code remains transport-specific and does not own TinyUSB.
assert '"usb_hid.h"' not in classic
assert "tud_" not in classic

# Canonical ownership remains independent from BTstack and TinyUSB.
assert "btstack" not in canonical.lower()
assert "tud_" not in canonical
assert '"tusb.h"' not in canonical
assert "canonical_hid_apply_mouse" in canonical
assert "CANONICAL_MOUSE_EVENT_MOVE" in canonical
assert "canonical_hid_consume_relative" in canonical

# Normal release-sensitive overflow keeps exact kind+instance source identity.
assert "g_release_sources" in bus
assert "canonical_source_from_input_prefix" in bus
assert "canonical_source_encode" in bus
assert "bridge_bus_take_release_sources" in bus
assert "usb_hid_release_all();" in main  # unknown-source/latch-exhaustion fallback only

# USB queue overflow no longer silently converts every source to neutral.
submit_start = usb.index("bool usb_hid_submit_keyboard")
submit_end = usb.index("void usb_hid_task", submit_start)
submit_body = usb[submit_start:submit_end]
assert "usb_hid_release_keyboard" not in submit_body
assert "keyboard_report_queue_push" in submit_body

print("PASS: FORK-03 kind+instance canonical ownership, 16-source capacity and source-aware recovery")
