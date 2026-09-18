#include "canonical_hid.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

#define CANONICAL_MAX_TRACKED_KEYS \
    (CANONICAL_KEYBOARD_KEYCODE_COUNT * CANONICAL_SOURCE_CAPACITY)

static bool report_equal(
    const canonical_keyboard_report_t *a,
    const canonical_keyboard_report_t *b) {
    return memcmp(a, b, sizeof(*a)) == 0;
}

static bool source_state_empty(const canonical_hid_source_state_t *slot) {
    if (slot->mouse_buttons != 0u || slot->modifiers != 0u) return false;
    for (size_t i = 0u; i < CANONICAL_KEYBOARD_KEYCODE_COUNT; ++i) {
        if (slot->keycodes[i] != 0u) return false;
    }
    return true;
}

static canonical_hid_source_state_t *find_source(
    canonical_hid_state_t *state,
    canonical_source_t source) {
    for (size_t i = 0u; i < CANONICAL_SOURCE_CAPACITY; ++i) {
        canonical_hid_source_state_t *slot = &state->sources[i];
        if (slot->active && canonical_source_equal(slot->id, source)) return slot;
    }
    return NULL;
}

static canonical_hid_source_state_t *find_or_allocate_source(
    canonical_hid_state_t *state,
    canonical_source_t source) {
    canonical_hid_source_state_t *slot = find_source(state, source);
    if (slot != NULL) return slot;

    for (size_t i = 0u; i < CANONICAL_SOURCE_CAPACITY; ++i) {
        slot = &state->sources[i];
        if (!slot->active) {
            memset(slot, 0, sizeof(*slot));
            slot->active = true;
            slot->id = source;
            return slot;
        }
    }
    return NULL;
}

static void release_slot_if_empty(canonical_hid_source_state_t *slot) {
    if (slot != NULL && source_state_empty(slot)) memset(slot, 0, sizeof(*slot));
}

static bool key_already_present(
    const uint8_t *keys,
    size_t count,
    uint8_t keycode) {
    for (size_t i = 0u; i < count; ++i) {
        if (keys[i] == keycode) return true;
    }
    return false;
}

static void sort_keycodes(uint8_t *keys, size_t count) {
    for (size_t i = 1u; i < count; ++i) {
        const uint8_t value = keys[i];
        size_t j = i;
        while (j > 0u && keys[j - 1u] > value) {
            keys[j] = keys[j - 1u];
            --j;
        }
        keys[j] = value;
    }
}

static canonical_keyboard_report_t rebuild_keyboard_report(
    const canonical_hid_state_t *state) {
    canonical_keyboard_report_t report = {0};
    uint8_t unique_keys[CANONICAL_MAX_TRACKED_KEYS] = {0};
    size_t unique_count = 0u;
    bool rollover = false;

    for (size_t source_index = 0u;
         source_index < CANONICAL_SOURCE_CAPACITY;
         ++source_index) {
        const canonical_hid_source_state_t *slot = &state->sources[source_index];
        if (!slot->active || !canonical_source_kind_is_keyboard(slot->id.kind)) continue;

        report.modifiers |= slot->modifiers;
        for (size_t key_index = 0u;
             key_index < CANONICAL_KEYBOARD_KEYCODE_COUNT;
             ++key_index) {
            const uint8_t keycode = slot->keycodes[key_index];
            if (keycode == 0u) continue;
            if (keycode == CANONICAL_KEYBOARD_ERROR_ROLLOVER) {
                rollover = true;
                continue;
            }
            if (key_already_present(unique_keys, unique_count, keycode)) continue;
            if (unique_count < CANONICAL_MAX_TRACKED_KEYS) {
                unique_keys[unique_count++] = keycode;
            }
        }
    }

    if (rollover || unique_count > CANONICAL_KEYBOARD_KEYCODE_COUNT) {
        memset(report.keycodes,
               CANONICAL_KEYBOARD_ERROR_ROLLOVER,
               sizeof(report.keycodes));
        return report;
    }

    sort_keycodes(unique_keys, unique_count);
    memcpy(report.keycodes, unique_keys, unique_count);
    return report;
}

static uint8_t rebuild_mouse_buttons(const canonical_hid_state_t *state) {
    uint8_t buttons = 0u;
    for (size_t i = 0u; i < CANONICAL_SOURCE_CAPACITY; ++i) {
        const canonical_hid_source_state_t *slot = &state->sources[i];
        if (slot->active && canonical_source_kind_is_mouse(slot->id.kind)) {
            buttons |= slot->mouse_buttons;
        }
    }
    return buttons;
}

static void publish_keyboard_rebuild(
    canonical_hid_state_t *state,
    canonical_keyboard_report_t *report,
    bool *changed) {
    const canonical_keyboard_report_t next = rebuild_keyboard_report(state);
    const bool did_change = !report_equal(&state->keyboard_report, &next);
    state->keyboard_report = next;
    if (report != NULL) *report = next;
    if (changed != NULL) *changed = did_change;
}

static int32_t saturating_add_i32(int32_t current, int32_t delta) {
    const int64_t sum = (int64_t)current + (int64_t)delta;
    if (sum > INT32_MAX) return INT32_MAX;
    if (sum < INT32_MIN) return INT32_MIN;
    return (int32_t)sum;
}

static bool valid_consumption(int32_t pending, int32_t consumed) {
    if (consumed == 0) return true;
    if (pending == 0) return false;
    if ((pending > 0) != (consumed > 0)) return false;
    const int64_t pending_abs = pending < 0 ? -(int64_t)pending : (int64_t)pending;
    const int64_t consumed_abs = consumed < 0 ? -(int64_t)consumed : (int64_t)consumed;
    return consumed_abs <= pending_abs;
}

