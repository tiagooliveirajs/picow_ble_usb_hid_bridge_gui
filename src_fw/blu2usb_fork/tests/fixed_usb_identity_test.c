#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "usb_hid.h"

int main(void) {
    const usb_hid_identity_t *identity = usb_hid_identity();
    assert(identity != NULL);
    assert(identity->vid == UINT16_C(0xCAFE));
    assert(identity->pid == UINT16_C(0x4010));
    assert(identity->bcd_device == UINT16_C(0x0100));
    assert(identity->interface_count == 2u);
    assert(identity->mouse_interface == 0u);
    assert(identity->keyboard_interface == 1u);
    assert(strcmp(BLU2USB_USB_MANUFACTURER, "BLU2USB") == 0);
    assert(strcmp(BLU2USB_USB_PRODUCT, "BLU2USB Mouse + Keyboard") == 0);
    puts("PASS: FORK-04 immutable CAFE:4010 Mouse+Keyboard identity");
    return 0;
}
