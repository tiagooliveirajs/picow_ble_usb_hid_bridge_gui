#!/usr/bin/env python3
from pathlib import Path
import sys

root = Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
classic = (root / "classic_keyboard.c").read_text()
mouse = (root / "ble_mouse.c").read_text()

# Classic Keyboard: once the peer has been READY in this live session, a
# sleep/power-save disconnect must not collapse into discoverable-only inquiry.
for token in [
    "CLASSIC_KNOWN_RECONNECT_WAIT",
    "KNOWN_RECONNECT_CONNECT_TIMEOUT_MS 5000u",
    "KNOWN_RECONNECT_RETRY_MS 1000u",
    "schedule_known_reconnect",
    "g_known_reconnect_attempt",
]:
    assert token in classic, token

retry_body = classic.split("static void retry_timeout", 1)[1].split(
    "static void schedule_retry", 1
)[0]
assert "CLASSIC_KNOWN_RECONNECT_WAIT" in retry_body
assert "connect_target(g_target_addr)" in retry_body

known_body = classic.split("static void schedule_known_reconnect", 2)[2].split(
    "static void phase_timeout", 1
)[0]
assert "g_target_valid" in known_body
assert "CLASSIC_KNOWN_RECONNECT_WAIT" in known_body
assert "BT_EVENT_CLASSIC_RETRYING" in known_body

closed_body = classic.split("case HID_SUBEVENT_CONNECTION_CLOSED", 1)[1].split(
    "case HID_SUBEVENT_SET_PROTOCOL_RESPONSE", 1
)[0]
assert "schedule_known_reconnect(KNOWN_RECONNECT_RETRY_MS)" in closed_body
assert "schedule_retry(RETRY_DELAY_MS)" in closed_body

hci_disc = classic.split("case HCI_EVENT_DISCONNECTION_COMPLETE", 1)[1].split(
    "case HCI_EVENT_PIN_CODE_REQUEST", 1
)[0]
assert "schedule_known_reconnect(KNOWN_RECONNECT_RETRY_MS)" in hci_disc

manual = classic.split("case BT_COMMAND_CLASSIC_RETRY", 1)[1].split(
    "default:", 1
)[0]
assert "g_known_reconnect_attempt = false" in manual
assert "schedule_retry(MANUAL_RETRY_DELAY_MS)" in manual

# BLE Mouse: a bonded reconnect timeout must not degrade into permanent generic
# scan. Recovery alternates bounded bonded-initiation and scan-fallback windows
# until the known peer returns; manual Pair/Retry explicitly exits that cycle.
for token in [
    "BLE_MOUSE_BONDED_RECONNECT_TIMEOUT_MS 8000u",
    "BLE_MOUSE_RECOVERY_SCAN_WINDOW_MS 3000u",
    "g_recovery_cycle_active",
    "start_scan(bool resume_bonded_recovery)",
]:
    assert token in mouse, token

timer_body = mouse.split("static void reconnect_timeout_handler", 1)[1].split(
    "static bool start_bonded_reconnect", 1
)[0]
assert "BLE_MOUSE_SCANNING && g_recovery_cycle_active" in timer_body
assert "start_bonded_reconnect()" in timer_body
assert "start_scan(resume_bonded_recovery)" in timer_body

scan_body = mouse.split("static void start_scan(bool resume_bonded_recovery)", 2)[2].split(
    "static void reconnect_timeout_handler", 1
)[0]
assert "g_recovery_cycle_active = resume_bonded_recovery" in scan_body
assert "BLE_MOUSE_RECOVERY_SCAN_WINDOW_MS" in scan_body

gap_complete = mouse.split("case HCI_EVENT_META_GAP", 1)[1].split(
    "case HCI_EVENT_DISCONNECTION_COMPLETE", 1
)[0]
assert "const bool resume_bonded_recovery = g_recovery_cycle_active" in gap_complete
assert "start_scan(resume_bonded_recovery)" in gap_complete

manual_mouse = mouse.split("case BT_COMMAND_BLE_MOUSE_RETRY", 1)[1].split(
    "case BT_COMMAND_BLE_MOUSE_CANCEL", 1
)[0]
assert "g_recovery_cycle_active = false" in manual_mouse
assert "start_scan(false)" in manual_mouse

print("PASS: FORK-05 sleep/power-cycle reconnect recovery contract")
