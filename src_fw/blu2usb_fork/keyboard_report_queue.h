#ifndef BLU2USB_FORK_KEYBOARD_REPORT_QUEUE_H
#define BLU2USB_FORK_KEYBOARD_REPORT_QUEUE_H

#include <stdbool.h>
#include <stdint.h>

#include "usb_hid.h"

#define USB_KEYBOARD_QUEUE_CAPACITY 16u

typedef struct {
    usb_keyboard_report_t entries[USB_KEYBOARD_QUEUE_CAPACITY];
    uint8_t head;
    uint8_t tail;
    uint8_t count;
} keyboard_report_queue_t;

void keyboard_report_queue_init(keyboard_report_queue_t *queue);
bool keyboard_report_queue_push(
    keyboard_report_queue_t *queue,
    const usb_keyboard_report_t *report);
bool keyboard_report_queue_peek(
    const keyboard_report_queue_t *queue,
    usb_keyboard_report_t *report);
bool keyboard_report_queue_drop(keyboard_report_queue_t *queue);
void keyboard_report_queue_replace_with(
    keyboard_report_queue_t *queue,
    const usb_keyboard_report_t *report);
uint8_t keyboard_report_queue_count(const keyboard_report_queue_t *queue);

#endif
