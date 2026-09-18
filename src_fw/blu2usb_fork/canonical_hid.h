#ifndef BLU2USB_FORK_CANONICAL_HID_H
#define BLU2USB_FORK_CANONICAL_HID_H

#include <stdbool.h>
#include <stdint.h>

#include "canonical_source.h"
#include "keyboard_input.h"

#define CANONICAL_KEYBOARD_KEYCODE_COUNT 6u
#define CANONICAL_KEYBOARD_ERROR_ROLLOVER 0x01u

typedef struct {
    uint8_t modifiers;
    uint8_t keycodes[CANONICAL_KEYBOARD_KEYCODE_COUNT];
} canonical_keyboard_report_t;

typedef struct {
    keyboard_input_snapshot_t keyboard_sources[CANONICAL_SOURCE_MAX_ID + 1u];
    canonical_keyboard_report_t keyboard_report;
} canonical_hid_state_t;

void canonical_hid_init(canonical_hid_state_t *state);

/* Apply one ordered full-state transport snapshot. Returns false only for an
 * invalid/non-Keyboard source. `changed` reports whether the aggregate USB-
 * representable Keyboard state changed after source ownership was merged. */
bool canonical_hid_apply_keyboard_snapshot(
    canonical_hid_state_t *state,
    const keyboard_input_snapshot_t *snapshot,
    canonical_keyboard_report_t *report,
    bool *changed);

/* Tear down exactly one canonical source. Mouse-only sources are accepted as
 * known IDs but do not alter Keyboard state in FORK-03. */
bool canonical_hid_release_source(
    canonical_hid_state_t *state,
    uint8_t source,
    canonical_keyboard_report_t *report,
    bool *changed);

void canonical_hid_release_all(
    canonical_hid_state_t *state,
    canonical_keyboard_report_t *report);

const canonical_keyboard_report_t *canonical_hid_keyboard_report(
    const canonical_hid_state_t *state);

#endif
