#include "usb_hid.h"

#include <string.h>

#include "bsp/board_api.h"
#include "tusb.h"

#define USB_KEYBOARD_QUEUE_CAPACITY 16u

_Static_assert(sizeof(usb_mouse_report_t) == 5u, "USB mouse report must stay five bytes");
_Static_assert(sizeof(usb_keyboard_report_t) == 8u, "USB keyboard report must stay eight bytes");

static bool g_release_mouse_pending;
static bool g_release_keyboard_pending;
static usb_keyboard_report_t g_keyboard_queue[USB_KEYBOARD_QUEUE_CAPACITY];
static uint8_t g_keyboard_head;
static uint8_t g_keyboard_tail;
static uint8_t g_keyboard_count;

static void keyboard_queue_reset(void) {
    g_keyboard_head = 0u;
    g_keyboard_tail = 0u;
    g_keyboard_count = 0u;
    memset(g_keyboard_queue, 0, sizeof(g_keyboard_queue));
}

bool usb_hid_init(void) {
    g_release_mouse_pending = false;
    g_release_keyboard_pending = false;
    keyboard_queue_reset();

    if (!tud_init(BOARD_TUD_RHPORT)) return false;
    if (board_init_after_tusb) board_init_after_tusb();
    return true;
}

void usb_hid_release_keyboard(void) {
    keyboard_queue_reset();
    g_release_keyboard_pending = true;
}

void usb_hid_release_all(void) {
    g_release_mouse_pending = true;
    usb_hid_release_keyboard();
}

bool usb_hid_submit_keyboard(const usb_keyboard_report_t *report) {
    if (report == NULL) return false;

    if (g_keyboard_count >= USB_KEYBOARD_QUEUE_CAPACITY) {
        usb_hid_release_keyboard();
        return false;
    }

    g_keyboard_queue[g_keyboard_tail] = *report;
    g_keyboard_tail = (uint8_t)((g_keyboard_tail + 1u) % USB_KEYBOARD_QUEUE_CAPACITY);
    ++g_keyboard_count;
    return true;
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
        return;
    }

    if (g_keyboard_count != 0u &&
        tud_hid_n_ready(BLU2USB_USB_HID_KEYBOARD_INTERFACE)) {
        const usb_keyboard_report_t report = g_keyboard_queue[g_keyboard_head];
        if (tud_hid_n_report(
                BLU2USB_USB_HID_KEYBOARD_INTERFACE,
                0u,
                &report,
                (uint16_t)sizeof(report))) {
            g_keyboard_head =
                (uint8_t)((g_keyboard_head + 1u) % USB_KEYBOARD_QUEUE_CAPACITY);
            --g_keyboard_count;
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
