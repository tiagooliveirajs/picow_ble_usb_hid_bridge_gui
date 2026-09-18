#include "ble_mouse_parser.h"

#include <limits.h>
#include <string.h>

#define HID_USAGE_PAGE_GENERIC_DESKTOP 0x01u
#define HID_USAGE_PAGE_BUTTON 0x09u
#define HID_USAGE_PAGE_CONSUMER 0x0cu
#define HID_USAGE_MOUSE 0x02u
#define HID_USAGE_X 0x30u
#define HID_USAGE_Y 0x31u
#define HID_USAGE_WHEEL 0x38u
#define HID_USAGE_AC_PAN 0x0238u
#define HID_COLLECTION_APPLICATION 0x01u
#define HID_MAIN_INPUT 0x08u
#define HID_MAIN_COLLECTION 0x0au
#define HID_MAIN_END_COLLECTION 0x0cu
#define HID_GLOBAL_USAGE_PAGE 0x00u
#define HID_GLOBAL_LOGICAL_MINIMUM 0x01u
#define HID_GLOBAL_LOGICAL_MAXIMUM 0x02u
#define HID_GLOBAL_REPORT_SIZE 0x07u
#define HID_GLOBAL_REPORT_ID 0x08u
#define HID_GLOBAL_REPORT_COUNT 0x09u
#define HID_GLOBAL_PUSH 0x0au
#define HID_GLOBAL_POP 0x0bu
#define HID_LOCAL_USAGE 0x00u
#define HID_LOCAL_USAGE_MINIMUM 0x01u
#define HID_LOCAL_USAGE_MAXIMUM 0x02u
#define HID_INPUT_CONSTANT 0x01u
#define HID_INPUT_VARIABLE 0x02u
#define HID_INPUT_RELATIVE 0x04u
#define LOCAL_USAGE_CAPACITY 16u
#define GLOBAL_STACK_CAPACITY 4u
#define COLLECTION_STACK_CAPACITY 8u

typedef struct {
    uint16_t usage_page;
    int32_t logical_minimum;
    int32_t logical_maximum;
    uint8_t report_size;
    uint8_t report_count;
    uint8_t report_id;
} hid_globals_t;

typedef struct {
    uint32_t usages[LOCAL_USAGE_CAPACITY];
    size_t usage_count;
    uint32_t usage_minimum;
    uint32_t usage_maximum;
    bool has_usage_range;
} hid_locals_t;

static uint32_t item_unsigned(const uint8_t *data, size_t size) {
    uint32_t value = 0u;
    for (size_t index = 0u; index < size; ++index)
        value |= ((uint32_t)data[index]) << (8u * index);
    return value;
}

static int32_t item_signed(const uint8_t *data, size_t size) {
    uint32_t value = item_unsigned(data, size);
    if (size == 0u) return 0;
    const unsigned bits = (unsigned)(size * 8u);
    if (bits < 32u && (value & (UINT32_C(1) << (bits - 1u))) != 0u)
        value |= UINT32_MAX << bits;
    return (int32_t)value;
}

static uint32_t qualified_usage(uint16_t page, uint32_t raw, size_t item_size) {
    if (item_size == 4u && (raw >> 16u) != 0u) return raw;
    return ((uint32_t)page << 16u) | (raw & UINT32_C(0xffff));
}

static void locals_reset(hid_locals_t *locals) { memset(locals, 0, sizeof(*locals)); }

static uint32_t local_usage_at(const hid_locals_t *locals, size_t index, uint16_t default_page) {
    if (index < locals->usage_count) return locals->usages[index];
    if (locals->has_usage_range) {
        const uint32_t usage = locals->usage_minimum + (uint32_t)index;
        if (usage <= locals->usage_maximum) return usage;
    }
    return ((uint32_t)default_page << 16u);
}

