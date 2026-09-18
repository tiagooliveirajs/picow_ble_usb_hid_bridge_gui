#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "bsp/board_api.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"

#include "bridge_bus.h"
#include "bt_runtime.h"
#include "keyboard_input.h"
#include "storage_owner.h"
#include "usb_hid.h"

#define CORE1_STACK_BYTES 8192u

_Static_assert(CORE1_STACK_BYTES >= 8192u, "Classic/dual-mode BT runtime requires at least the accepted 8 KiB Core1 stack");

static uint32_t g_core1_stack[CORE1_STACK_BYTES / sizeof(uint32_t)]
    __attribute__((aligned(8)));

static void forward_keyboard_snapshot(const bridge_message_t *message) {
    if (message->length != sizeof(keyboard_input_snapshot_t)) return;

    keyboard_input_snapshot_t snapshot;
    memcpy(&snapshot, message->payload, sizeof(snapshot));

    switch ((keyboard_input_source_t)snapshot.source) {
        case KEYBOARD_SOURCE_CLASSIC_HID:
        case KEYBOARD_SOURCE_BLE_HOGP:
        case KEYBOARD_SOURCE_BLE_COMPOSITE:
        case KEYBOARD_SOURCE_SYNTHETIC:
            break;
        default:
            return;
    }

    usb_keyboard_report_t report = {
        .modifiers = snapshot.modifiers,
        .reserved = 0u,
    };
    memcpy(report.keycodes, snapshot.keycodes, sizeof(report.keycodes));

    if (!usb_hid_submit_keyboard(&report)) {
        usb_hid_release_keyboard();
    }
}

static void application_service(void) {
    bridge_message_t message;

    if (bridge_bus_take_release_required()) {
        usb_hid_release_all();
    }

    while (bridge_bus_take_bt_event(&message)) {
        if (message.channel == BRIDGE_CHANNEL_INPUT &&
            message.type == BT_EVENT_KEYBOARD_SNAPSHOT) {
            forward_keyboard_snapshot(&message);
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
                // UI projections are introduced later; status events already
                // cross the transport-neutral boundary without raw BT calls.
                break;
        }
    }
}

int main(void) {
    board_init();
    bridge_bus_init();

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
