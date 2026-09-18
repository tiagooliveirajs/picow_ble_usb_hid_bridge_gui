#include "keyboard_queue.h"

#include <string.h>

#include "pico/critical_section.h"

#define KEYBOARD_QUEUE_CAPACITY 16u

static critical_section_t g_lock;
static uint8_t g_reports[KEYBOARD_QUEUE_CAPACITY][POC_USB_KEYBOARD_REPORT_LEN];
static uint8_t g_head;
static uint8_t g_tail;

static uint8_t next_index(uint8_t value) {
    return (uint8_t)((value + 1u) % KEYBOARD_QUEUE_CAPACITY);
}

void keyboard_queue_init(void) {
    critical_section_init(&g_lock);
    g_head = 0u;
    g_tail = 0u;
    memset(g_reports, 0, sizeof(g_reports));
}

void keyboard_queue_clear(void) {
    critical_section_enter_blocking(&g_lock);
    g_head = 0u;
    g_tail = 0u;
    critical_section_exit(&g_lock);
}

void keyboard_queue_push(const uint8_t report[POC_USB_KEYBOARD_REPORT_LEN]) {
    critical_section_enter_blocking(&g_lock);

    uint8_t next = next_index(g_tail);
    if (next == g_head) {
        // Keep the newest state. Losing a key-up forever is worse than dropping
        // an intermediate key state while the USB host is busy.
        g_head = next_index(g_head);
    }

    memcpy(g_reports[g_tail], report, POC_USB_KEYBOARD_REPORT_LEN);
    g_tail = next;

    critical_section_exit(&g_lock);
}

bool keyboard_queue_peek(uint8_t report[POC_USB_KEYBOARD_REPORT_LEN]) {
    bool available = false;

    critical_section_enter_blocking(&g_lock);
    if (g_head != g_tail) {
        memcpy(report, g_reports[g_head], POC_USB_KEYBOARD_REPORT_LEN);
        available = true;
    }
    critical_section_exit(&g_lock);

    return available;
}

void keyboard_queue_advance(void) {
    critical_section_enter_blocking(&g_lock);
    if (g_head != g_tail) {
        g_head = next_index(g_head);
    }
    critical_section_exit(&g_lock);
}
