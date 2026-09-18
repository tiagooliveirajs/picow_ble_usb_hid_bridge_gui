#include "ui_runtime.h"

#include <stdbool.h>
#include <stdint.h>

#include "bridge_bus.h"
#include "bt_runtime.h"
#include "hat.h"
#include "st7789_pico.h"
#include "ui_model.h"
#include "ui_renderer.h"

typedef enum {
    KEYBOARD_UI_SCANNING = 0,
    KEYBOARD_UI_BONDING,
    KEYBOARD_UI_CONNECTING,
    KEYBOARD_UI_READY,
    KEYBOARD_UI_RETRYING,
    KEYBOARD_UI_CANCELLED,
} keyboard_ui_phase_t;

static blu2usb_ux_model_t g_ux;
static blu2usb_display_hal_t g_display;
static bool g_initialized;
static bool g_keyboard_connected;
/* Composite transport is a FORK-12 feature. Keeping the projection state here
 * makes its future visual semantics explicit without pretending the transport
 * exists in FORK-05. */
static bool g_composite_connected;
static bool g_dirty;
static keyboard_ui_phase_t g_keyboard_phase = KEYBOARD_UI_SCANNING;

static void clear_row(blu2usb_ui_frame_t *frame, uint8_t row) {
    if (frame == NULL || row >= BLU2USB_RENDERER_TEXT_ROWS) return;
    for (uint8_t column = 0u; column < BLU2USB_RENDERER_TEXT_COLS; ++column) {
        frame->cells[row][column].character = ' ';
        frame->cells[row][column].tone = BLU2USB_UI_TONE_ACTIONABLE;
    }
}

static void replace_row(blu2usb_ui_frame_t *frame, uint8_t row,
                        const char *text, blu2usb_ui_tone_t tone) {
    clear_row(frame, row);
    (void)blu2usb_ui_frame_set_text(frame, row, 0u, text, tone);
}

static void replace_option_row(blu2usb_ui_frame_t *frame, uint8_t row,
                               const char *text, bool current,
                               unsigned option_index) {
    blu2usb_ui_tone_t tone = current
        ? BLU2USB_UI_TONE_CURRENT
        : BLU2USB_UI_TONE_ACTIONABLE;
    if (g_ux.selection == option_index) tone = BLU2USB_UI_TONE_EMPHASIZED;
    replace_row(frame, row, text, tone);
}

