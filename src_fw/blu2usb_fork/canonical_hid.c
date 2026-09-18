#include "canonical_hid.h"

#include <stddef.h>
#include <string.h>

#define CANONICAL_MAX_TRACKED_KEYS \
    (CANONICAL_KEYBOARD_KEYCODE_COUNT * 4u)

static bool report_equal(
    const canonical_keyboard_report_t *a,
    const canonical_keyboard_report_t *b) {
    return memcmp(a, b, sizeof(*a)) == 0;
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
    bool remote_rollover = false;

    for (uint8_t source = 1u; source <= CANONICAL_SOURCE_CAPACITY; ++source) {
        if (!canonical_source_is_keyboard(source)) continue;

        const keyboard_input_snapshot_t *snapshot = &state->keyboard_sources[source];
        report.modifiers |= snapshot->modifiers;

        for (size_t i = 0u; i < KEYBOARD_INPUT_KEYCODE_COUNT; ++i) {
            const uint8_t keycode = snapshot->keycodes[i];
            if (keycode == 0u) continue;
            if (keycode == CANONICAL_KEYBOARD_ERROR_ROLLOVER) {
                remote_rollover = true;
                continue;
            }
            if (key_already_present(unique_keys, unique_count, keycode)) continue;
            if (unique_count < CANONICAL_MAX_TRACKED_KEYS) {
                unique_keys[unique_count++] = keycode;
            }
        }
    }

    if (remote_rollover || unique_count > CANONICAL_KEYBOARD_KEYCODE_COUNT) {
        memset(report.keycodes,
               CANONICAL_KEYBOARD_ERROR_ROLLOVER,
               sizeof(report.keycodes));
        return report;
    }

    sort_keycodes(unique_keys, unique_count);
    memcpy(report.keycodes, unique_keys, unique_count);
    return report;
}

static void publish_rebuild(
    canonical_hid_state_t *state,
    canonical_keyboard_report_t *report,
    bool *changed) {
    const canonical_keyboard_report_t next = rebuild_keyboard_report(state);
    const bool did_change = !report_equal(&state->keyboard_report, &next);
    state->keyboard_report = next;
    if (report != NULL) *report = next;
    if (changed != NULL) *changed = did_change;
}

void canonical_hid_init(canonical_hid_state_t *state) {
    if (state == NULL) return;
    memset(state, 0, sizeof(*state));
    for (uint8_t source = 1u; source <= CANONICAL_SOURCE_CAPACITY; ++source) {
        state->keyboard_sources[source].source = source;
    }
}

bool canonical_hid_apply_keyboard_snapshot(
    canonical_hid_state_t *state,
    const keyboard_input_snapshot_t *snapshot,
    canonical_keyboard_report_t *report,
    bool *changed) {
    if (state == NULL || snapshot == NULL ||
        snapshot->source > CANONICAL_SOURCE_CAPACITY ||
        !canonical_source_is_keyboard(snapshot->source)) {
        if (changed != NULL) *changed = false;
        return false;
    }

    state->keyboard_sources[snapshot->source] = *snapshot;
    publish_rebuild(state, report, changed);
    return true;
}

bool canonical_hid_release_source(
    canonical_hid_state_t *state,
    uint8_t source,
    canonical_keyboard_report_t *report,
    bool *changed) {
    if (state == NULL || source == CANONICAL_SOURCE_INVALID ||
        source > CANONICAL_SOURCE_CAPACITY) {
        if (changed != NULL) *changed = false;
        return false;
    }

    if (canonical_source_is_keyboard(source)) {
        keyboard_input_snapshot_t neutral = {.source = source};
        state->keyboard_sources[source] = neutral;
        publish_rebuild(state, report, changed);
    } else {
        if (report != NULL) *report = state->keyboard_report;
        if (changed != NULL) *changed = false;
    }
    return true;
}

void canonical_hid_release_all(
    canonical_hid_state_t *state,
    canonical_keyboard_report_t *report) {
    if (state == NULL) return;
    canonical_hid_init(state);
    if (report != NULL) *report = state->keyboard_report;
}

const canonical_keyboard_report_t *canonical_hid_keyboard_report(
    const canonical_hid_state_t *state) {
    return state == NULL ? NULL : &state->keyboard_report;
}