static ble_mouse_report_state_t *find_report(ble_mouse_parser_t *parser,
                                              uint8_t report_id,
                                              bool create) {
    for (size_t i = 0u; i < parser->report_count; ++i)
        if (parser->reports[i].report_id == report_id) return &parser->reports[i];
    if (!create || parser->report_count >= BLE_MOUSE_MAX_REPORTS) return NULL;
    ble_mouse_report_state_t *report = &parser->reports[parser->report_count++];
    memset(report, 0, sizeof(*report));
    report->report_id = report_id;
    return report;
}

static const ble_mouse_report_state_t *find_report_const(const ble_mouse_parser_t *parser,
                                                          uint8_t report_id) {
    if (parser == NULL) return NULL;
    for (size_t i = 0u; i < parser->report_count; ++i)
        if (parser->reports[i].report_id == report_id) return &parser->reports[i];
    return NULL;
}

static bool add_field(ble_mouse_parser_t *parser,
                      uint8_t report_id,
                      ble_mouse_field_kind_t kind,
                      uint16_t bit_offset,
                      uint8_t bit_size,
                      uint8_t button_index,
                      bool signed_value) {
    if (parser->field_count >= BLE_MOUSE_MAX_FIELDS || bit_size == 0u || bit_size > 32u)
        return false;
    ble_mouse_field_t *field = &parser->fields[parser->field_count++];
    field->report_id = report_id;
    field->kind = kind;
    field->bit_offset = bit_offset;
    field->bit_size = bit_size;
    field->button_index = button_index;
    field->signed_value = signed_value;
    return true;
}

static bool classify_and_add(ble_mouse_parser_t *parser,
                             const hid_globals_t *globals,
                             uint32_t usage,
                             uint16_t bit_offset,
                             uint32_t input_flags) {
    const uint16_t page = (uint16_t)(usage >> 16u);
    const uint16_t code = (uint16_t)usage;
    const bool relative = (input_flags & HID_INPUT_RELATIVE) != 0u;

    if (page == HID_USAGE_PAGE_BUTTON && code >= 1u && code <= CANONICAL_MOUSE_BUTTON_COUNT) {
        return add_field(parser, globals->report_id, BLE_MOUSE_FIELD_BUTTON,
                         bit_offset, globals->report_size, (uint8_t)(code - 1u), false);
    }
    if (!relative) return true;
    if (page == HID_USAGE_PAGE_GENERIC_DESKTOP) {
        if (code == HID_USAGE_X)
            return add_field(parser, globals->report_id, BLE_MOUSE_FIELD_X,
                             bit_offset, globals->report_size, 0u, globals->logical_minimum < 0);
        if (code == HID_USAGE_Y)
            return add_field(parser, globals->report_id, BLE_MOUSE_FIELD_Y,
                             bit_offset, globals->report_size, 0u, globals->logical_minimum < 0);
        if (code == HID_USAGE_WHEEL)
            return add_field(parser, globals->report_id, BLE_MOUSE_FIELD_WHEEL,
                             bit_offset, globals->report_size, 0u, globals->logical_minimum < 0);
    }
    if (page == HID_USAGE_PAGE_CONSUMER && code == HID_USAGE_AC_PAN)
        return add_field(parser, globals->report_id, BLE_MOUSE_FIELD_PAN,
                         bit_offset, globals->report_size, 0u, globals->logical_minimum < 0);
    return true;
}

