#ifndef BLU2USB_FORK_UI_MODEL_H
#define BLU2USB_FORK_UI_MODEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "ui_control.h"
#include "ui_profile.h"
#include "ui_interaction.h"

typedef enum {
    BLU2USB_SCREEN_HOME = 0,
    BLU2USB_SCREEN_MOUSE_STATUS,
    BLU2USB_SCREEN_OTHER_DEVICES_STATUS,
    BLU2USB_SCREEN_MOUSE_HELP,
    BLU2USB_SCREEN_DEVICES_HELP,
    BLU2USB_SCREEN_MOUSE_OPTIONS,
    BLU2USB_SCREEN_PAIR_MOUSE,
    BLU2USB_SCREEN_PAIR_MOUSE_HELP,
    BLU2USB_SCREEN_MOUSE_SAVED,
    BLU2USB_SCREEN_APPLY_PASSTHROUGH,
    BLU2USB_SCREEN_PASSTHROUGH_APPLIED,
    BLU2USB_SCREEN_APPLY_DEFAULT,
    BLU2USB_SCREEN_DEFAULT_APPLIED,
    BLU2USB_SCREEN_APPLY_ESCAPE,
    BLU2USB_SCREEN_ESCAPE_APPLIED,
    BLU2USB_SCREEN_EDIT_CUSTOM,
    BLU2USB_SCREEN_CUSTOM_APPLIED,
    BLU2USB_SCREEN_LEFT_WILL_BECOME,
    BLU2USB_SCREEN_RIGHT_WILL_BECOME,
    BLU2USB_SCREEN_MIDDLE_WILL_BECOME,
    BLU2USB_SCREEN_FORWARD_WILL_BECOME,
    BLU2USB_SCREEN_BACKWARD_WILL_BECOME,
    BLU2USB_SCREEN_OTHER_OPTIONS,
    BLU2USB_SCREEN_OTHER_OPTIONS_HELP,
    BLU2USB_SCREEN_PAIR_KEYBOARD,
    BLU2USB_SCREEN_PAIR_KEYBOARD_HELP,
    BLU2USB_SCREEN_KEYBOARD_SAVED,
    BLU2USB_SCREEN_PAIR_COMPOSITE,
    BLU2USB_SCREEN_PAIR_COMPOSITE_HELP,
    BLU2USB_SCREEN_COMPOSITE_SAVED,
    BLU2USB_SCREEN_SAVED_DEVICES,
    BLU2USB_SCREEN_DEVICE_DETAILS_MOUSE,
    BLU2USB_SCREEN_DEVICE_DETAILS_KEYBOARD,
    BLU2USB_SCREEN_DEVICE_DETAILS_COMPOSITE,
    BLU2USB_SCREEN_REMOVE_DEVICE,
    BLU2USB_SCREEN_LEARN_KEYS,
    BLU2USB_SCREEN_COUNT
} blu2usb_screen_id_t;

typedef enum {
    BLU2USB_UX_COMMAND_NONE = 0,
    BLU2USB_UX_COMMAND_PAIR_MOUSE,
    BLU2USB_UX_COMMAND_PAIR_KEYBOARD,
    BLU2USB_UX_COMMAND_PAIR_COMPOSITE,
    BLU2USB_UX_COMMAND_RETRY,
    BLU2USB_UX_COMMAND_APPLY_PASSTHROUGH,
    BLU2USB_UX_COMMAND_APPLY_DEFAULT,
    BLU2USB_UX_COMMAND_APPLY_ESCAPE,
    BLU2USB_UX_COMMAND_APPLY_CUSTOM,
    BLU2USB_UX_COMMAND_CUSTOM_SET_TARGET,
    BLU2USB_UX_COMMAND_REMOVE_DEVICE
} blu2usb_ux_command_kind_t;

typedef struct {
    blu2usb_ux_command_kind_t kind;
    blu2usb_mouse_source_t source;
    blu2usb_mouse_target_t target;
} blu2usb_ux_command_t;

typedef struct {
    const char *rows[9];
    uint16_t dynamic_rows;
} blu2usb_screen_template_t;

typedef struct {
    blu2usb_interaction_t interaction;
    blu2usb_screen_id_t screen;
    blu2usb_screen_id_t return_screen;
    unsigned selection;
    unsigned status_page;
    unsigned saved_page;
    unsigned saved_pages;
    unsigned saved_device_count;
    blu2usb_mouse_profile_kind_t active_profile;
    bool custom_dirty;
    blu2usb_mouse_source_t custom_source;
    blu2usb_mouse_target_t custom_targets[BLU2USB_MOUSE_SOURCE_COUNT];
} blu2usb_ux_model_t;

void blu2usb_ux_init(blu2usb_ux_model_t *ux);
blu2usb_ux_command_t blu2usb_ux_input(blu2usb_ux_model_t *ux, blu2usb_control_t control, bool pressed);
void blu2usb_ux_set_saved_device_count(blu2usb_ux_model_t *ux, unsigned count);
void blu2usb_ux_set_custom_target(blu2usb_ux_model_t *ux, blu2usb_mouse_source_t source, blu2usb_mouse_target_t target);
void blu2usb_ux_profile_applied(blu2usb_ux_model_t *ux, blu2usb_mouse_profile_kind_t active_profile);
void blu2usb_ux_restore_profile_state(blu2usb_ux_model_t *ux, blu2usb_mouse_profile_kind_t active_profile, const blu2usb_mouse_target_t custom_targets[BLU2USB_MOUSE_SOURCE_COUNT]);
void blu2usb_ux_set_mouse_connected(bool connected);
bool blu2usb_ux_mouse_connected(void);
const blu2usb_screen_template_t *blu2usb_ux_screen_template(blu2usb_screen_id_t screen);
unsigned blu2usb_ux_option_count(const blu2usb_ux_model_t *ux);
uint16_t blu2usb_ux_learn_white_span_mask(const blu2usb_ux_model_t *ux);

#endif
