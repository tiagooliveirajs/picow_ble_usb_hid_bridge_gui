#!/usr/bin/env python3
from pathlib import Path
import sys
root=Path(sys.argv[1] if len(sys.argv)>1 else '.').resolve()
model=(root/'ui_model.c').read_text()
renderer=(root/'ui_renderer.h').read_text()
hat=(root/'hat.h').read_text()
lcd=(root/'st7789_pico.c').read_text()
runtime=(root/'ui_runtime.c').read_text()
main=(root/'main.c').read_text()
cmake=(root/'CMakeLists.txt').read_text()
assert 'PRESS TO LEARN A KEY' in model
assert 'KEY X: HELP' in model and 'KEY C: HELP' not in model
assert 'GO TO HOME' not in model
assert 'JOY UP / DOWN: SELECT' in model
assert 'ANY KEY: BACK' in model
for s in ['#define BLU2USB_RENDERER_WIDTH 240u','#define BLU2USB_RENDERER_HEIGHT 240u','#define BLU2USB_RENDERER_TEXT_ROWS 9u','#define BLU2USB_RENDERER_TEXT_COLS 21u','#define BLU2USB_RENDERER_TEXT_X 7u','#define BLU2USB_RENDERER_TEXT_Y 8u','#define BLU2USB_RENDERER_CHAR_ADVANCE 11u']:
    assert s in renderer,s
for pin in ['JOY_UP 2u','JOY_PRESS 3u','KEY_A 15u','JOY_LEFT 16u','KEY_B 17u','JOY_DOWN 18u','KEY_X 19u','JOY_RIGHT 20u','KEY_Y 21u']:
    assert pin in hat,pin
for pin in ['LCD_PIN_DC 8u','LCD_PIN_CS 9u','LCD_PIN_SCK 10u','LCD_PIN_MOSI 11u','LCD_PIN_RST 12u','LCD_PIN_BL 13u']:
    assert pin in lcd,pin
assert 'blu2usb_st7789_pico_set_backlight(false)' in runtime
assert 'was_locked && !is_locked' in runtime
assert 'BT_COMMAND_CLASSIC_RETRY' in runtime and 'BT_COMMAND_CLASSIC_CANCEL' in runtime
assert 'btstack.h' not in runtime and 'cyw43_arch_init' not in runtime
assert 'usb_hid_task();application_service();ui_runtime_task();' in main
assert 'hardware_spi' in cmake and 'pico_enable_stdio_usb(blu2usb_fork_foundation 0)' in cmake
print('PASS: FORK-05 G06 HAT/LCD/interaction source contract')
