#ifndef BLU2USB_FORK_CLASSIC_KEYBOARD_REPORT_H
#define BLU2USB_FORK_CLASSIC_KEYBOARD_REPORT_H

#include <stdbool.h>
#include <stdint.h>

#include "keyboard_input.h"

bool classic_keyboard_parse_report(
    const uint8_t *descriptor,
    uint16_t descriptor_len,
    const uint8_t *hid_transaction_report,
    uint16_t report_len,
    keyboard_input_snapshot_t *snapshot);

#endif
