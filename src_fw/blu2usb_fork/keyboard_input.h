#ifndef BLU2USB_FORK_KEYBOARD_INPUT_H
#define BLU2USB_FORK_KEYBOARD_INPUT_H

#include <stdint.h>

#define KEYBOARD_INPUT_KEYCODE_COUNT 6u

typedef enum {
    KEYBOARD_SOURCE_CLASSIC_HID = 1u,
    KEYBOARD_SOURCE_BLE_HOGP = 2u,
    KEYBOARD_SOURCE_BLE_COMPOSITE = 3u,
    KEYBOARD_SOURCE_SYNTHETIC = 4u,
} keyboard_input_source_t;

typedef struct {
    uint8_t source;
    uint8_t modifiers;
    uint8_t keycodes[KEYBOARD_INPUT_KEYCODE_COUNT];
} keyboard_input_snapshot_t;

_Static_assert(sizeof(keyboard_input_snapshot_t) == 8u,
               "Keyboard transport snapshot must remain compact and transport-neutral");

#endif
