#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "tusb.h"
#include "usb_hid.h"

static const uint8_t k_mouse_report_descriptor[] = {
    TUD_HID_REPORT_DESC_MOUSE()
};

static const uint8_t k_keyboard_report_descriptor[] = {
    TUD_HID_REPORT_DESC_KEYBOARD()
};

static const tusb_desc_device_t k_device_descriptor = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = BLU2USB_USB_VID,
    .idProduct = BLU2USB_USB_PID,
    .bcdDevice = BLU2USB_USB_BCD_DEVICE,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x00,
    .bNumConfigurations = 0x01,
};

const uint8_t *tud_descriptor_device_cb(void) {
    return (const uint8_t *)&k_device_descriptor;
}

const uint8_t *tud_hid_descriptor_report_cb(uint8_t instance) {
    if (instance == BLU2USB_USB_HID_MOUSE_INTERFACE) return k_mouse_report_descriptor;
    if (instance == BLU2USB_USB_HID_KEYBOARD_INTERFACE) return k_keyboard_report_descriptor;
    return NULL;
}

enum {
    ITF_NUM_MOUSE = BLU2USB_USB_HID_MOUSE_INTERFACE,
    ITF_NUM_KEYBOARD = BLU2USB_USB_HID_KEYBOARD_INTERFACE,
    ITF_NUM_TOTAL = BLU2USB_USB_HID_INTERFACE_COUNT,
};

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + (2u * TUD_HID_DESC_LEN))
#define EPNUM_MOUSE 0x81u
#define EPNUM_KEYBOARD 0x82u

static const uint8_t k_configuration_descriptor[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 500),
    TUD_HID_DESCRIPTOR(
        ITF_NUM_MOUSE,
        0,
        HID_ITF_PROTOCOL_MOUSE,
        sizeof(k_mouse_report_descriptor),
        EPNUM_MOUSE,
        CFG_TUD_HID_EP_BUFSIZE,
        1),
    TUD_HID_DESCRIPTOR(
        ITF_NUM_KEYBOARD,
        0,
        HID_ITF_PROTOCOL_KEYBOARD,
        sizeof(k_keyboard_report_descriptor),
        EPNUM_KEYBOARD,
        CFG_TUD_HID_EP_BUFSIZE,
        1),
};

const uint8_t *tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    return k_configuration_descriptor;
}

static uint16_t k_string_descriptor[64];

const uint16_t *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;

    if (index == 0u) {
        k_string_descriptor[0] = (uint16_t)((TUSB_DESC_STRING << 8) | 4u);
        k_string_descriptor[1] = 0x0409u;
        return k_string_descriptor;
    }

    const char *text = NULL;
    if (index == 1u) text = BLU2USB_USB_MANUFACTURER;
    if (index == 2u) text = BLU2USB_USB_PRODUCT;
    if (text == NULL) return NULL;

    size_t count = strlen(text);
    const size_t maximum =
        (sizeof(k_string_descriptor) / sizeof(k_string_descriptor[0])) - 1u;
    if (count > maximum) count = maximum;

    for (size_t offset = 0u; offset < count; ++offset) {
        k_string_descriptor[offset + 1u] = (uint8_t)text[offset];
    }
    k_string_descriptor[0] =
        (uint16_t)((TUSB_DESC_STRING << 8) | (uint16_t)(2u * count + 2u));
    return k_string_descriptor;
}
