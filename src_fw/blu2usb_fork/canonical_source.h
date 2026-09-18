#ifndef BLU2USB_FORK_CANONICAL_SOURCE_H
#define BLU2USB_FORK_CANONICAL_SOURCE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    CANONICAL_SOURCE_INVALID = 0u,
    CANONICAL_SOURCE_CLASSIC_KEYBOARD = 1u,
    CANONICAL_SOURCE_BLE_HOGP_KEYBOARD = 2u,
    CANONICAL_SOURCE_BLE_HOGP_MOUSE = 3u,
    CANONICAL_SOURCE_BLE_COMPOSITE_KEYBOARD = 4u,
    CANONICAL_SOURCE_BLE_COMPOSITE_MOUSE = 5u,
    CANONICAL_SOURCE_SYNTHETIC_REMAP = 6u,
    CANONICAL_SOURCE_MAX_ID = CANONICAL_SOURCE_SYNTHETIC_REMAP,
} canonical_source_id_t;

static inline bool canonical_source_is_keyboard(uint8_t source) {
    return source == CANONICAL_SOURCE_CLASSIC_KEYBOARD ||
           source == CANONICAL_SOURCE_BLE_HOGP_KEYBOARD ||
           source == CANONICAL_SOURCE_BLE_COMPOSITE_KEYBOARD ||
           source == CANONICAL_SOURCE_SYNTHETIC_REMAP;
}

static inline bool canonical_source_is_mouse(uint8_t source) {
    return source == CANONICAL_SOURCE_BLE_HOGP_MOUSE ||
           source == CANONICAL_SOURCE_BLE_COMPOSITE_MOUSE;
}

#endif