void canonical_hid_init(canonical_hid_state_t *state) {
    if (state != NULL) memset(state, 0, sizeof(*state));
}

bool canonical_hid_apply_keyboard_snapshot(
    canonical_hid_state_t *state,
    const keyboard_input_snapshot_t *snapshot,
    canonical_keyboard_report_t *report,
    bool *changed) {
    if (state == NULL || snapshot == NULL) {
        if (changed != NULL) *changed = false;
        return false;
    }

    const canonical_source_t source = keyboard_input_snapshot_source(snapshot);
    if (!canonical_source_is_valid(source) ||
        !canonical_source_kind_is_keyboard(source.kind)) {
        if (changed != NULL) *changed = false;
        return false;
    }

    bool neutral = snapshot->modifiers == 0u;
    for (size_t i = 0u; i < CANONICAL_KEYBOARD_KEYCODE_COUNT; ++i) {
        if (snapshot->keycodes[i] != 0u) neutral = false;
    }

    canonical_hid_source_state_t *slot = find_source(state, source);
    if (slot == NULL && !neutral) {
        slot = find_or_allocate_source(state, source);
        if (slot == NULL) {
            if (changed != NULL) *changed = false;
            return false;
        }
    }

    if (slot != NULL) {
        slot->modifiers = snapshot->modifiers;
        memcpy(slot->keycodes, snapshot->keycodes, sizeof(slot->keycodes));
        release_slot_if_empty(slot);
    }

    publish_keyboard_rebuild(state, report, changed);
    return true;
}

bool canonical_hid_apply_mouse(
    canonical_hid_state_t *state,
    const canonical_mouse_event_t *event) {
    if (state == NULL || event == NULL ||
        !canonical_source_is_valid(event->source) ||
        !canonical_source_kind_is_mouse(event->source.kind)) {
        return false;
    }

    switch (event->type) {
        case CANONICAL_MOUSE_EVENT_BUTTON: {
            if ((unsigned int)event->data.button.button >= CANONICAL_MOUSE_BUTTON_COUNT) {
                return false;
            }
            const uint8_t mask =
                (uint8_t)(1u << (uint8_t)event->data.button.button);
            canonical_hid_source_state_t *slot = find_source(state, event->source);
            if (event->data.button.pressed) {
                if (slot == NULL) {
                    slot = find_or_allocate_source(state, event->source);
                    if (slot == NULL) return false;
                }
                slot->mouse_buttons |= mask;
            } else if (slot != NULL) {
                slot->mouse_buttons &= (uint8_t)~mask;
                release_slot_if_empty(slot);
            }
            return true;
        }

        case CANONICAL_MOUSE_EVENT_MOVE:
            state->pending_dx = saturating_add_i32(state->pending_dx, event->data.move.dx);
            state->pending_dy = saturating_add_i32(state->pending_dy, event->data.move.dy);
            return true;

        case CANONICAL_MOUSE_EVENT_WHEEL:
            state->pending_wheel_vertical = saturating_add_i32(
                state->pending_wheel_vertical, event->data.wheel.vertical);
            state->pending_wheel_horizontal = saturating_add_i32(
                state->pending_wheel_horizontal, event->data.wheel.horizontal);
            return true;

        default:
            return false;
    }
}

bool canonical_hid_release_source(
    canonical_hid_state_t *state,
    canonical_source_t source,
    canonical_keyboard_report_t *report,
    bool *changed) {
    if (state == NULL || !canonical_source_is_valid(source)) {
        if (changed != NULL) *changed = false;
        return false;
    }

    canonical_hid_source_state_t *slot = find_source(state, source);
    if (slot != NULL) memset(slot, 0, sizeof(*slot));
    publish_keyboard_rebuild(state, report, changed);
    return true;
}

void canonical_hid_release_all(
    canonical_hid_state_t *state,
    canonical_keyboard_report_t *report) {
    if (state == NULL) return;
    memset(state->sources, 0, sizeof(state->sources));
    memset(&state->keyboard_report, 0, sizeof(state->keyboard_report));
    if (report != NULL) *report = state->keyboard_report;
}

const canonical_keyboard_report_t *canonical_hid_keyboard_report(
    const canonical_hid_state_t *state) {
    return state == NULL ? NULL : &state->keyboard_report;
}

void canonical_hid_snapshot(
    const canonical_hid_state_t *state,
    canonical_hid_output_state_t *output) {
    if (output == NULL) return;
    memset(output, 0, sizeof(*output));
    if (state == NULL) return;

    output->mouse_buttons = rebuild_mouse_buttons(state);
    output->dx = state->pending_dx;
    output->dy = state->pending_dy;
    output->wheel_vertical = state->pending_wheel_vertical;
    output->wheel_horizontal = state->pending_wheel_horizontal;
    output->keyboard = state->keyboard_report;
}

bool canonical_hid_consume_relative(
    canonical_hid_state_t *state,
    int32_t dx,
    int32_t dy,
    int32_t wheel_vertical,
    int32_t wheel_horizontal) {
    if (state == NULL ||
        !valid_consumption(state->pending_dx, dx) ||
        !valid_consumption(state->pending_dy, dy) ||
        !valid_consumption(state->pending_wheel_vertical, wheel_vertical) ||
        !valid_consumption(state->pending_wheel_horizontal, wheel_horizontal)) {
        return false;
    }

    state->pending_dx -= dx;
    state->pending_dy -= dy;
    state->pending_wheel_vertical -= wheel_vertical;
    state->pending_wheel_horizontal -= wheel_horizontal;
    return true;
}
