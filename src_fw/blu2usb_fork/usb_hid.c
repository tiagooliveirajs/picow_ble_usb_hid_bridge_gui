#include "usb_hid.h"

#include <string.h>

#include "bsp/board_api.h"
#include "tusb.h"

_Static_assert(sizeof(usb_mouse_report_t) == 5u, "USB mouse report must stay five bytes");
_Static_assert(sizeof(usb_keyboard_report_t) == 8u, "USB keyboard report must stay eight bytes");

static bool g_release_mouse_pending;
static bool g_release_keyboard_pending;

bool usb_hid_init(void) {
    g_release_mouse_pending = false;
    g_release_keyboard_pending = false;

    if (!tud_init(BOARD_TUD_RHPORT)) return false;
    if (board_init_after_tusb) board_init_after_tusb();
    return true;
}

void usb_hid_release_all(void) {
    g_release_mouse_pending = true;
    g_release_keyboard_pending = true;
}

void usb_hid_task(void) {
    tud_task();

    if (tud_suspended()) {
        tud_remote_wakeup();
        return;
    }

    if (g_release_mouse_pending &&
        tud_hid_n_ready(BLU2USB_USB_HID_MOUSE_INTERFACE)) {
        const usb_mouse_report_t neutral = {0};
        if (tud_hid_n_report(
                BLU2USB_USB_HID_MOUSE_INTERFACE,
                0u,
                &neutral,
                (uint16_t)sizeof(neutral))) {
            g_release_mouse_pending = false;
        }
    }

    if (g_release_keyboard_pending &&
        tud_hid_n_ready(BLU2USB_USB_HID_KEYBOARD_INTERFACE)) {
        const usb_keyboard_report_t neutral = {0};
        if (tud_hid_n_report(
                BLU2USB_USB_HID_KEYBOARD_INTERFACE,
                0u,
                &neutral,
                (uint16_t)sizeof(neutral))) {
            g_release_keyboard_pending = false;
        }
    }
}

uint16_t tud_hid_get_report_cb(
    uint8_t instance,
    uint8_t report_id,
    hid_report_type_t report_type,
    uint8_t *buffer,
    uint16_t reqlen) {
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;
    return 0u;
}

void tud_hid_set_report_cb(
    uint8_t instance,
    uint8_t report_id,
    hid_report_type_t report_type,
    const uint8_t *buffer,
    uint16_t bufsize) {
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)bufsize;
}
