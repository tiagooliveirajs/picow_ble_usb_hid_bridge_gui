#include "poc_log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "pico/critical_section.h"
#include "pico/stdlib.h"
#include "tusb.h"

#define LOG_LINE_COUNT 96u
#define LOG_LINE_SIZE 224u

typedef struct {
    uint16_t len;
    char data[LOG_LINE_SIZE];
} log_line_t;

static critical_section_t g_log_lock;
static log_line_t g_lines[LOG_LINE_COUNT];
static uint16_t g_head;
static uint16_t g_tail;
static uint32_t g_dropped;

static uint16_t next_index(uint16_t value) {
    return (uint16_t)((value + 1u) % LOG_LINE_COUNT);
}

static void enqueue_line(const char *text, size_t len) {
    if (text == NULL || len == 0u) return;
    if (len >= LOG_LINE_SIZE) len = LOG_LINE_SIZE - 1u;

    critical_section_enter_blocking(&g_log_lock);

    uint16_t next = next_index(g_tail);
    if (next == g_head) {
        g_head = next_index(g_head);
        ++g_dropped;
    }

    memcpy(g_lines[g_tail].data, text, len);
    g_lines[g_tail].data[len] = '\0';
    g_lines[g_tail].len = (uint16_t)len;
    g_tail = next;

    critical_section_exit(&g_log_lock);
}

void poc_log_init(void) {
    critical_section_init(&g_log_lock);
    g_head = 0u;
    g_tail = 0u;
    g_dropped = 0u;
    memset(g_lines, 0, sizeof(g_lines));
}

void poc_logf(const char *format, ...) {
    char body[LOG_LINE_SIZE - 32u];
    char line[LOG_LINE_SIZE];

    va_list args;
    va_start(args, format);
    vsnprintf(body, sizeof(body), format, args);
    va_end(args);

    const uint32_t ms = to_ms_since_boot(get_absolute_time());
    int written = snprintf(line, sizeof(line), "[%010lu] %s\r\n",
                           (unsigned long)ms, body);
    if (written <= 0) return;

    size_t len = (size_t)written;
    if (len >= sizeof(line)) len = sizeof(line) - 1u;
    enqueue_line(line, len);
}

void poc_log_hex(const char *label, const uint8_t *data, size_t len) {
    if (label != NULL) {
        poc_logf("%s (%u bytes)", label, (unsigned)len);
    }
    if (data == NULL) return;

    for (size_t offset = 0; offset < len; offset += 16u) {
        char body[96];
        size_t used = (size_t)snprintf(body, sizeof(body), "  %04x:", (unsigned)offset);
        const size_t chunk = (len - offset > 16u) ? 16u : len - offset;

        for (size_t i = 0; i < chunk && used + 4u < sizeof(body); ++i) {
            int n = snprintf(&body[used], sizeof(body) - used, " %02x", data[offset + i]);
            if (n <= 0) break;
            used += (size_t)n;
        }

        poc_logf("%s", body);
    }
}

void poc_log_usb_task(void) {
    if (!tud_cdc_connected()) return;

    uint32_t dropped = 0u;
    critical_section_enter_blocking(&g_log_lock);
    if (g_dropped != 0u) {
        dropped = g_dropped;
        g_dropped = 0u;
    }
    critical_section_exit(&g_log_lock);

    if (dropped != 0u) {
        char warning[96];
        int n = snprintf(warning, sizeof(warning),
                         "[LOG] %lu older log lines were dropped\r\n",
                         (unsigned long)dropped);
        if (n > 0 && tud_cdc_write_available() >= (uint32_t)n) {
            tud_cdc_write(warning, (uint32_t)n);
        } else {
            critical_section_enter_blocking(&g_log_lock);
            g_dropped += dropped;
            critical_section_exit(&g_log_lock);
        }
    }

    while (true) {
        log_line_t line;
        bool have_line = false;

        critical_section_enter_blocking(&g_log_lock);
        if (g_head != g_tail) {
            line = g_lines[g_head];
            if (tud_cdc_write_available() >= line.len) {
                g_head = next_index(g_head);
                have_line = true;
            }
        }
        critical_section_exit(&g_log_lock);

        if (!have_line) break;

        tud_cdc_write(line.data, line.len);
    }

    tud_cdc_write_flush();
}
