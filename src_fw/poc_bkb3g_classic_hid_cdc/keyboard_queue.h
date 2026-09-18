#ifndef POC_KEYBOARD_QUEUE_H
#define POC_KEYBOARD_QUEUE_H

#include <stdbool.h>
#include <stdint.h>

#define POC_USB_KEYBOARD_REPORT_LEN 8u

void keyboard_queue_init(void);
void keyboard_queue_clear(void);
void keyboard_queue_push(const uint8_t report[POC_USB_KEYBOARD_REPORT_LEN]);
bool keyboard_queue_peek(uint8_t report[POC_USB_KEYBOARD_REPORT_LEN]);
void keyboard_queue_advance(void);

#endif
