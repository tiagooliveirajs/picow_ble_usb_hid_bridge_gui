#include <string.h>

#include "bsp/board_api.h"
#include "tusb.h"
#include "usb_descriptors.h"

#define USB_VID 0xCAFE
#define USB_BCD 0x0200

#define _PID_MAP(itf, n) ((CFG_TUD_##itf) << (n))
#define USB_PID (0x4000 | _PID_MAP(CDC, 0) | _PID_MAP(MSC, 1) | \
                 _PID_MAP(HID, 2) | _PID_MAP(MIDI, 3) | _PID_MAP(VENDOR, 4))

tusb_desc_device_t const desc_device = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = USB_BCD,
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = USB_VID,
    .idProduct = USB_PID,
    .bcdDevice = 0x0100,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01,
};

uint8_t const *tud_descriptor_device_cb(void) {
    return (uint8_t const *)&desc_device;
}

// Fixed boot-compatible keyboard descriptor. The Bluetooth report descriptor is
// parsed on Core 1 and normalized to the standard 8-byte USB keyboard report.
uint8_t const desc_hid_report[] = {
    TUD_HID_REPORT_DESC_KEYBOARD()
};

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance) {
    (void)instance;
    return desc_hid_report;
}

enum {
    ITF_NUM_CDC = 0,
    ITF_NUM_CDC_DATA,
    ITF_NUM_HID,
    ITF_NUM_TOTAL
};

#define CONFIG_TOTAL_LEN \
    (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_HID_DESC_LEN)

#define EPNUM_CDC_NOTIF 0x81
#define EPNUM_CDC_OUT   0x02
#define EPNUM_CDC_IN    0x82
#define EPNUM_HID_IN    0x83

uint8_t const desc_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(
        1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN,
        TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 250),

    TUD_CDC_DESCRIPTOR(
        ITF_NUM_CDC, 4,
        EPNUM_CDC_NOTIF, 8,
        EPNUM_CDC_OUT, EPNUM_CDC_IN, 64),

    TUD_HID_DESCRIPTOR(
        ITF_NUM_HID, 5,
        HID_ITF_PROTOCOL_KEYBOARD,
        sizeof(desc_hid_report),
        EPNUM_HID_IN,
        CFG_TUD_HID_EP_BUFSIZE,
        1),
};

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    return desc_configuration;
}

enum {
    STRID_LANGID = 0,
    STRID_MANUFACTURER,
    STRID_PRODUCT,
    STRID_SERIAL,
    STRID_CDC,
    STRID_HID,
};

static char const *string_desc_arr[] = {
    (const char[]){0x09, 0x04},
    "OpenAI POC / TinyUSB",
    "RP2350 BKB-3G Classic HID POC",
    NULL,
    "POC debug log",
    "BKB-3G USB Keyboard",
};

static uint16_t _desc_str[32 + 1];

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;
    size_t chr_count;

    switch (index) {
        case STRID_LANGID:
            memcpy(&_desc_str[1], string_desc_arr[0], 2);
            chr_count = 1;
            break;

        case STRID_SERIAL:
            chr_count = board_usb_get_serial(_desc_str + 1, 32);
            break;

        default:
            if (index >= sizeof(string_desc_arr) / sizeof(string_desc_arr[0])) {
                return NULL;
            }

            const char *str = string_desc_arr[index];
            if (str == NULL) return NULL;

            chr_count = strlen(str);
            const size_t max_count =
                sizeof(_desc_str) / sizeof(_desc_str[0]) - 1u;
            if (chr_count > max_count) chr_count = max_count;

            for (size_t i = 0; i < chr_count; ++i) {
                _desc_str[1 + i] = (uint8_t)str[i];
            }
            break;
    }

    _desc_str[0] =
        (uint16_t)((TUSB_DESC_STRING << 8) | (2u * chr_count + 2u));
    return _desc_str;
}
