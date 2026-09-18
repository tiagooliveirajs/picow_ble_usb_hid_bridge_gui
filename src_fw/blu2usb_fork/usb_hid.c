#include "usb_hid.h"

#include "bsp/board_api.h"
#include "tusb.h"

#include "keyboard_report_queue.h"

_Static_assert(sizeof(usb_mouse_report_t) == 5u, "USB mouse report must stay five bytes");
_Static_assert(sizeof(usb_keyboard_report_t) == 8u, "USB keyboard report must stay eight bytes");

static bool g_release_mouse_pending;
static keyboard_report_queue_t g_keyboard_queue;

bool usb_hid_init(void) {
    g_release_mouse_pending = false;
    keyboard_report_queue_init(&g_keyboard_queue);

    if (!tud_init(BOARD_TUD_RHPORT)) return false;
    if (board_init_after_tusb) board_init_after_tusb();
    return true;
}

void usb_hid_replace_keyboard_state(const usb_keyboard_report_t *report) {
    if (report == NULL) return;
    keyboard_report_queue_replace_with(&g_keyboard_queue, report);
}

void usb_hid_release_keyboard(void) {
    const usb_keyboard_report_t neutral = {0};
    usb_hid_replace_keyboard_state(&neutral);
}

void usb_hid_release_all(void) {
    g_release_mouse_pending = true;
    usb_hid_release_keyboard();
}

bool usb_hid_submit_keyboard(const usb_keyboard_report_t *report) {
    return keyboard_report_queue_push(&g_keyboard_queue, report);
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

    if (tud_hid_n_ready(BLU2USB_USB_HID_KEYBOARD_INTERFACE)) {
        usb_keyboard_report_t report;
        if (keyboard_report_queue_peek(&g_keyboard_queue, &report) &&
            tud_hid_n_report(
                BLU2USB_USB_HID_KEYBOARD_INTERFACE,
                0u,
                &report,
                (uint16_t)sizeof(report))) {
            (void)keyboard_report_queue_drop(&g_keyboard_queue);
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
