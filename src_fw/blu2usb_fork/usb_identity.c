#include "usb_hid.h"

_Static_assert(BLU2USB_USB_VID == UINT16_C(0xCAFE), "FORK-04 fixed VID must not drift");
_Static_assert(BLU2USB_USB_PID == UINT16_C(0x4010), "FORK-04 fixed PID must not drift");
_Static_assert(BLU2USB_USB_BCD_DEVICE == UINT16_C(0x0100), "FORK-04 fixed revision must not drift");
_Static_assert(BLU2USB_USB_HID_INTERFACE_COUNT == 2u, "FORK-04 requires exactly two HID interfaces");
_Static_assert(BLU2USB_USB_HID_MOUSE_INTERFACE == 0u, "FORK-04 Mouse must remain interface 0");
_Static_assert(BLU2USB_USB_HID_KEYBOARD_INTERFACE == 1u, "FORK-04 Keyboard must remain interface 1");

static const usb_hid_identity_t k_identity = {
    .vid = BLU2USB_USB_VID,
    .pid = BLU2USB_USB_PID,
    .bcd_device = BLU2USB_USB_BCD_DEVICE,
    .interface_count = BLU2USB_USB_HID_INTERFACE_COUNT,
    .mouse_interface = BLU2USB_USB_HID_MOUSE_INTERFACE,
    .keyboard_interface = BLU2USB_USB_HID_KEYBOARD_INTERFACE,
};

const usb_hid_identity_t *usb_hid_identity(void) {
    return &k_identity;
}