bool ble_mouse_parser_configure(ble_mouse_parser_t *parser,
                                canonical_source_t source,
                                const uint8_t *descriptor,
                                size_t descriptor_len) {
    if (parser == NULL || descriptor == NULL || descriptor_len == 0u ||
        !canonical_source_is_valid(source) || !canonical_source_kind_is_mouse(source.kind))
        return false;

    memset(parser, 0, sizeof(*parser));
    parser->source = source;
    hid_globals_t globals = {0};
    hid_globals_t global_stack[GLOBAL_STACK_CAPACITY];
    size_t global_depth = 0u;
    hid_locals_t locals = {0};
    bool mouse_collection_stack[COLLECTION_STACK_CAPACITY] = {false};
    size_t collection_depth = 0u;
    bool in_mouse_collection = false;

    size_t position = 0u;
    while (position < descriptor_len) {
        const uint8_t prefix = descriptor[position++];
        if (prefix == 0xfeu) {
            if (position + 2u > descriptor_len) return false;
            const size_t long_size = descriptor[position];
            position += 2u;
            if (position + long_size > descriptor_len) return false;
            position += long_size;
            continue;
        }

        size_t item_size = (size_t)(prefix & 0x03u);
        if (item_size == 3u) item_size = 4u;
        const uint8_t item_type = (uint8_t)((prefix >> 2u) & 0x03u);
        const uint8_t item_tag = (uint8_t)((prefix >> 4u) & 0x0fu);
        if (position + item_size > descriptor_len) return false;
        const uint8_t *item_data = &descriptor[position];
        const uint32_t unsigned_value = item_unsigned(item_data, item_size);
        position += item_size;

        if (item_type == 1u) {
            switch (item_tag) {
            case HID_GLOBAL_USAGE_PAGE: globals.usage_page = (uint16_t)unsigned_value; break;
            case HID_GLOBAL_LOGICAL_MINIMUM: globals.logical_minimum = item_signed(item_data, item_size); break;
            case HID_GLOBAL_LOGICAL_MAXIMUM: globals.logical_maximum = item_signed(item_data, item_size); break;
            case HID_GLOBAL_REPORT_SIZE:
                if (unsigned_value > 32u) return false;
                globals.report_size = (uint8_t)unsigned_value;
                break;
            case HID_GLOBAL_REPORT_ID:
                if (unsigned_value == 0u || unsigned_value > 255u) return false;
                globals.report_id = (uint8_t)unsigned_value;
                break;
            case HID_GLOBAL_REPORT_COUNT:
                if (unsigned_value > 255u) return false;
                globals.report_count = (uint8_t)unsigned_value;
                break;
            case HID_GLOBAL_PUSH:
                if (global_depth >= GLOBAL_STACK_CAPACITY) return false;
                global_stack[global_depth++] = globals;
                break;
            case HID_GLOBAL_POP:
                if (global_depth == 0u) return false;
                globals = global_stack[--global_depth];
                break;
            default: break;
            }
            continue;
        }

        if (item_type == 2u) {
            const uint32_t usage = qualified_usage(globals.usage_page, unsigned_value, item_size);
            switch (item_tag) {
            case HID_LOCAL_USAGE:
                if (locals.usage_count < LOCAL_USAGE_CAPACITY) locals.usages[locals.usage_count++] = usage;
                break;
            case HID_LOCAL_USAGE_MINIMUM:
                locals.usage_minimum = usage; locals.has_usage_range = true; break;
            case HID_LOCAL_USAGE_MAXIMUM:
                locals.usage_maximum = usage; locals.has_usage_range = true; break;
            default: break;
            }
            continue;
        }
        if (item_type != 0u) continue;

        if (item_tag == HID_MAIN_COLLECTION) {
            const uint32_t usage = local_usage_at(&locals, 0u, globals.usage_page);
            const bool mouse_application = unsigned_value == HID_COLLECTION_APPLICATION &&
                (uint16_t)(usage >> 16u) == HID_USAGE_PAGE_GENERIC_DESKTOP &&
                (uint16_t)usage == HID_USAGE_MOUSE;
            if (collection_depth >= COLLECTION_STACK_CAPACITY) return false;
            mouse_collection_stack[collection_depth++] = in_mouse_collection || mouse_application;
            in_mouse_collection = mouse_collection_stack[collection_depth - 1u];
            locals_reset(&locals);
            continue;
        }
        if (item_tag == HID_MAIN_END_COLLECTION) {
            if (collection_depth == 0u) return false;
            --collection_depth;
            in_mouse_collection = collection_depth > 0u ? mouse_collection_stack[collection_depth - 1u] : false;
            locals_reset(&locals);
            continue;
        }
        if (item_tag == HID_MAIN_INPUT) {
            ble_mouse_report_state_t *report = find_report(parser, globals.report_id, true);
            if (report == NULL) return false;
            const uint32_t input_bits = (uint32_t)globals.report_size * (uint32_t)globals.report_count;
            if ((uint32_t)report->input_bits + input_bits > UINT16_MAX) return false;
            const uint16_t base_offset = report->input_bits;
            if (in_mouse_collection && (unsigned_value & HID_INPUT_CONSTANT) == 0u &&
                (unsigned_value & HID_INPUT_VARIABLE) != 0u) {
                for (uint32_t index = 0u; index < globals.report_count; ++index) {
                    const uint32_t field_offset = (uint32_t)base_offset + index * (uint32_t)globals.report_size;
                    if (field_offset > UINT16_MAX) return false;
                    const uint32_t usage = local_usage_at(&locals, (size_t)index, globals.usage_page);
                    if (!classify_and_add(parser, &globals, usage, (uint16_t)field_offset, unsigned_value))
                        return false;
                }
            }
            report->input_bits = (uint16_t)((uint32_t)report->input_bits + input_bits);
            locals_reset(&locals);
            continue;
        }
        locals_reset(&locals);
    }

    parser->configured = parser->field_count > 0u;
    return parser->configured;
}

