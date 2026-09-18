#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "../keyboard_report_queue.h"

static usb_keyboard_report_t report(uint8_t modifiers, uint8_t keycode) {
    usb_keyboard_report_t value = {
        .modifiers = modifiers,
        .keycodes = {keycode, 0u, 0u, 0u, 0u, 0u},
    };
    return value;
}

int main(void) {
    keyboard_report_queue_t queue;
    keyboard_report_queue_init(&queue);
    assert(keyboard_report_queue_count(&queue) == 0u);

    const usb_keyboard_report_t press = report(0u, 0x04u);
    const usb_keyboard_report_t release = report(0u, 0u);
    assert(keyboard_report_queue_push(&queue, &press));
    assert(keyboard_report_queue_push(&queue, &release));
    assert(keyboard_report_queue_count(&queue) == 2u);

    usb_keyboard_report_t observed;
    assert(keyboard_report_queue_peek(&queue, &observed));
    assert(memcmp(&observed, &press, sizeof(observed)) == 0);
    assert(keyboard_report_queue_drop(&queue));
    assert(keyboard_report_queue_peek(&queue, &observed));
    assert(memcmp(&observed, &release, sizeof(observed)) == 0);
    assert(keyboard_report_queue_drop(&queue));
    assert(!keyboard_report_queue_peek(&queue, &observed));

    keyboard_report_queue_init(&queue);
    for (uint8_t i = 0u; i < USB_KEYBOARD_QUEUE_CAPACITY; ++i) {
        const usb_keyboard_report_t value = report(0u, (uint8_t)(0x04u + i));
        assert(keyboard_report_queue_push(&queue, &value));
    }
    assert(keyboard_report_queue_count(&queue) == USB_KEYBOARD_QUEUE_CAPACITY);
    assert(!keyboard_report_queue_push(&queue, &press));
    assert(keyboard_report_queue_count(&queue) == USB_KEYBOARD_QUEUE_CAPACITY);

    /* Overflow recovery replaces stale queued transitions with the exact
     * aggregate state that remains after one canonical source is torn down. */
    const usb_keyboard_report_t survivor = report(0x02u, 0x29u);
    keyboard_report_queue_replace_with(&queue, &survivor);
    assert(keyboard_report_queue_count(&queue) == 1u);
    assert(keyboard_report_queue_peek(&queue, &observed));
    assert(memcmp(&observed, &survivor, sizeof(observed)) == 0);

    return 0;
}
