#include "ui_model.h"
#include <string.h>

static bool g_mouse_connected;

static bool valid_profile(blu2usb_mouse_profile_kind_t profile) {
    return profile >= BLU2USB_MOUSE_PROFILE_PASSTHROUGH && profile <= BLU2USB_MOUSE_PROFILE_CUSTOM_REMAP;
}
static bool valid_target(blu2usb_mouse_target_t target) { return (unsigned)target < BLU2USB_MOUSE_TARGET_COUNT; }
void blu2usb_ux_set_mouse_connected(bool connected) { g_mouse_connected = connected; }
bool blu2usb_ux_mouse_connected(void) { return g_mouse_connected; }
void blu2usb_ux_restore_profile_state(blu2usb_ux_model_t *ux, blu2usb_mouse_profile_kind_t active_profile, const blu2usb_mouse_target_t custom_targets[BLU2USB_MOUSE_SOURCE_COUNT]) {
    if (ux == NULL || custom_targets == NULL || !valid_profile(active_profile)) return;
    for (unsigned i=0u;i<BLU2USB_MOUSE_SOURCE_COUNT;++i) if (!valid_target(custom_targets[i])) return;
    ux->active_profile=active_profile; memcpy(ux->custom_targets,custom_targets,sizeof(ux->custom_targets)); ux->custom_dirty=false;
}
void blu2usb_ux_profile_applied(blu2usb_ux_model_t *ux, blu2usb_mouse_profile_kind_t active_profile) {
    if (ux == NULL || !valid_profile(active_profile)) return;
    ux->active_profile=active_profile; ux->selection=0u;
    switch(active_profile){
    case BLU2USB_MOUSE_PROFILE_PASSTHROUGH: ux->screen=BLU2USB_SCREEN_PASSTHROUGH_APPLIED; break;
    case BLU2USB_MOUSE_PROFILE_DEFAULT_REMAP: ux->screen=BLU2USB_SCREEN_DEFAULT_APPLIED; break;
    case BLU2USB_MOUSE_PROFILE_ESCAPE_REMAP: ux->screen=BLU2USB_SCREEN_ESCAPE_APPLIED; break;
    case BLU2USB_MOUSE_PROFILE_CUSTOM_REMAP: ux->custom_dirty=false; ux->screen=BLU2USB_SCREEN_CUSTOM_APPLIED; break;
    default: break;
    }
}
