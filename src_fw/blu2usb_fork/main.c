#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "bsp/board_api.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"

#include "bridge_bus.h"
#include "bt_runtime.h"
#include "canonical_hid.h"
#include "canonical_source.h"
#include "keyboard_input.h"
#include "storage_owner.h"
#include "usb_hid.h"

#define CORE1_STACK_BYTES 8192u

_Static_assert(CORE1_STACK_BYTES >= 8192u, "Classic/dual-mode BT runtime requires at least the accepted 8 KiB Core1 stack");
_Static_assert(CANONICAL_SOURCE_CAPACITY >= 16u, "G06 canonical ownership capacity must not regress below 16 sources");

static uint32_t g_core1_stack[CORE1_STACK_BYTES / sizeof(uint32_t)]
    __attribute__((aligned(8)));
static canonical_hid_state_t g_canonical_hid;

static usb_keyboard_report_t to_usb_keyboard_report(
    const canonical_keyboard_report_t *canonical) {
    usb_keyboard_report_t report = {
        .modifiers = canonical->modifiers,
        .reserved = 0u,
    };
    memcpy(report.keycodes, canonical->keycodes, sizeof(report.keycodes));
    return report;
}

static void recover_keyboard_source(uint8_t source) {
    canonical_keyboard_report_t canonical_report;
    bool changed = false;
    if (!canonical_hid_release_source(
            &g_canonical_hid, source, &canonical_report, &changed)) {
        return;
    }
    (void)changed;

    if (canonical_source_is_keyboard(source)) {
        const usb_keyboard_report_t report =
            to_usb_keyboard_report(&canonical_report);
        usb_hid_replace_keyboard_state(&report);
    }
}

static void apply_keyboard_snapshot(const bridge_message_t *message) {
    if (message->length != sizeof(keyboard_input_snapshot_t)) return;

    keyboard_input_snapshot_t snapshot;
    memcpy(&snapshot, message->payload, sizeof(snapshot));

    canonical_keyboard_report_t canonical_report;
    bool changed = false;
    if (!canonical_hid_apply_keyboard_snapshot(
            &g_canonical_hid, &snapshot, &canonical_report, &changed) ||
        !changed) {
        return;
    }

    const usb_keyboard_report_t report =
        to_usb_keyboard_report(&canonical_report);
    if (usb_hid_submit_keyboard(&report)) return;

    /* The USB queue lost ordering capacity for this source. Remove exactly the
     * source that triggered the failed publication and replace pending USB
     * reports with the aggregate state still owned by all other sources. */
    recover_keyboard_source(snapshot.source);
}

static void recover_dropped_bt_sources(void) {
    const uint32_t release_sources = bridge_bus_take_release_sources();
    bool keyboard_recovery_required = false;

    for (uint8_t source = 1u; source <= CANONICAL_SOURCE_CAPACITY; ++source) {
        if ((release_sources & (UINT32_C(1) << source)) == 0u) continue;

        canonical_keyboard_report_t ignored_report;
        bool ignored_changed = false;
        (void)canonical_hid_release_source(
            &g_canonical_hid, source, &ignored_report, &ignored_changed);
        if (canonical_source_is_keyboard(source)) {
            keyboard_recovery_required = true;
        }
    }

    if (keyboard_recovery_required) {
        const canonical_keyboard_report_t *current =
            canonical_hid_keyboard_report(&g_canonical_hid);
        const usb_keyboard_report_t report = to_usb_keyboard_report(current);
        usb_hid_replace_keyboard_state(&report);
    }

    (void)bridge_bus_take_bt_overflow();

    if (bridge_bus_take_release_required()) {
        canonical_keyboard_report_t neutral;
        canonical_hid_release_all(&g_canonical_hid, &neutral);
        usb_hid_release_all();
    }
}

static void application_service(void) {
    bridge_message_t message;

    while (bridge_bus_take_bt_event(&message)) {
        if (message.channel == BRIDGE_CHANNEL_INPUT &&
            message.type == BT_EVENT_KEYBOARD_SNAPSHOT) {
            apply_keyboard_snapshot(&message);
            continue;
        }

        switch (message.type) {
            case BT_EVENT_STACK_WORKING:
            case BT_EVENT_COMMAND_ACK:
            case BT_EVENT_STACK_ERROR:
            case BT_EVENT_CLASSIC_SCANNING:
            case BT_EVENT_CLASSIC_BONDING:
            case BT_EVENT_CLASSIC_CONNECTING:
            case BT_EVENT_CLASSIC_READY:
            case BT_EVENT_CLASSIC_RETRYING:
            case BT_EVENT_CLASSIC_CANCELLED:
            default:
                break;
        }
    }

    recover_dropped_bt_sources();
}

int main(void) {
    board_init();
    bridge_bus_init();
    canonical_hid_init(&g_canonical_hid);

    if (!storage_owner_init_core0()) {
        while (true) tight_loop_contents();
    }

    if (!usb_hid_init()) {
        while (true) tight_loop_contents();
    }

    multicore_launch_core1_with_stack(
        bt_runtime_core1_main,
        g_core1_stack,
        sizeof(g_core1_stack));

    const bridge_message_t ping = {
        .channel = BRIDGE_CHANNEL_CONTROL,
        .type = BT_COMMAND_PING,
        .length = 0u,
    };
    (void)bridge_bus_send_app_command(&ping);

    while (true) {
        usb_hid_task();
        application_service();
        tight_loop_contents();
    }
}
