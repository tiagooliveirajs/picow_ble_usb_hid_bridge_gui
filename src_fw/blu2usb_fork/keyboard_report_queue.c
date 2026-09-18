#include "keyboard_report_queue.h"

#include <string.h>

void keyboard_report_queue_init(keyboard_report_queue_t *queue) {
    if (queue == NULL) return;
    memset(queue, 0, sizeof(*queue));
}

bool keyboard_report_queue_push(
    keyboard_report_queue_t *queue,
    const usb_keyboard_report_t *report) {
    if (queue == NULL || report == NULL ||
        queue->count >= USB_KEYBOARD_QUEUE_CAPACITY) {
        return false;
    }

    queue->entries[queue->tail] = *report;
    queue->tail = (uint8_t)((queue->tail + 1u) % USB_KEYBOARD_QUEUE_CAPACITY);
    ++queue->count;
    return true;
}

bool keyboard_report_queue_peek(
    const keyboard_report_queue_t *queue,
    usb_keyboard_report_t *report) {
    if (queue == NULL || report == NULL || queue->count == 0u) return false;
    *report = queue->entries[queue->head];
    return true;
}

bool keyboard_report_queue_drop(keyboard_report_queue_t *queue) {
    if (queue == NULL || queue->count == 0u) return false;
    queue->head = (uint8_t)((queue->head + 1u) % USB_KEYBOARD_QUEUE_CAPACITY);
    --queue->count;
    return true;
}

void keyboard_report_queue_replace_with(
    keyboard_report_queue_t *queue,
    const usb_keyboard_report_t *report) {
    if (queue == NULL || report == NULL) return;
    keyboard_report_queue_init(queue);
    (void)keyboard_report_queue_push(queue, report);
}

uint8_t keyboard_report_queue_count(const keyboard_report_queue_t *queue) {
    return queue == NULL ? 0u : queue->count;
}
