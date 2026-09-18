#ifndef BLU2USB_FORK_UI_INTERACTION_H
#define BLU2USB_FORK_UI_INTERACTION_H

#include <stdbool.h>
#include <stdint.h>
#include "ui_control.h"

typedef enum {
    BLU2USB_INTERACTION_NONE = 0,
    BLU2USB_INTERACTION_RELEASE,
    BLU2USB_INTERACTION_UNLOCK
} blu2usb_interaction_event_kind_t;

typedef struct {
    blu2usb_interaction_event_kind_t kind;
    blu2usb_control_t control;
} blu2usb_interaction_event_t;

typedef struct {
    uint16_t held_mask;
    bool locked;
    bool unlock_armed;
    blu2usb_control_t unlock_control;
} blu2usb_interaction_t;

void blu2usb_interaction_init(blu2usb_interaction_t *state);
void blu2usb_interaction_lock(blu2usb_interaction_t *state);
bool blu2usb_interaction_is_locked(const blu2usb_interaction_t *state);
bool blu2usb_interaction_is_pressed(const blu2usb_interaction_t *state, blu2usb_control_t control);
blu2usb_interaction_event_t blu2usb_interaction_input(blu2usb_interaction_t *state, blu2usb_control_t control, bool pressed);

#endif
