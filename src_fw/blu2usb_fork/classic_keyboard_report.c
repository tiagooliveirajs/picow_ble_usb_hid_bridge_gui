#include "classic_keyboard_report.h"

#include <string.h>

#include "btstack.h"

static bool add_key_usage(uint8_t keys[KEYBOARD_INPUT_KEYCODE_COUNT],
                          uint8_t *key_count,
                          uint16_t usage) {
    if (usage == 0u) return true;

    for (uint8_t i = 0u; i < *key_count; ++i) {
        if (keys[i] == (uint8_t)usage) return true;
    }

    if (*key_count >= KEYBOARD_INPUT_KEYCODE_COUNT || usage > 0x00ffu) {
        return false;
    }

    keys[*key_count] = (uint8_t)usage;
    ++(*key_count);
    return true;
}

bool classic_keyboard_parse_report(
    const uint8_t *descriptor,
    uint16_t descriptor_len,
    const uint8_t *hid_transaction_report,
    uint16_t report_len,
    keyboard_input_snapshot_t *snapshot) {
    if (descriptor == NULL || descriptor_len == 0u ||
        hid_transaction_report == NULL || report_len < 2u ||
        snapshot == NULL || hid_transaction_report[0] != 0xa1u) {
        return false;
    }

    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->source = KEYBOARD_SOURCE_CLASSIC_HID;

    uint8_t key_count = 0u;
    bool saw_keyboard_page = false;
    bool rollover = false;

    btstack_hid_parser_t parser;
    btstack_hid_parser_init(
        &parser,
        descriptor,
        descriptor_len,
        HID_REPORT_TYPE_INPUT,
        &hid_transaction_report[1],
        (uint16_t)(report_len - 1u));

    while (btstack_hid_parser_has_more(&parser)) {
        uint16_t usage_page;
        uint16_t usage;
        int32_t value;
        btstack_hid_parser_get_field(&parser, &usage_page, &usage, &value);

        if (usage_page != 0x0007u) continue;
        saw_keyboard_page = true;
        if (value == 0) continue;

        if (usage >= 0x00e0u && usage <= 0x00e7u) {
            snapshot->modifiers |= (uint8_t)(1u << (usage - 0x00e0u));
            continue;
        }

        if (!add_key_usage(snapshot->keycodes, &key_count, usage)) {
            rollover = true;
        }
    }

    if (!saw_keyboard_page) return false;

    if (rollover) {
        for (uint8_t i = 0u; i < KEYBOARD_INPUT_KEYCODE_COUNT; ++i) {
            snapshot->keycodes[i] = 0x01u;
        }
    }

    return true;
}