static void project_runtime_state(blu2usb_ui_frame_t *frame) {
    if (g_ux.screen == BLU2USB_SCREEN_MOUSE_OPTIONS) {
        const bool connected = blu2usb_ux_mouse_connected();
        replace_option_row(frame, 1u,
            connected ? " MOUSE PAIRED" : " PAIR MOUSE",
            connected, 0u);
        return;
    }

    if (g_ux.screen == BLU2USB_SCREEN_OTHER_OPTIONS) {
        replace_option_row(frame, 1u,
            g_keyboard_connected ? " KEYBOARD PAIRED" : " PAIR KEYBOARD",
            g_keyboard_connected, 0u);
        replace_option_row(frame, 2u,
            g_composite_connected ? " COMPOSITE PAIRED" : " PAIR COMPOSITE",
            g_composite_connected, 1u);
        return;
    }

    if (g_ux.screen == BLU2USB_SCREEN_OTHER_DEVICES_STATUS) {
        if (g_keyboard_connected) {
            replace_row(frame, 1u, "KEYBOARD CONNECTED", BLU2USB_UI_TONE_CURRENT);
            replace_row(frame, 2u, "", BLU2USB_UI_TONE_STATIC);
        } else {
            replace_row(frame, 1u, "KEYBOARD", BLU2USB_UI_TONE_STATIC);
            replace_row(frame, 2u, "NOT CONNECTED", BLU2USB_UI_TONE_STATIC);
        }

        if (g_composite_connected) {
            replace_row(frame, 3u, "COMPOSITE CONNECTED", BLU2USB_UI_TONE_CURRENT);
            replace_row(frame, 4u, "", BLU2USB_UI_TONE_STATIC);
        } else {
            replace_row(frame, 3u, "COMPOSITE", BLU2USB_UI_TONE_STATIC);
            replace_row(frame, 4u, "NOT CONNECTED", BLU2USB_UI_TONE_STATIC);
        }
        return;
    }

    if (g_ux.screen == BLU2USB_SCREEN_PAIR_COMPOSITE && g_composite_connected) {
        replace_row(frame, 0u, "COMPOSITE PAIRED", BLU2USB_UI_TONE_TITLE);
        replace_row(frame, 1u, "COMPOSITE CONNECTED", BLU2USB_UI_TONE_CURRENT);
        replace_row(frame, 2u, "READY TO USE", BLU2USB_UI_TONE_CURRENT);
        replace_row(frame, 3u, "", BLU2USB_UI_TONE_STATIC);
        replace_row(frame, 4u, "", BLU2USB_UI_TONE_STATIC);
        return;
    }

    if (g_ux.screen != BLU2USB_SCREEN_PAIR_KEYBOARD) return;

    if (g_keyboard_connected || g_keyboard_phase == KEYBOARD_UI_READY) {
        replace_row(frame, 0u, "KEYBOARD PAIRED", BLU2USB_UI_TONE_TITLE);
        replace_row(frame, 1u, "KEYBOARD CONNECTED", BLU2USB_UI_TONE_CURRENT);
        replace_row(frame, 2u, "READY TO USE", BLU2USB_UI_TONE_CURRENT);
        replace_row(frame, 3u, "", BLU2USB_UI_TONE_STATIC);
        replace_row(frame, 4u, "", BLU2USB_UI_TONE_STATIC);
        return;
    }

    switch (g_keyboard_phase) {
    case KEYBOARD_UI_BONDING:
        replace_row(frame, 1u, "PAIRING KEYBOARD", BLU2USB_UI_TONE_STATIC);
        replace_row(frame, 2u, "SECURITY LEVEL 2", BLU2USB_UI_TONE_STATIC);
        replace_row(frame, 3u, "JUST WORKS", BLU2USB_UI_TONE_STATIC);
        replace_row(frame, 4u, "PLEASE WAIT", BLU2USB_UI_TONE_STATIC);
        break;
    case KEYBOARD_UI_CONNECTING:
        replace_row(frame, 1u, "CONNECTING KEYBOARD", BLU2USB_UI_TONE_STATIC);
        replace_row(frame, 2u, "HID REPORT MODE", BLU2USB_UI_TONE_STATIC);
        replace_row(frame, 3u, "PLEASE WAIT", BLU2USB_UI_TONE_STATIC);
        replace_row(frame, 4u, "", BLU2USB_UI_TONE_STATIC);
        break;
    case KEYBOARD_UI_RETRYING:
        replace_row(frame, 1u, "RETRYING KEYBOARD", BLU2USB_UI_TONE_STATIC);
        replace_row(frame, 2u, "SEARCH WILL RESUME", BLU2USB_UI_TONE_STATIC);
        replace_row(frame, 3u, "", BLU2USB_UI_TONE_STATIC);
        replace_row(frame, 4u, "", BLU2USB_UI_TONE_STATIC);
        break;
    case KEYBOARD_UI_CANCELLED:
        replace_row(frame, 1u, "PAIRING CANCELLED", BLU2USB_UI_TONE_STATIC);
        replace_row(frame, 2u, "KEY A TO RETRY", BLU2USB_UI_TONE_STATIC);
        replace_row(frame, 3u, "", BLU2USB_UI_TONE_STATIC);
        replace_row(frame, 4u, "", BLU2USB_UI_TONE_STATIC);
        break;
    case KEYBOARD_UI_SCANNING:
    default:
        replace_row(frame, 1u, "SEARCHING KEYBOARD", BLU2USB_UI_TONE_STATIC);
        replace_row(frame, 2u, "TARGET KEYBOARD", BLU2USB_UI_TONE_STATIC);
        replace_row(frame, 3u, "AUTO SEARCH ACTIVE", BLU2USB_UI_TONE_STATIC);
        replace_row(frame, 4u, "FOUND 0 HID", BLU2USB_UI_TONE_STATIC);
        break;
    }
}

static bool render_state(void) {
    blu2usb_ui_frame_t frame;
    blu2usb_ui_project(&g_ux, &frame);
    blu2usb_ui_enforce_applied_visual_contract(&g_ux, &frame);
    project_runtime_state(&frame);
    g_dirty = false;
    return blu2usb_renderer_render(&g_display, &frame);
}

static void send_bt_command(uint16_t type) {
    const bridge_message_t message = {
        .channel = BRIDGE_CHANNEL_CONTROL,
        .type = type,
        .length = 0u,
    };
    (void)bridge_bus_send_app_command(&message);
}

static void handle_ux_command(blu2usb_ux_command_t command,
                              blu2usb_screen_id_t previous_screen) {
    switch (command.kind) {
    case BLU2USB_UX_COMMAND_PAIR_MOUSE:
        send_bt_command(BT_COMMAND_BLE_MOUSE_RETRY);
        break;
    case BLU2USB_UX_COMMAND_PAIR_KEYBOARD:
        /* Accessing the Keyboard option while already connected is live
         * feedback, not a destructive re-pair request. */
        if (!g_keyboard_connected) send_bt_command(BT_COMMAND_CLASSIC_RETRY);
        break;
    case BLU2USB_UX_COMMAND_RETRY:
        if (previous_screen == BLU2USB_SCREEN_PAIR_MOUSE)
            send_bt_command(BT_COMMAND_BLE_MOUSE_RETRY);
        else if (previous_screen == BLU2USB_SCREEN_PAIR_KEYBOARD)
            send_bt_command(BT_COMMAND_CLASSIC_RETRY);
        break;
    case BLU2USB_UX_COMMAND_CUSTOM_SET_TARGET:
        blu2usb_ux_set_custom_target(&g_ux, command.source, command.target);
        break;
    default:
        break;
    }
}

