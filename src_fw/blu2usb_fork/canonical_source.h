#ifndef BLU2USB_FORK_CANONICAL_SOURCE_H
#define BLU2USB_FORK_CANONICAL_SOURCE_H

#include <stdbool.h>
#include <stdint.h>

/* G06 accepted up to sixteen simultaneously owned persistent HID sources.
 * Source identity itself is kind + instance, so two peers of the same class do
 * not alias each other. */
#define CANONICAL_SOURCE_CAPACITY 16u
#define CANONICAL_INPUT_SOURCE_PREFIX_SIZE 4u

typedef enum {
    CANONICAL_SOURCE_KIND_INVALID = 0u,
    CANONICAL_SOURCE_KIND_CLASSIC_KEYBOARD = 1u,
    CANONICAL_SOURCE_KIND_BLE_HOGP_KEYBOARD = 2u,
    CANONICAL_SOURCE_KIND_BLE_HOGP_MOUSE = 3u,
    CANONICAL_SOURCE_KIND_BLE_COMPOSITE_KEYBOARD = 4u,
    CANONICAL_SOURCE_KIND_BLE_COMPOSITE_MOUSE = 5u,
    CANONICAL_SOURCE_KIND_SYNTHETIC_REMAP = 6u,
    CANONICAL_SOURCE_KIND_COUNT,
} canonical_source_kind_t;

typedef struct {
    uint16_t kind;
    uint16_t instance;
} canonical_source_t;

static inline canonical_source_t canonical_source_make(
    canonical_source_kind_t kind,
    uint16_t instance) {
    const canonical_source_t source = {(uint16_t)kind, instance};
    return source;
}

static inline bool canonical_source_is_valid(canonical_source_t source) {
    return source.kind > (uint16_t)CANONICAL_SOURCE_KIND_INVALID &&
           source.kind < (uint16_t)CANONICAL_SOURCE_KIND_COUNT;
}

static inline bool canonical_source_equal(
    canonical_source_t a,
    canonical_source_t b) {
    return a.kind == b.kind && a.instance == b.instance;
}

static inline bool canonical_source_kind_is_keyboard(uint16_t kind) {
    return kind == (uint16_t)CANONICAL_SOURCE_KIND_CLASSIC_KEYBOARD ||
           kind == (uint16_t)CANONICAL_SOURCE_KIND_BLE_HOGP_KEYBOARD ||
           kind == (uint16_t)CANONICAL_SOURCE_KIND_BLE_COMPOSITE_KEYBOARD ||
           kind == (uint16_t)CANONICAL_SOURCE_KIND_SYNTHETIC_REMAP;
}

static inline bool canonical_source_kind_is_mouse(uint16_t kind) {
    return kind == (uint16_t)CANONICAL_SOURCE_KIND_BLE_HOGP_MOUSE ||
           kind == (uint16_t)CANONICAL_SOURCE_KIND_BLE_COMPOSITE_MOUSE ||
           kind == (uint16_t)CANONICAL_SOURCE_KIND_SYNTHETIC_REMAP;
}

/* Every release-sensitive canonical INPUT message begins with this compact
 * source prefix: kind byte, reserved byte, instance little-endian. This lets
 * the bounded bus preserve source identity even when publication fails. */
static inline canonical_source_t canonical_source_from_input_prefix(
    const uint8_t prefix[CANONICAL_INPUT_SOURCE_PREFIX_SIZE]) {
    canonical_source_t source = {
        .kind = prefix[0],
        .instance = (uint16_t)((uint16_t)prefix[2] |
                              ((uint16_t)prefix[3] << 8u)),
    };
    return source;
}

static inline void canonical_source_write_input_prefix(
    uint8_t prefix[CANONICAL_INPUT_SOURCE_PREFIX_SIZE],
    canonical_source_t source) {
    prefix[0] = (uint8_t)source.kind;
    prefix[1] = 0u;
    prefix[2] = (uint8_t)(source.instance & 0xffu);
    prefix[3] = (uint8_t)(source.instance >> 8u);
}

static inline uint32_t canonical_source_encode(canonical_source_t source) {
    return ((uint32_t)source.kind << 16u) | (uint32_t)source.instance;
}

static inline canonical_source_t canonical_source_decode(uint32_t encoded) {
    canonical_source_t source = {
        .kind = (uint16_t)(encoded >> 16u),
        .instance = (uint16_t)(encoded & UINT32_C(0xffff)),
    };
    return source;
}

#endif
