#!/usr/bin/env python3
from pathlib import Path
import re
import sys

root = Path(sys.argv[1]).resolve()
header = (root / "usb_hid.h").read_text()
tusb = (root / "tusb_config.h").read_text()
desc = (root / "usb_descriptors.c").read_text()
main = (root / "main.c").read_text()
cmake = (root / "CMakeLists.txt").read_text()

expected_header = {
    "BLU2USB_USB_VID": "UINT16_C(0xCAFE)",
    "BLU2USB_USB_PID": "UINT16_C(0x4010)",
    "BLU2USB_USB_BCD_DEVICE": "UINT16_C(0x0100)",
    "BLU2USB_USB_HID_MOUSE_INTERFACE": "0u",
    "BLU2USB_USB_HID_KEYBOARD_INTERFACE": "1u",
    "BLU2USB_USB_HID_INTERFACE_COUNT": "2u",
}
for name, value in expected_header.items():
    pattern = rf"^#define\s+{re.escape(name)}\s+{re.escape(value)}\s*$"
    assert re.search(pattern, header, re.MULTILINE), f"fixed USB define drifted: {name}"

assert '#define BLU2USB_USB_MANUFACTURER "BLU2USB"' in header
assert '#define BLU2USB_USB_PRODUCT "BLU2USB Mouse + Keyboard"' in header

classes = {
    "CFG_TUD_CDC": "0",
    "CFG_TUD_HID": "2",
    "CFG_TUD_MSC": "0",
    "CFG_TUD_MIDI": "0",
    "CFG_TUD_VENDOR": "0",
}
for name, value in classes.items():
    assert re.search(rf"^#define\s+{name}\s+{value}\s*$", tusb, re.MULTILINE), f"USB class drifted: {name}"

assert desc.count("TUD_HID_DESCRIPTOR(") == 2, "must expose exactly Mouse + Keyboard HID interfaces"
assert "TUD_HID_REPORT_DESC_MOUSE()" in desc
assert "TUD_HID_REPORT_DESC_KEYBOARD()" in desc
assert "HID_ITF_PROTOCOL_MOUSE" in desc
assert "HID_ITF_PROTOCOL_KEYBOARD" in desc
assert "#define EPNUM_MOUSE 0x81u" in desc
assert "#define EPNUM_KEYBOARD 0x82u" in desc
assert desc.index("ITF_NUM_MOUSE") < desc.index("ITF_NUM_KEYBOARD")

# USB is brought up before the radio runtime, so both fixed interfaces exist
# without a Bluetooth peer.
assert main.index("usb_hid_init()") < main.index("multicore_launch_core1_with_stack")

# Production must not gain a runtime disconnect/reconnect identity workaround.
for path in root.glob("*.c"):
    text = path.read_text()
    assert "tud_disconnect(" not in text, f"runtime USB disconnect forbidden: {path.name}"
    assert "tud_connect(" not in text, f"runtime USB reconnect forbidden: {path.name}"

assert "pico_enable_stdio_usb(blu2usb_fork_foundation 0)" in cmake
assert "pico_enable_stdio_uart(blu2usb_fork_foundation 0)" in cmake

print("PASS: FORK-04 fixed descriptors/classes, boot independence and no re-enumeration path")
