#include <stdbool.h>
#include <stdint.h>

#include "bsp/board_api.h"
#include "pico/flash.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "tusb.h"

#include "classic_hid.h"
#include "keyboard_queue.h"
#include "poc_log.h"

#define CORE1_STACK_BYTES 8192u

static uint32_t g_core1_stack[CORE1_STACK_BYTES / sizeof(uint32_t)]
    __attribute__((aligned(8)));

static void usb_hid_task(void) {
    static uint8_t report[POC_USB_KEYBOARD_REPORT_LEN];

    if (!keyboard_queue_peek(report)) return;

    if (tud_suspended()) {
        tud_remote_wakeup();
        return;
    }

    if (!tud_hid_ready()) return;

    if (tud_hid_report(0, report, sizeof(report))) {
        keyboard_queue_advance();
    }
}

static void cdc_rx_drain_task(void) {
    while (tud_cdc_available()) {
        (void)tud_cdc_read_char();
    }
}

int main(void) {
    board_init();
    poc_log_init();
    keyboard_queue_init();

    tud_init(BOARD_TUD_RHPORT);
    if (board_init_after_tusb) {
        board_init_after_tusb();
    }

    poc_logf("BOOT: RP2350 BKB-3G Classic HID -> USB HID POC");
    poc_logf("USB: composite CDC ACM debug console + fixed HID keyboard");
    poc_logf("CDC: open /dev/ttyACM* to receive logs; queued boot logs will flush");
    poc_logf("BT: Bluetooth Classic HID Host, target BKB-3G / Bluetooth keyboard 3.0");

    // BTstack's CYW43 integration persists Classic link keys through its TLV
    // flash bank. Register Core 0 before Core 1 can store a newly bonded key,
    // otherwise a flash-safe operation may collide with TinyUSB execution.
    flash_safe_execute_core_init();
    poc_logf("FLASH: Core0 registered for BTstack TLV link-key writes");

    multicore_launch_core1_with_stack(
        classic_hid_core_main,
        g_core1_stack,
        sizeof(g_core1_stack));

    poc_logf("CORE0: TinyUSB task loop started");

    while (true) {
        tud_task();
        poc_log_usb_task();
        usb_hid_task();
        cdc_rx_drain_task();
        tight_loop_contents();
    }
}

void tud_mount_cb(void) {
    poc_logf("USB: mounted");
}

void tud_umount_cb(void) {
    poc_logf("USB: unmounted");
}

void tud_suspend_cb(bool remote_wakeup_en) {
    poc_logf("USB: suspended remote_wakeup=%u", remote_wakeup_en ? 1u : 0u);
}

void tud_resume_cb(void) {
    poc_logf("USB: resumed");
}

void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts) {
    (void)itf;
    (void)rts;
    if (dtr) {
        poc_logf("CDC: terminal connected");
    }
}

void tud_hid_report_complete_cb(uint8_t instance,
                                uint8_t const *report,
                                uint16_t len) {
    (void)instance;
    (void)report;
    (void)len;
}

uint16_t tud_hid_get_report_cb(uint8_t instance,
                               uint8_t report_id,
                               hid_report_type_t report_type,
                               uint8_t *buffer,
                               uint16_t reqlen) {
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance,
                           uint8_t report_id,
                           hid_report_type_t report_type,
                           uint8_t const *buffer,
                           uint16_t bufsize) {
    (void)instance;
    (void)report_id;

    // Keyboard LED output (Caps/Num/Scroll Lock) is deliberately not forwarded
    // to Bluetooth in this POC, but logging it confirms USB OUT traffic.
    if (report_type == HID_REPORT_TYPE_OUTPUT && buffer != NULL && bufsize != 0u) {
        poc_log_hex("USB HID output report", buffer, bufsize);
    }
}
