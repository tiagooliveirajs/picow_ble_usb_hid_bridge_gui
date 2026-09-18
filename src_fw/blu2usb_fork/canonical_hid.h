#ifndef BLU2USB_FORK_CANONICAL_HID_H
#define BLU2USB_FORK_CANONICAL_HID_H

#include <stdbool.h>
#include <stdint.h>

#include "canonical_source.h"
#include "keyboard_input.h"

#define CANONICAL_KEYBOARD_KEYCODE_COUNT 6u
#define CANONICAL_KEYBOARD_ERROR_ROLLOVER 0x01u
#define CANONICAL_MOUSE_BUTTON_COUNT 8u

typedef enum {
    CANONICAL_MOUSE_BUTTON_LEFT = 0u,
    CANONICAL_MOUSE_BUTTON_RIGHT,
    CANONICAL_MOUSE_BUTTON_MIDDLE,
    CANONICAL_MOUSE_BUTTON_BACK,
    CANONICAL_MOUSE_BUTTON_FORWARD,
    CANONICAL_MOUSE_BUTTON_6,
    CANONICAL_MOUSE_BUTTON_7,
    CANONICAL_MOUSE_BUTTON_8,
} canonical_mouse_button_t;

typedef enum {
    CANONICAL_MOUSE_EVENT_BUTTON = 0u,
    CANONICAL_MOUSE_EVENT_MOVE,
    CANONICAL_MOUSE_EVENT_WHEEL,
} canonical_mouse_event_type_t;

typedef struct {
    canonical_source_t source;
    canonical_mouse_event_type_t type;
    union {
        struct {
            canonical_mouse_button_t button;
            bool pressed;
        } button;
        struct {
            int16_t dx;
            int16_t dy;
        } move;
        struct {
            int16_t vertical;
            int16_t horizontal;
        } wheel;
    } data;
} canonical_mouse_event_t;

typedef struct {
    uint8_t modifiers;
    uint8_t keycodes[CANONICAL_KEYBOARD_KEYCODE_COUNT];
} canonical_keyboard_report_t;

typedef struct {
    uint8_t mouse_buttons;
    int32_t dx;
    int32_t dy;
    int32_t wheel_vertical;
    int32_t wheel_horizontal;
    canonical_keyboard_report_t keyboard;
} canonical_hid_output_state_t;

typedef struct {
    bool active;
    canonical_source_t id;
    uint8_t mouse_buttons;
    uint8_t modifiers;
    uint8_t keycodes[CANONICAL_KEYBOARD_KEYCODE_COUNT];
} canonical_hid_source_state_t;

typedef struct {
    canonical_hid_source_state_t sources[CANONICAL_SOURCE_CAPACITY];
    canonical_keyboard_report_t keyboard_report;
    int32_t pending_dx;
    int32_t pending_dy;
    int32_t pending_wheel_vertical;
    int32_t pending_wheel_horizontal;
} canonical_hid_state_t;

void canonical_hid_init(canonical_hid_state_t *state);

bool canonical_hid_apply_keyboard_snapshot(
    canonical_hid_state_t *state,
    const keyboard_input_snapshot_t *snapshot,
    canonical_keyboard_report_t *report,
    bool *changed);

bool canonical_hid_apply_mouse(
    canonical_hid_state_t *state,
    const canonical_mouse_event_t *event);

bool canonical_hid_release_source(
    canonical_hid_state_t *state,
    canonical_source_t source,
    canonical_keyboard_report_t *report,
    bool *changed);

void canonical_hid_release_all(
    canonical_hid_state_t *state,
    canonical_keyboard_report_t *report);

const canonical_keyboard_report_t *canonical_hid_keyboard_report(
    const canonical_hid_state_t *state);

void canonical_hid_snapshot(
    const canonical_hid_state_t *state,
    canonical_hid_output_state_t *output);

bool canonical_hid_consume_relative(
    canonical_hid_state_t *state,
    int32_t dx,
    int32_t dy,
    int32_t wheel_vertical,
    int32_t wheel_horizontal);

#endif
