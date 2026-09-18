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
    canonical_source_t source,
    uint8_t modifiers,
    uint8_t k0,
    uint8_t k1,
    uint8_t k2,
    uint8_t k3,
    uint8_t k4,
    uint8_t k5) {
    keyboard_input_snapshot_t value = {
        .modifiers = modifiers,
        .keycodes = {k0, k1, k2, k3, k4, k5},
    };
    keyboard_input_snapshot_set_source(&value, source);
    return value;
}

static void assert_keys(
    const canonical_keyboard_report_t *report,
    const uint8_t expected[CANONICAL_KEYBOARD_KEYCODE_COUNT]) {
    assert(memcmp(report->keycodes, expected, CANONICAL_KEYBOARD_KEYCODE_COUNT) == 0);
}

static canonical_mouse_event_t mouse_button(
    canonical_source_t source,
    canonical_mouse_button_t button,
    bool pressed) {
    canonical_mouse_event_t event = {0};
    event.source = source;
    event.type = CANONICAL_MOUSE_EVENT_BUTTON;
    event.data.button.button = button;
    event.data.button.pressed = pressed;
    return event;
}

int main(void) {
    canonical_hid_state_t state;
    canonical_keyboard_report_t report;
    canonical_hid_output_state_t output;
    bool changed = false;
    canonical_hid_init(&state);

    assert(CANONICAL_SOURCE_CAPACITY == 16u);
    assert(CANONICAL_SOURCE_KIND_CLASSIC_KEYBOARD != CANONICAL_SOURCE_KIND_BLE_HOGP_KEYBOARD);
    assert(CANONICAL_SOURCE_KIND_BLE_HOGP_KEYBOARD != CANONICAL_SOURCE_KIND_BLE_HOGP_MOUSE);
    assert(CANONICAL_SOURCE_KIND_BLE_COMPOSITE_KEYBOARD != CANONICAL_SOURCE_KIND_BLE_COMPOSITE_MOUSE);
    assert(CANONICAL_SOURCE_KIND_SYNTHETIC_REMAP != CANONICAL_SOURCE_KIND_CLASSIC_KEYBOARD);

    const canonical_source_t classic = canonical_source_make(
        CANONICAL_SOURCE_KIND_CLASSIC_KEYBOARD, 0u);
    keyboard_input_snapshot_t in = snapshot(classic, 0u, HID_KEY_A, 0u, 0u, 0u, 0u, 0u);
    assert(canonical_hid_apply_keyboard_snapshot(&state, &in, &report, &changed));
    assert(changed && report.keycodes[0] == HID_KEY_A);
    in = snapshot(classic, 0u, 0u, 0u, 0u, 0u, 0u, 0u);
    assert(canonical_hid_apply_keyboard_snapshot(&state, &in, &report, &changed));
    assert(changed && report.keycodes[0] == 0u);

    /* Full snapshots preserve tap ordering while aggregate-equivalent reorder is idempotent. */
    in = snapshot(classic, 0u, HID_KEY_B, HID_KEY_A, 0u, 0u, 0u, 0u);
    assert(canonical_hid_apply_keyboard_snapshot(&state, &in, &report, &changed));
    assert(changed);
    const uint8_t ab[6] = {HID_KEY_A, HID_KEY_B, 0u, 0u, 0u, 0u};
    assert_keys(&report, ab);
    in = snapshot(classic, 0u, HID_KEY_A, HID_KEY_B, 0u, 0u, 0u, 0u);
    assert(canonical_hid_apply_keyboard_snapshot(&state, &in, &report, &changed));
    assert(!changed);

    /* Same kind, different instance is independent ownership. */
    canonical_hid_release_all(&state, &report);
    const canonical_source_t ble_keyboard_10 = canonical_source_make(
        CANONICAL_SOURCE_KIND_BLE_HOGP_KEYBOARD, 10u);
    const canonical_source_t ble_keyboard_11 = canonical_source_make(
        CANONICAL_SOURCE_KIND_BLE_HOGP_KEYBOARD, 11u);
    in = snapshot(ble_keyboard_10, 0u, HID_KEY_A, 0u, 0u, 0u, 0u, 0u);
    assert(canonical_hid_apply_keyboard_snapshot(&state, &in, &report, &changed));
    in = snapshot(ble_keyboard_11, 0u, HID_KEY_A, 0u, 0u, 0u, 0u, 0u);
    assert(canonical_hid_apply_keyboard_snapshot(&state, &in, &report, &changed));
    assert(!changed);
    assert(canonical_hid_release_source(&state, ble_keyboard_10, &report, &changed));
    assert(!changed && report.keycodes[0] == HID_KEY_A);
    assert(canonical_hid_release_source(&state, ble_keyboard_11, &report, &changed));
    assert(changed && report.keycodes[0] == 0u);

    /* Physical + Synthetic Escape remains down until the final owner releases. */
    const canonical_source_t synthetic = canonical_source_make(
        CANONICAL_SOURCE_KIND_SYNTHETIC_REMAP, 0u);
    in = snapshot(classic, 0u, HID_KEY_ESCAPE, 0u, 0u, 0u, 0u, 0u);
    assert(canonical_hid_apply_keyboard_snapshot(&state, &in, &report, &changed));
    assert(changed && report.keycodes[0] == HID_KEY_ESCAPE);
    in = snapshot(synthetic, 0u, HID_KEY_ESCAPE, 0u, 0u, 0u, 0u, 0u);
    assert(canonical_hid_apply_keyboard_snapshot(&state, &in, &report, &changed));
    assert(!changed);
    in = snapshot(classic, 0u, 0u, 0u, 0u, 0u, 0u, 0u);
    assert(canonical_hid_apply_keyboard_snapshot(&state, &in, &report, &changed));
    assert(!changed && report.keycodes[0] == HID_KEY_ESCAPE);
    in = snapshot(synthetic, 0u, 0u, 0u, 0u, 0u, 0u, 0u);
    assert(canonical_hid_apply_keyboard_snapshot(&state, &in, &report, &changed));
    assert(changed && report.keycodes[0] == 0u);

    /* Shared modifier ownership follows the same last-owner rule. */
    in = snapshot(classic, MOD_LEFT_SHIFT, 0u, 0u, 0u, 0u, 0u, 0u);
    assert(canonical_hid_apply_keyboard_snapshot(&state, &in, &report, &changed));
    in = snapshot(ble_keyboard_10, MOD_LEFT_SHIFT, 0u, 0u, 0u, 0u, 0u, 0u);
    assert(canonical_hid_apply_keyboard_snapshot(&state, &in, &report, &changed));
    assert(!changed);
    assert(canonical_hid_release_source(&state, classic, &report, &changed));
    assert(!changed && report.modifiers == MOD_LEFT_SHIFT);
    assert(canonical_hid_release_source(&state, ble_keyboard_10, &report, &changed));
    assert(changed && report.modifiers == 0u);

    /* Six distinct keys fit; the seventh enters deterministic HID ErrorRollOver. */
    in = snapshot(classic, 0u, HID_KEY_A, HID_KEY_B, HID_KEY_C, HID_KEY_D, HID_KEY_E, HID_KEY_F);
    assert(canonical_hid_apply_keyboard_snapshot(&state, &in, &report, &changed));
    const uint8_t abcdef[6] = {HID_KEY_A, HID_KEY_B, HID_KEY_C, HID_KEY_D, HID_KEY_E, HID_KEY_F};
    assert_keys(&report, abcdef);
    in = snapshot(synthetic, 0u, HID_KEY_G, 0u, 0u, 0u, 0u, 0u);
    assert(canonical_hid_apply_keyboard_snapshot(&state, &in, &report, &changed));
    const uint8_t rollover[6] = {1u, 1u, 1u, 1u, 1u, 1u};
    assert(changed);
    assert_keys(&report, rollover);
    assert(canonical_hid_release_source(&state, synthetic, &report, &changed));
    assert(changed);
    assert_keys(&report, abcdef);

    /* Persistent Mouse buttons have independent last-owner semantics too. */
    canonical_hid_release_all(&state, &report);
    const canonical_source_t mouse1 = canonical_source_make(
        CANONICAL_SOURCE_KIND_BLE_HOGP_MOUSE, 1u);
    const canonical_source_t mouse2 = canonical_source_make(
        CANONICAL_SOURCE_KIND_BLE_COMPOSITE_MOUSE, 9u);
    canonical_mouse_event_t mouse = mouse_button(mouse1, CANONICAL_MOUSE_BUTTON_LEFT, true);
    assert(canonical_hid_apply_mouse(&state, &mouse));
    assert(canonical_hid_apply_mouse(&state, &mouse));
    mouse = mouse_button(mouse2, CANONICAL_MOUSE_BUTTON_LEFT, true);
    assert(canonical_hid_apply_mouse(&state, &mouse));
    assert(canonical_hid_release_source(&state, mouse1, &report, &changed));
    canonical_hid_snapshot(&state, &output);
    assert((output.mouse_buttons & 0x01u) != 0u);
    assert(canonical_hid_release_source(&state, mouse2, &report, &changed));
    canonical_hid_snapshot(&state, &output);
    assert((output.mouse_buttons & 0x01u) == 0u);

    /* Relative motion/wheel is transient and consumed only in accepted chunks. */
    memset(&mouse, 0, sizeof(mouse));
    mouse.source = mouse1;
    mouse.type = CANONICAL_MOUSE_EVENT_MOVE;
    mouse.data.move.dx = 12;
    mouse.data.move.dy = -3;
    assert(canonical_hid_apply_mouse(&state, &mouse));
    mouse.source = mouse2;
    mouse.data.move.dx = -2;
    mouse.data.move.dy = 8;
    assert(canonical_hid_apply_mouse(&state, &mouse));
    mouse.source = mouse1;
    mouse.type = CANONICAL_MOUSE_EVENT_WHEEL;
    mouse.data.wheel.vertical = 2;
    mouse.data.wheel.horizontal = -1;
    assert(canonical_hid_apply_mouse(&state, &mouse));
    canonical_hid_snapshot(&state, &output);
    assert(output.dx == 10 && output.dy == 5);
    assert(output.wheel_vertical == 2 && output.wheel_horizontal == -1);
    assert(!canonical_hid_consume_relative(&state, 11, 0, 0, 0));
    assert(canonical_hid_consume_relative(&state, 7, 5, 2, -1));
    canonical_hid_snapshot(&state, &output);
    assert(output.dx == 3 && output.dy == 0);
    assert(output.wheel_vertical == 0 && output.wheel_horizontal == 0);

    /* Sixteen simultaneously held source instances fit; the seventeenth fails
     * until one slot is released and can be reused. */
    canonical_hid_init(&state);
    for (uint16_t instance = 0u; instance < CANONICAL_SOURCE_CAPACITY; ++instance) {
        const canonical_source_t source = canonical_source_make(
            CANONICAL_SOURCE_KIND_BLE_HOGP_KEYBOARD, instance);
        in = snapshot(source, 0u, HID_KEY_A, 0u, 0u, 0u, 0u, 0u);
        assert(canonical_hid_apply_keyboard_snapshot(&state, &in, &report, &changed));
    }
    const canonical_source_t overflow_source = canonical_source_make(
        CANONICAL_SOURCE_KIND_BLE_HOGP_KEYBOARD, CANONICAL_SOURCE_CAPACITY);
    in = snapshot(overflow_source, 0u, HID_KEY_B, 0u, 0u, 0u, 0u, 0u);
    assert(!canonical_hid_apply_keyboard_snapshot(&state, &in, &report, &changed));
    const canonical_source_t released = canonical_source_make(
        CANONICAL_SOURCE_KIND_BLE_HOGP_KEYBOARD, 3u);
    assert(canonical_hid_release_source(&state, released, &report, &changed));
    assert(canonical_hid_apply_keyboard_snapshot(&state, &in, &report, &changed));

    /* Mouse-class input cannot masquerade as a Keyboard snapshot. */
    in = snapshot(mouse1, 0u, HID_KEY_C, 0u, 0u, 0u, 0u, 0u);
    assert(!canonical_hid_apply_keyboard_snapshot(&state, &in, &report, &changed));

    return 0;
}
