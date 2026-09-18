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
    keyboard_input_snapshot_t keyboard_sources[CANONICAL_SOURCE_CAPACITY + 1u];
    canonical_keyboard_report_t keyboard_report;
} canonical_hid_state_t;

void canonical_hid_init(canonical_hid_state_t *state);

bool canonical_hid_apply_keyboard_snapshot(
    canonical_hid_state_t *state,
    const keyboard_input_snapshot_t *snapshot,
    canonical_keyboard_report_t *report,
    bool *changed);

/* Tear down exactly one stable source ID. IDs in the 1..16 ownership capacity
 * are valid even if a later gate has not yet assigned a transport kind to that
 * slot. Unassigned/Mouse-only slots do not alter Keyboard state. */
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
