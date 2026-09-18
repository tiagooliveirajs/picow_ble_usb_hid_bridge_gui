#include "ui_interaction.h"

static uint16_t bit_for(blu2usb_control_t control) {
    return (uint16_t)(1u << (unsigned)control);
}

void blu2usb_interaction_init(blu2usb_interaction_t *state) {
    state->held_mask = 0;
    state->locked = false;
    state->unlock_armed = false;
    state->unlock_control = BLU2USB_CONTROL_JOY_UP;
}

void blu2usb_interaction_lock(blu2usb_interaction_t *state) {
    state->held_mask = 0;
    state->locked = true;
    state->unlock_armed = false;
}

bool blu2usb_interaction_is_locked(const blu2usb_interaction_t *state) {
    return state->locked;
}

bool blu2usb_interaction_is_pressed(const blu2usb_interaction_t *state, blu2usb_control_t control) {
    return (state->held_mask & bit_for(control)) != 0;
}

blu2usb_interaction_event_t blu2usb_interaction_input(blu2usb_interaction_t *state, blu2usb_control_t control, bool pressed) {
    blu2usb_interaction_event_t event = {BLU2USB_INTERACTION_NONE, control};
    if ((unsigned)control >= BLU2USB_CONTROL_COUNT) return event;
    const uint16_t bit = bit_for(control);
    if (pressed) {
        state->held_mask |= bit;
        if (state->locked && !state->unlock_armed) {
            state->unlock_armed = true;
            state->unlock_control = control;
        }
        return event;
    }
    if ((state->held_mask & bit) == 0) return event;
    state->held_mask &= (uint16_t)~bit;
    if (state->locked) {
        if (state->unlock_armed && state->unlock_control == control) {
            state->locked = false;
            state->unlock_armed = false;
            event.kind = BLU2USB_INTERACTION_UNLOCK;
        }
        return event;
    }
    event.kind = BLU2USB_INTERACTION_RELEASE;
    return event;
}
