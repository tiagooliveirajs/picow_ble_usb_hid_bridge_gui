#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "classic_keyboard_report.h"

static const uint8_t keyboard_descriptor[] = {
    0x05, 0x01,       // Usage Page (Generic Desktop)
    0x09, 0x06,       // Usage (Keyboard)
    0xa1, 0x01,       // Collection (Application)
    0x85, 0x01,       // Report ID 1
    0x05, 0x07,       // Usage Page (Keyboard)
    0x19, 0xe0,       // Usage Minimum (Left Control)
    0x29, 0xe7,       // Usage Maximum (Right GUI)
    0x15, 0x00,       // Logical Minimum 0
    0x25, 0x01,       // Logical Maximum 1
    0x75, 0x01,       // Report Size 1
    0x95, 0x08,       // Report Count 8
    0x81, 0x02,       // Input (Data,Var,Abs)
    0x95, 0x01,       // Report Count 1
    0x75, 0x08,       // Report Size 8
    0x81, 0x01,       // Input (Constant)
    0x95, 0x06,       // Report Count 6
    0x75, 0x08,       // Report Size 8
    0x15, 0x00,       // Logical Minimum 0
    0x25, 0x65,       // Logical Maximum 101
    0x05, 0x07,       // Usage Page (Keyboard)
    0x19, 0x00,       // Usage Minimum 0
    0x29, 0x65,       // Usage Maximum 101
    0x81, 0x00,       // Input (Data,Array,Abs)
    0xc0              // End Collection
};

static void expect_report(const uint8_t report[10], uint8_t modifier, uint8_t key) {
    keyboard_input_snapshot_t snapshot;
    const bool ok = classic_keyboard_parse_report(
        keyboard_descriptor,
        sizeof(keyboard_descriptor),
        report,
        10u,
        &snapshot);
    assert(ok);
    assert(snapshot.source == KEYBOARD_SOURCE_CLASSIC_HID);
    assert(snapshot.modifiers == modifier);
    assert(snapshot.keycodes[0] == key);
    for (unsigned int i = 1u; i < KEYBOARD_INPUT_KEYCODE_COUNT; ++i) {
        assert(snapshot.keycodes[i] == 0u);
    }
}

int main(void) {
    const uint8_t a_down[10] = {0xa1, 0x01, 0x00, 0x00, 0x04, 0, 0, 0, 0, 0};
    const uint8_t s_down[10] = {0xa1, 0x01, 0x00, 0x00, 0x16, 0, 0, 0, 0, 0};
    const uint8_t d_down[10] = {0xa1, 0x01, 0x00, 0x00, 0x07, 0, 0, 0, 0, 0};
    const uint8_t release[10] = {0xa1, 0x01, 0x00, 0x00, 0x00, 0, 0, 0, 0, 0};
    const uint8_t shift_a[10] = {0xa1, 0x01, 0x02, 0x00, 0x04, 0, 0, 0, 0, 0};
    const uint8_t malformed[2] = {0x00, 0x01};

    expect_report(a_down, 0x00u, 0x04u);
    expect_report(s_down, 0x00u, 0x16u);
    expect_report(d_down, 0x00u, 0x07u);
    expect_report(release, 0x00u, 0x00u);
    expect_report(shift_a, 0x02u, 0x04u);

    keyboard_input_snapshot_t snapshot;
    assert(!classic_keyboard_parse_report(
        keyboard_descriptor,
        sizeof(keyboard_descriptor),
        malformed,
        sizeof(malformed),
        &snapshot));

    puts("PASS: accepted a/s/d+release fixtures, modifier parsing and Classic report framing");
    return 0;
}