bool ble_mouse_parser_has_mouse(const ble_mouse_parser_t *parser) {
    return parser != NULL && parser->configured && parser->field_count > 0u;
}

bool ble_mouse_parser_normalize_report(const ble_mouse_parser_t *parser,
                                       uint8_t report_id,
                                       const uint8_t *report,
                                       size_t report_len,
                                       const uint8_t **payload,
                                       size_t *payload_len) {
    if (parser == NULL || report == NULL || payload == NULL || payload_len == NULL) return false;
    const ble_mouse_report_state_t *state = find_report_const(parser, report_id);
    if (state == NULL) return false;
    const size_t expected_len = ((size_t)state->input_bits + 7u) / 8u;
    if (report_len == expected_len) {
        *payload = report; *payload_len = report_len; return true;
    }
    if (report_len == expected_len + 1u && report_len > 0u && report[0] == report_id) {
        *payload = report + 1u; *payload_len = report_len - 1u; return true;
    }
    return false;
}

static bool extract_value(const uint8_t *report, size_t report_len,
                          uint16_t bit_offset, uint8_t bit_size,
                          bool signed_value, int32_t *out_value) {
    if (report == NULL || out_value == NULL || bit_size == 0u || bit_size > 32u ||
        (size_t)bit_offset + (size_t)bit_size > report_len * 8u) return false;
    uint32_t raw = 0u;
    for (uint8_t index = 0u; index < bit_size; ++index) {
        const size_t bit = (size_t)bit_offset + index;
        if ((report[bit >> 3u] & (uint8_t)(1u << (bit & 7u))) != 0u)
            raw |= UINT32_C(1) << index;
    }
    if (signed_value && bit_size < 32u && (raw & (UINT32_C(1) << (bit_size - 1u))) != 0u)
        raw |= UINT32_MAX << bit_size;
    *out_value = (int32_t)raw;
    return true;
}

static int16_t clamp_i16(int32_t value) {
    if (value > INT16_MAX) return INT16_MAX;
    if (value < INT16_MIN) return INT16_MIN;
    return (int16_t)value;
}

