#ifndef BLU2USB_FORK_USB_HID_H
#define BLU2USB_FORK_USB_HID_H

#include <stdbool.h>
#include <stdint.h>

#define BLU2USB_USB_VID UINT16_C(0xCAFE)
#define BLU2USB_USB_PID UINT16_C(0x4010)
#define BLU2USB_USB_BCD_DEVICE UINT16_C(0x0100)
#define BLU2USB_USB_HID_MOUSE_INTERFACE 0u
#define BLU2USB_USB_HID_KEYBOARD_INTERFACE 1u
#define BLU2USB_USB_HID_INTERFACE_COUNT 2u
#define BLU2USB_USB_KEYCODE_COUNT 6u
#define BLU2USB_USB_MANUFACTURER "BLU2USB"
#define BLU2USB_USB_PRODUCT "BLU2USB Mouse + Keyboard"

typedef struct {
    uint8_t buttons;
    int8_t x;
    int8_t y;
    int8_t wheel;
    int8_t pan;
} usb_mouse_report_t;

typedef struct {
    uint8_t modifiers;
    uint8_t reserved;
    uint8_t keycodes[BLU2USB_USB_KEYCODE_COUNT];
} usb_keyboard_report_t;

bool usb_hid_init(void);
void usb_hid_task(void);
void usb_hid_release_all(void);
void usb_hid_release_keyboard(void);
bool usb_hid_submit_keyboard(const usb_keyboard_report_t *report);

/* Recovery primitive for canonical source teardown. Pending Keyboard reports
 * are discarded and replaced with the exact aggregate state that remains after
 * the failed source is released. This avoids a global neutral release of other
 * still-owned keys/modifiers. */
void usb_hid_replace_keyboard_state(const usb_keyboard_report_t *report);

#endif
