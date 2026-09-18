#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "../canonical_hid.h"

#define HID_KEY_A 0x04u
#define HID_KEY_B 0x05u
#define HID_KEY_C 0x06u
#define HID_KEY_D 0x07u
#define HID_KEY_E 0x08u
#define HID_KEY_F 0x09u
#define HID_KEY_G 0x0au
#define HID_KEY_ESCAPE 0x29u
#define MOD_LEFT_SHIFT 0x02u

static keyboard_input_snapshot_t snapshot(
    uint8_t source,
    uint8_t modifiers,
    uint8_t k0,
    uint8_t k1,
    uint8_t k2,
    uint8_t k3,
    uint8_t k4,
    uint8_t k5) {
    keyboard_input_snapshot_t value = {
        .source = source,
        .modifiers = modifiers,
        .keycodes = {k0, k1, k2, k3, k4, k5},
    };
    return value;
}

static void assert_keys(
    const canonical_keyboard_report_t *report,
    const uint8_t expected[CANONICAL_KEYBOARD_KEYCODE_COUNT]) {
    assert(memcmp(report->keycodes, expected, CANONICAL_KEYBOARD_KEYCODE_COUNT) == 0);
}

int main(void) {
    canonical_hid_state_t state;
    canonical_keyboard_report_t report;
    bool changed = false;
    canonical_hid_init(&state);

    assert(CANONICAL_SOURCE_CLASSIC_KEYBOARD != CANONICAL_SOURCE_BLE_HOGP_KEYBOARD);
    assert(CANONICAL_SOURCE_BLE_HOGP_KEYBOARD != CANONICAL_SOURCE_BLE_HOGP_MOUSE);
    assert(CANONICAL_SOURCE_BLE_COMPOSITE_KEYBOARD != CANONICAL_SOURCE_BLE_COMPOSITE_MOUSE);
    assert(CANONICAL_SOURCE_SYNTHETIC_REMAP != CANONICAL_SOURCE_CLASSIC_KEYBOARD);

    /* Ordered full snapshots preserve a short physical tap. */
    keyboard_input_snapshot_t in = snapshot(
        CANONICAL_SOURCE_CLASSIC_KEYBOARD, 0u, HID_KEY_A, 0u, 0u, 0u, 0u, 0u);
    assert(canonical_hid_apply_keyboard_snapshot(&state, &in, &report, &changed));
    assert(changed && report.keycodes[0] == HID_KEY_A);
    in = snapshot(CANONICAL_SOURCE_CLASSIC_KEYBOARD, 0u, 0u, 0u, 0u, 0u, 0u, 0u);
    assert(canonical_hid_apply_keyboard_snapshot(&state, &in, &report, &changed));
    assert(changed && report.keycodes[0] == 0u);

    /* Duplicate/reordered snapshots that produce the same aggregate are idempotent. */
    in = snapshot(CANONICAL_SOURCE_CLASSIC_KEYBOARD, 0u, HID_KEY_B, HID_KEY_A, 0u, 0u, 0u, 0u);
    assert(canonical_hid_apply_keyboard_snapshot(&state, &in, &report, &changed));
    assert(changed);
    const uint8_t ab[6] = {HID_KEY_A, HID_KEY_B, 0u, 0u, 0u, 0u};
    assert_keys(&report, ab);
    in = snapshot(CANONICAL_SOURCE_CLASSIC_KEYBOARD, 0u, HID_KEY_A, HID_KEY_B, 0u, 0u, 0u, 0u);
    assert(canonical_hid_apply_keyboard_snapshot(&state, &in, &report, &changed));
    assert(!changed);

    /* Physical + Synthetic Escape is one held target until the last owner releases. */
    canonical_hid_release_all(&state, &report);
    in = snapshot(CANONICAL_SOURCE_CLASSIC_KEYBOARD, 0u, HID_KEY_ESCAPE, 0u, 0u, 0u, 0u, 0u);
    assert(canonical_hid_apply_keyboard_snapshot(&state, &in, &report, &changed));
    assert(changed && report.keycodes[0] == HID_KEY_ESCAPE);
    in = snapshot(CANONICAL_SOURCE_SYNTHETIC_REMAP, 0u, HID_KEY_ESCAPE, 0u, 0u, 0u, 0u, 0u);
    assert(canonical_hid_apply_keyboard_snapshot(&state, &in, &report, &changed));
    assert(!changed && report.keycodes[0] == HID_KEY_ESCAPE);
    in = snapshot(CANONICAL_SOURCE_CLASSIC_KEYBOARD, 0u, 0u, 0u, 0u, 0u, 0u, 0u);
    assert(canonical_hid_apply_keyboard_snapshot(&state, &in, &report, &changed));
    assert(!changed && report.keycodes[0] == HID_KEY_ESCAPE);
    in = snapshot(CANONICAL_SOURCE_SYNTHETIC_REMAP, 0u, 0u, 0u, 0u, 0u, 0u, 0u);
    assert(canonical_hid_apply_keyboard_snapshot(&state, &in, &report, &changed));
    assert(changed && report.keycodes[0] == 0u);

    /* Shared modifier ownership behaves the same way. */
    in = snapshot(CANONICAL_SOURCE_CLASSIC_KEYBOARD, MOD_LEFT_SHIFT, 0u, 0u, 0u, 0u, 0u, 0u);
    assert(canonical_hid_apply_keyboard_snapshot(&state, &in, &report, &changed));
    assert(changed && report.modifiers == MOD_LEFT_SHIFT);
    in = snapshot(CANONICAL_SOURCE_BLE_HOGP_KEYBOARD, MOD_LEFT_SHIFT, 0u, 0u, 0u, 0u, 0u, 0u);
    assert(canonical_hid_apply_keyboard_snapshot(&state, &in, &report, &changed));
    assert(!changed);
    assert(canonical_hid_release_source(
        &state, CANONICAL_SOURCE_CLASSIC_KEYBOARD, &report, &changed));
    assert(!changed && report.modifiers == MOD_LEFT_SHIFT);
    assert(canonical_hid_release_source(
        &state, CANONICAL_SOURCE_BLE_HOGP_KEYBOARD, &report, &changed));
    assert(changed && report.modifiers == 0u);

    /* Six distinct keys are representable; the seventh deterministically enters 6KRO error rollover. */
    canonical_hid_release_all(&state, &report);
    in = snapshot(CANONICAL_SOURCE_CLASSIC_KEYBOARD, 0u,
                  HID_KEY_A, HID_KEY_B, HID_KEY_C, HID_KEY_D, HID_KEY_E, HID_KEY_F);
    assert(canonical_hid_apply_keyboard_snapshot(&state, &in, &report, &changed));
    const uint8_t abcdef[6] = {HID_KEY_A, HID_KEY_B, HID_KEY_C, HID_KEY_D, HID_KEY_E, HID_KEY_F};
    assert_keys(&report, abcdef);
    in = snapshot(CANONICAL_SOURCE_SYNTHETIC_REMAP, 0u,
                  HID_KEY_G, 0u, 0u, 0u, 0u, 0u);
    assert(canonical_hid_apply_keyboard_snapshot(&state, &in, &report, &changed));
    assert(changed);
    const uint8_t rollover[6] = {1u, 1u, 1u, 1u, 1u, 1u};
    assert_keys(&report, rollover);
    assert(canonical_hid_release_source(
        &state, CANONICAL_SOURCE_SYNTHETIC_REMAP, &report, &changed));
    assert(changed);
    assert_keys(&report, abcdef);

    /* Source teardown releases only that source; another source remains held. */
    canonical_hid_release_all(&state, &report);
    in = snapshot(CANONICAL_SOURCE_CLASSIC_KEYBOARD, 0u, HID_KEY_A, 0u, 0u, 0u, 0u, 0u);
    assert(canonical_hid_apply_keyboard_snapshot(&state, &in, &report, &changed));
    in = snapshot(CANONICAL_SOURCE_BLE_COMPOSITE_KEYBOARD, 0u, HID_KEY_B, 0u, 0u, 0u, 0u, 0u);
    assert(canonical_hid_apply_keyboard_snapshot(&state, &in, &report, &changed));
    assert(canonical_hid_release_source(
        &state, CANONICAL_SOURCE_CLASSIC_KEYBOARD, &report, &changed));
    assert(changed && report.keycodes[0] == HID_KEY_B && report.keycodes[1] == 0u);

    /* A Mouse source cannot alias or mutate Keyboard ownership. */
    in = snapshot(CANONICAL_SOURCE_BLE_HOGP_MOUSE, 0u, HID_KEY_C, 0u, 0u, 0u, 0u, 0u);
    assert(!canonical_hid_apply_keyboard_snapshot(&state, &in, &report, &changed));
    assert(!changed && report.keycodes[0] == HID_KEY_B);
    assert(canonical_hid_release_source(
        &state, CANONICAL_SOURCE_BLE_HOGP_MOUSE, &report, &changed));
    assert(!changed && report.keycodes[0] == HID_KEY_B);

    return 0;
}