static bool emit_button_deltas(ble_mouse_parser_t *parser, uint8_t old_mask,
                               uint8_t new_mask, ble_mouse_emit_fn emit,
                               void *context) {
    const uint8_t changed = (uint8_t)(old_mask ^ new_mask);
    for (uint8_t button = 0u; button < CANONICAL_MOUSE_BUTTON_COUNT; ++button) {
        const uint8_t mask = (uint8_t)(1u << button);
        if ((changed & mask) == 0u) continue;
        canonical_mouse_event_t event = {0};
        event.source = parser->source;
        event.type = CANONICAL_MOUSE_EVENT_BUTTON;
        event.data.button.button = (canonical_mouse_button_t)button;
        event.data.button.pressed = (new_mask & mask) != 0u;
        if (!emit(context, &event)) return false;
    }
    return true;
}

static bool parse_payload(ble_mouse_parser_t *parser, uint8_t report_id,
                          const uint8_t *report, size_t report_len,
                          ble_mouse_emit_fn emit, void *context) {
    ble_mouse_report_state_t *report_state = find_report(parser, report_id, false);
    if (report_state == NULL) return false;
    uint8_t candidate_buttons = report_state->button_mask;
    bool has_button_fields = false;
    int32_t dx = 0, dy = 0, wheel = 0, pan = 0;

    for (size_t index = 0u; index < parser->field_count; ++index) {
        const ble_mouse_field_t *field = &parser->fields[index];
        if (field->report_id != report_id) continue;
        int32_t value = 0;
        if (!extract_value(report, report_len, field->bit_offset, field->bit_size,
                           field->signed_value, &value)) return false;
        switch (field->kind) {
        case BLE_MOUSE_FIELD_BUTTON: {
            has_button_fields = true;
            const uint8_t mask = (uint8_t)(1u << field->button_index);
            candidate_buttons = value != 0 ? (uint8_t)(candidate_buttons | mask)
                                           : (uint8_t)(candidate_buttons & (uint8_t)~mask);
            break;
        }
        case BLE_MOUSE_FIELD_X: dx += value; break;
        case BLE_MOUSE_FIELD_Y: dy += value; break;
        case BLE_MOUSE_FIELD_WHEEL: wheel += value; break;
        case BLE_MOUSE_FIELD_PAN: pan += value; break;
        }
    }

    if (has_button_fields) {
        uint8_t new_aggregate = 0u;
        for (size_t i = 0u; i < parser->report_count; ++i)
            new_aggregate = (uint8_t)(new_aggregate |
                (parser->reports[i].report_id == report_id ? candidate_buttons : parser->reports[i].button_mask));
        if (!emit_button_deltas(parser, parser->aggregate_buttons, new_aggregate, emit, context)) return false;
        report_state->button_mask = candidate_buttons;
        parser->aggregate_buttons = new_aggregate;
    }

    if (dx != 0 || dy != 0) {
        canonical_mouse_event_t event = {0};
        event.source = parser->source;
        event.type = CANONICAL_MOUSE_EVENT_MOVE;
        event.data.move.dx = clamp_i16(dx);
        event.data.move.dy = clamp_i16(dy);
        if (!emit(context, &event)) return false;
    }
    if (wheel != 0 || pan != 0) {
        canonical_mouse_event_t event = {0};
        event.source = parser->source;
        event.type = CANONICAL_MOUSE_EVENT_WHEEL;
        event.data.wheel.vertical = clamp_i16(wheel);
        event.data.wheel.horizontal = clamp_i16(pan);
        if (!emit(context, &event)) return false;
    }
    return true;
}

bool ble_mouse_parser_parse_report(ble_mouse_parser_t *parser,
                                   uint8_t report_id,
                                   const uint8_t *report,
                                   size_t report_len,
                                   ble_mouse_emit_fn emit,
                                   void *context) {
    if (parser == NULL || !parser->configured || report == NULL || emit == NULL) return false;
    const uint8_t *payload = NULL;
    size_t payload_len = 0u;
    if (!ble_mouse_parser_normalize_report(parser, report_id, report, report_len,
                                           &payload, &payload_len)) return false;
    return parse_payload(parser, report_id, payload, payload_len, emit, context);
}
