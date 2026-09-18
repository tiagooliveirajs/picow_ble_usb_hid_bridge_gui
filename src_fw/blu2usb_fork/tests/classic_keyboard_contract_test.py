#!/usr/bin/env python3
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
classic = (root / "classic_keyboard.c").read_text()
runtime = (root / "bt_runtime.c").read_text()
main = (root / "main.c").read_text()
keyboard_input = (root / "keyboard_input.h").read_text()
cmake = (root / "CMakeLists.txt").read_text()

assert "20:20:01:60:0B:94" not in classic
assert 'TARGET_NAME "Bluetooth keyboard 3.0"' in classic
assert 'TARGET_NAME_ALIAS "BKB-3G"' in classic
assert re.search(r"gap_dedicated_bonding\s*\(\s*g_target_addr\s*,\s*0\s*\)", classic)
assert "SSP_IO_CAPABILITY_NO_INPUT_NO_OUTPUT" in runtime
assert "SSP_IO_AUTHREQ_MITM_PROTECTION_NOT_REQUIRED_GENERAL_BONDING" in runtime
assert "btstack_run_loop_execute_on_main_thread(&g_start_hid_callback)" in classic

bond_body = classic.split("static void handle_bonding_complete", 1)[1].split("static void request_next_remote_name", 1)[0]
assert "hid_host_connect(" not in bond_body
assert "btstack_run_loop_execute_on_main_thread" in bond_body

deferred_body = classic.split("static void start_hid_after_bonding", 1)[1].split("static void handle_bonding_complete", 1)[0]
assert "connect_target(g_target_addr)" in deferred_body
connect_body = classic.split("static void connect_target", 2)[2].split("static void start_hid_after_bonding", 1)[0]
assert "hid_host_connect(g_target_addr, HID_PROTOCOL_MODE_REPORT" in connect_body

for forbidden in ("cyw43_arch_init", "hci_power_control", "l2cap_init()"):
    assert forbidden not in classic, f"Classic adapter must not own runtime lifecycle: {forbidden}"

assert "classic_keyboard_init();" in runtime
assert "classic_keyboard_on_stack_working();" in runtime
assert "BONDING_TIMEOUT_MS 15000u" in classic
assert "HID_CONNECT_TIMEOUT_MS 15000u" in classic
assert "REMOTE_NAME_TIMEOUT_MS 5000u" in classic
assert "BT_COMMAND_CLASSIC_CANCEL" in runtime
assert "BT_COMMAND_CLASSIC_RETRY" in runtime

assert "KEYBOARD_SOURCE_CLASSIC_HID" in keyboard_input
assert "KEYBOARD_SOURCE_BLE_HOGP" in keyboard_input
assert "KEYBOARD_SOURCE_BLE_COMPOSITE" in keyboard_input
assert "BT_EVENT_KEYBOARD_SNAPSHOT" in main
assert "pico_enable_stdio_usb(blu2usb_fork_foundation 0)" in cmake
assert "pico_enable_stdio_uart(blu2usb_fork_foundation 0)" in cmake

print("PASS: FORK-02 Classic adapter contract, deferred HID, bounded recovery and transport-neutral Keyboard facade")
