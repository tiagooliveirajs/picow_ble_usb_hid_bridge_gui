#include <stdbool.h>
#include <stdint.h>

#include "bsp/board_api.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"

#include "bridge_bus.h"
#include "bt_runtime.h"
#include "storage_owner.h"
#include "usb_hid.h"

#define CORE1_STACK_BYTES 8192u

_Static_assert(CORE1_STACK_BYTES >= 8192u, "Classic/dual-mode BT runtime requires at least the accepted 8 KiB Core1 stack");

static uint32_t g_core1_stack[CORE1_STACK_BYTES / sizeof(uint32_t)]
    __attribute__((aligned(8)));

static void application_service(void) {
    bridge_message_t message;

    if (bridge_bus_take_release_required()) {
        usb_hid_release_all();
    }

    while (bridge_bus_take_bt_event(&message)) {
        switch (message.type) {
            case BT_EVENT_STACK_WORKING:
            case BT_EVENT_COMMAND_ACK:
            case BT_EVENT_STACK_ERROR:
            default:
                // FORK-01 only establishes the transport-neutral projection
                // boundary. Product state/UI behavior is introduced later.
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

    // Exercise the bounded Core0 -> Core1 command path without granting the
    // application direct access to BTstack. The response is a status event.
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
