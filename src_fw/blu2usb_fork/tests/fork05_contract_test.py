#!/usr/bin/env python3
from pathlib import Path
import sys
root=Path(sys.argv[1] if len(sys.argv)>1 else '.').resolve()
model=(root/'ui_model.c').read_text(); renderer=(root/'ui_renderer.h').read_text(); hat=(root/'hat.h').read_text(); lcd=(root/'st7789_pico.c').read_text(); runtime=(root/'ui_runtime.c').read_text(); main=(root/'main.c').read_text(); cmake=(root/'CMakeLists.txt').read_text(); bt=(root/'bt_runtime.c').read_text(); mouse=(root/'ble_mouse.c').read_text(); classic=(root/'classic_keyboard.c').read_text(); usb=(root/'usb_hid.c').read_text()
assert 'PRESS TO LEARN A KEY' in model
assert 'KEY X: HELP' in model and 'KEY C: HELP' not in model
assert 'GO TO HOME' not in model and 'JOY UP / DOWN: SELECT' in model and 'ANY KEY: BACK' in model
for s in ['#define BLU2USB_RENDERER_WIDTH 240u','#define BLU2USB_RENDERER_HEIGHT 240u','#define BLU2USB_RENDERER_TEXT_ROWS 9u','#define BLU2USB_RENDERER_TEXT_COLS 21u','#define BLU2USB_RENDERER_TEXT_X 7u','#define BLU2USB_RENDERER_TEXT_Y 8u','#define BLU2USB_RENDERER_CHAR_ADVANCE 11u']: assert s in renderer,s
for pin in ['JOY_UP 2u','JOY_PRESS 3u','KEY_A 15u','JOY_LEFT 16u','KEY_B 17u','JOY_DOWN 18u','KEY_X 19u','JOY_RIGHT 20u','KEY_Y 21u']: assert pin in hat,pin
for pin in ['LCD_PIN_DC 8u','LCD_PIN_CS 9u','LCD_PIN_SCK 10u','LCD_PIN_MOSI 11u','LCD_PIN_RST 12u','LCD_PIN_BL 13u']: assert pin in lcd,pin
assert 'g_ux.screen = BLU2USB_SCREEN_LEARN_KEYS;' in runtime
assert runtime.index('g_ux.screen = BLU2USB_SCREEN_LEARN_KEYS;') < runtime.index('render_state()')
assert 'blu2usb_st7789_pico_set_backlight(false)' in runtime and 'was_locked && !is_locked' in runtime
assert 'BT_COMMAND_CLASSIC_RETRY' in runtime and 'BT_COMMAND_CLASSIC_CANCEL' in runtime
assert 'BT_COMMAND_BLE_MOUSE_RETRY' in runtime and 'BT_COMMAND_BLE_MOUSE_CANCEL' in runtime
assert 'BT_EVENT_BLE_MOUSE_READY' in runtime and 'blu2usb_ux_set_mouse_connected(true)' in runtime
assert 'btstack.h' not in runtime and 'cyw43_arch_init' not in runtime
assert 'ble_mouse_init();' in bt and 'classic_keyboard_init();' in bt
assert 'cyw43_arch_init' in bt and 'hci_power_control(HCI_POWER_ON)' in bt
assert 'cyw43_arch_init' not in mouse and 'hci_power_control(HCI_POWER_ON)' not in mouse
assert 'hids_client_init' in mouse and 'gap_load_resolving_list_from_le_device_db' in mouse and 'gap_connect_with_whitelist' in mouse and 'BLE_MOUSE_BONDED_RECONNECT_TIMEOUT_MS 8000u' in mouse and 'handle != g_connection_handle' in mouse
assert 'BT_EVENT_BLE_MOUSE_EVENT' in main and 'canonical_hid_apply_mouse' in main and 'service_usb_mouse();' in main
assert 'usb_hid_submit_mouse' in main and 'usb_hid_submit_mouse' in usb
assert 'tud_disconnect(' not in main + usb and 'tud_connect(' not in main + usb
assert 'ble_mouse.c ble_mouse_parser.c' in cmake and 'pico_btstack_make_gatt_header' in cmake
assert 'hardware_spi' in cmake and 'pico_enable_stdio_usb(blu2usb_fork_foundation 0)' in cmake

# User-directed live-state UX amendment for the current gate.
for text in [' MOUSE PAIRED',' KEYBOARD PAIRED','KEYBOARD CONNECTED','KEYBOARD PAIRED']:
    assert text in runtime, text
assert 'connected ? " MOUSE PAIRED" : " PAIR MOUSE"' in runtime
assert 'g_keyboard_connected ? " KEYBOARD PAIRED" : " PAIR KEYBOARD"' in runtime
assert 'if (!g_keyboard_connected) send_bt_command(BT_COMMAND_CLASSIC_RETRY);' in runtime
assert 'BLU2USB_UI_TONE_CURRENT' in runtime and 'BLU2USB_UI_TONE_EMPHASIZED' in runtime

# Composite transport is still FORK-12, but its future paired labels/status are
# already frozen in the projection code without claiming a working transport.
assert 'g_composite_connected ? " COMPOSITE PAIRED" : " PAIR COMPOSITE"' in runtime
assert 'COMPOSITE CONNECTED' in runtime
assert 'Composite transport is a FORK-12 feature' in runtime

# Current-session Classic reconnect and stale-key recovery are regressions, not
# FORK-11 preferred/cold-boot persistence work.
assert 'state_allows_known_target_reconnect' in classic
assert 'gap_drop_link_key_for_bd_addr(g_target_addr)' in classic

print('PASS: FORK-05 Learn boot + live paired UI + Mouse/Classic reconnect remediation contract')
