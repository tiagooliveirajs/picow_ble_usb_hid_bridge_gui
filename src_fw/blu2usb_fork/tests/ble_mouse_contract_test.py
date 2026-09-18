#!/usr/bin/env python3
from pathlib import Path
import sys
root=Path(sys.argv[1]).resolve()
mouse=(root/'ble_mouse.c').read_text()
parser=(root/'ble_mouse_parser.c').read_text()
bt=(root/'bt_runtime.c').read_text()
header=(root/'bt_runtime.h').read_text()
bridge=(root/'bridge_bus.c').read_text()
for token in ['ORG_BLUETOOTH_SERVICE_HUMAN_INTERFACE_DEVICE','HID_PROTOCOL_MODE_REPORT','SM_EVENT_PAIRING_COMPLETE','SM_EVENT_REENCRYPTION_COMPLETE','gap_load_resolving_list_from_le_device_db','gap_connect_with_whitelist','BLE_MOUSE_BONDED_RECONNECT_TIMEOUT_MS 8000u','BT_EVENT_BLE_MOUSE_RELEASE_SOURCE']:
    assert token in mouse or token in header, token
assert 'handle != g_connection_handle' in mouse
for forbidden in ['cyw43_arch_init', 'hci_power_control(HCI_POWER_ON)', 'btstack_run_loop_execute']:
    assert forbidden not in mouse, forbidden
for required in ['cyw43_arch_init', 'hci_power_control(HCI_POWER_ON)', 'btstack_run_loop_execute']:
    assert required in bt, required
for required in ['HID_USAGE_MOUSE 0x02u','HID_USAGE_AC_PAN 0x0238u','report_len == expected_len + 1u','report[0] == report_id','in_mouse_collection']:
    assert required in parser, required
assert 'latch_release_for_failed_message' in bridge
assert 'canonical_source_from_input_prefix' in bridge
print('PASS: FORK-05 BLE Mouse G06 reconnect/parser/coexistence source contract')
