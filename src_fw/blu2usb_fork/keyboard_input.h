#ifndef BLU2USB_FORK_KEYBOARD_INPUT_H
#define BLU2USB_FORK_KEYBOARD_INPUT_H

#include <stdint.h>

#include "canonical_source.h"

#define KEYBOARD_INPUT_KEYCODE_COUNT 6u

/* Compatibility names retained for transport adapters. Canonical source IDs
 * are global across Keyboard, Mouse, Composite and Synthetic producers. */
typedef canonical_source_id_t keyboard_input_source_t;
#define KEYBOARD_SOURCE_CLASSIC_HID CANONICAL_SOURCE_CLASSIC_KEYBOARD
#define KEYBOARD_SOURCE_BLE_HOGP CANONICAL_SOURCE_BLE_HOGP_KEYBOARD
#define KEYBOARD_SOURCE_BLE_COMPOSITE CANONICAL_SOURCE_BLE_COMPOSITE_KEYBOARD
#define KEYBOARD_SOURCE_SYNTHETIC CANONICAL_SOURCE_SYNTHETIC_REMAP

typedef struct {
    uint8_t source;
    uint8_t modifiers;
    uint8_t keycodes[KEYBOARD_INPUT_KEYCODE_COUNT];
} keyboard_input_snapshot_t;

_Static_assert(sizeof(keyboard_input_snapshot_t) == 8u,
               "Keyboard transport snapshot must remain compact and transport-neutral");

#endif