bool ui_runtime_init(void) {
    blu2usb_ux_init(&g_ux);
    /* Physical correction: first visible page is the didactic Learn screen. */
    g_ux.screen = BLU2USB_SCREEN_LEARN_KEYS;
    g_ux.selection = 0u;
    g_keyboard_connected = false;
    g_composite_connected = false;
    blu2usb_ux_set_mouse_connected(false);
    blu2usb_hat_pico_init();
    if (!blu2usb_st7789_pico_init(&g_display)) return false;
    g_initialized = true;
    g_dirty = true;
    if (!render_state()) return false;
    blu2usb_st7789_pico_set_backlight(true);
    return true;
}

void ui_runtime_on_bt_event(uint16_t event_type) {
    if (!g_initialized) return;
    switch (event_type) {
    case BT_EVENT_CLASSIC_SCANNING:
        g_keyboard_connected = false;
        g_keyboard_phase = KEYBOARD_UI_SCANNING;
        break;
    case BT_EVENT_CLASSIC_BONDING:
        g_keyboard_connected = false;
        g_keyboard_phase = KEYBOARD_UI_BONDING;
        break;
    case BT_EVENT_CLASSIC_CONNECTING:
        g_keyboard_connected = false;
        g_keyboard_phase = KEYBOARD_UI_CONNECTING;
        break;
    case BT_EVENT_CLASSIC_READY:
        g_keyboard_connected = true;
        g_keyboard_phase = KEYBOARD_UI_READY;
        break;
    case BT_EVENT_CLASSIC_RETRYING:
        g_keyboard_connected = false;
        g_keyboard_phase = KEYBOARD_UI_RETRYING;
        break;
    case BT_EVENT_CLASSIC_CANCELLED:
        g_keyboard_connected = false;
        g_keyboard_phase = KEYBOARD_UI_CANCELLED;
        break;
    case BT_EVENT_BLE_MOUSE_SCANNING:
    case BT_EVENT_BLE_MOUSE_CONNECTING:
        blu2usb_ux_set_mouse_connected(false);
        break;
    case BT_EVENT_BLE_MOUSE_READY:
        blu2usb_ux_set_mouse_connected(true);
        if (g_ux.screen == BLU2USB_SCREEN_PAIR_MOUSE) {
            g_ux.screen = BLU2USB_SCREEN_MOUSE_SAVED;
            g_ux.selection = 0u;
        }
        break;
    case BT_EVENT_BLE_MOUSE_DISCONNECTED:
        blu2usb_ux_set_mouse_connected(false);
        break;
    default:
        return;
    }
    g_dirty = true;
}

void ui_runtime_task(void) {
    if (!g_initialized) return;
    blu2usb_hat_pico_task();
    blu2usb_hat_event_t event;
    while (blu2usb_hat_pico_poll_event(&event)) {
        const bool was_locked = blu2usb_interaction_is_locked(&g_ux.interaction);
        const blu2usb_screen_id_t previous_screen = g_ux.screen;
        const blu2usb_ux_command_t command =
            blu2usb_ux_input(&g_ux, event.control, event.pressed);

        if (!event.pressed && event.control == BLU2USB_CONTROL_KEY_B) {
            if (previous_screen == BLU2USB_SCREEN_PAIR_KEYBOARD)
                send_bt_command(BT_COMMAND_CLASSIC_CANCEL);
            else if (previous_screen == BLU2USB_SCREEN_PAIR_MOUSE)
                send_bt_command(BT_COMMAND_BLE_MOUSE_CANCEL);
        }
        handle_ux_command(command, previous_screen);

        const bool is_locked = blu2usb_interaction_is_locked(&g_ux.interaction);
        if (!was_locked && is_locked) {
            blu2usb_st7789_pico_set_backlight(false);
        } else if (was_locked && !is_locked) {
            g_dirty = true;
            (void)render_state();
            blu2usb_st7789_pico_set_backlight(true);
        } else if (!is_locked) {
            g_dirty = true;
            (void)render_state();
        }
    }
    if (g_dirty && !blu2usb_interaction_is_locked(&g_ux.interaction))
        (void)render_state();
}
