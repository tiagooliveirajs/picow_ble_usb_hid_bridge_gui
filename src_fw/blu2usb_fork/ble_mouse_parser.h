#ifndef BLU2USB_FORK_BLE_MOUSE_PARSER_H
#define BLU2USB_FORK_BLE_MOUSE_PARSER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "canonical_hid.h"

#define BLE_MOUSE_MAX_FIELDS 48u
#define BLE_MOUSE_MAX_REPORTS 16u

typedef enum {
    BLE_MOUSE_FIELD_BUTTON = 0,
    BLE_MOUSE_FIELD_X,
    BLE_MOUSE_FIELD_Y,
    BLE_MOUSE_FIELD_WHEEL,
    BLE_MOUSE_FIELD_PAN,
} ble_mouse_field_kind_t;

typedef struct {
    uint8_t report_id;
    ble_mouse_field_kind_t kind;
    uint16_t bit_offset;
    uint8_t bit_size;
    uint8_t button_index;
    bool signed_value;
} ble_mouse_field_t;

typedef struct {
    uint8_t report_id;
    uint16_t input_bits;
    uint8_t button_mask;
} ble_mouse_report_state_t;

typedef struct {
    canonical_source_t source;
    ble_mouse_field_t fields[BLE_MOUSE_MAX_FIELDS];
    size_t field_count;
    ble_mouse_report_state_t reports[BLE_MOUSE_MAX_REPORTS];
    size_t report_count;
    uint8_t aggregate_buttons;
    bool configured;
} ble_mouse_parser_t;

typedef bool (*ble_mouse_emit_fn)(void *context, const canonical_mouse_event_t *event);

bool ble_mouse_parser_configure(ble_mouse_parser_t *parser,
                                canonical_source_t source,
                                const uint8_t *descriptor,
                                size_t descriptor_len);
bool ble_mouse_parser_has_mouse(const ble_mouse_parser_t *parser);
bool ble_mouse_parser_normalize_report(const ble_mouse_parser_t *parser,
                                       uint8_t report_id,
                                       const uint8_t *report,
                                       size_t report_len,
                                       const uint8_t **payload,
                                       size_t *payload_len);
bool ble_mouse_parser_parse_report(ble_mouse_parser_t *parser,
                                   uint8_t report_id,
                                   const uint8_t *report,
                                   size_t report_len,
                                   ble_mouse_emit_fn emit,
                                   void *context);

#endif
