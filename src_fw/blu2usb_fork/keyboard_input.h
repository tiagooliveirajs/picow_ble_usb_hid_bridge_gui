#ifndef BLU2USB_FORK_KEYBOARD_INPUT_H
#define BLU2USB_FORK_KEYBOARD_INPUT_H

#include <stddef.h>
#include <stdint.h>

#include "canonical_source.h"

#define KEYBOARD_INPUT_KEYCODE_COUNT 6u

/* Keep the accepted Classic adapter's scalar `source` assignment compatible
 * while extending stable identity to kind + instance. The first four bytes are
 * the canonical input-source prefix required by bridge_bus overflow recovery. */
#define KEYBOARD_SOURCE_CLASSIC_HID CANONICAL_SOURCE_KIND_CLASSIC_KEYBOARD
#define KEYBOARD_SOURCE_BLE_HOGP CANONICAL_SOURCE_KIND_BLE_HOGP_KEYBOARD
#define KEYBOARD_SOURCE_BLE_COMPOSITE CANONICAL_SOURCE_KIND_BLE_COMPOSITE_KEYBOARD
#define KEYBOARD_SOURCE_SYNTHETIC CANONICAL_SOURCE_KIND_SYNTHETIC_REMAP

typedef struct {
    uint8_t source;
    uint8_t source_reserved;
    uint8_t source_instance_lo;
    uint8_t source_instance_hi;
    uint8_t modifiers;
    uint8_t keycodes[KEYBOARD_INPUT_KEYCODE_COUNT];
} keyboard_input_snapshot_t;

_Static_assert(offsetof(keyboard_input_snapshot_t, source) == 0u,
               "canonical source kind must start the input payload");
_Static_assert(offsetof(keyboard_input_snapshot_t, source_instance_lo) == 2u,
               "canonical source instance must be in the common input prefix");
_Static_assert(sizeof(keyboard_input_snapshot_t) == 11u,
               "Keyboard transport snapshot wire layout changed unexpectedly");

static inline canonical_source_t keyboard_input_snapshot_source(
    const keyboard_input_snapshot_t *snapshot) {
    return canonical_source_from_input_prefix((const uint8_t *)snapshot);
}

static inline void keyboard_input_snapshot_set_source(
    keyboard_input_snapshot_t *snapshot,
    canonical_source_t source) {
    canonical_source_write_input_prefix((uint8_t *)snapshot, source);
}

#endif
