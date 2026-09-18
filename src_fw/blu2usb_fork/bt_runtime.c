#include "bt_runtime.h"

#include <stdbool.h>

#include "btstack.h"
#include "pico/cyw43_arch.h"
#include "pico/flash.h"
#include "pico/stdlib.h"

#include "bridge_bus.h"
#include "classic_keyboard.h"

#define COMMAND_POLL_INTERVAL_MS 10u

static btstack_packet_callback_registration_t g_hci_event_callback;
static btstack_timer_source_t g_command_timer;

static void publish_status(uint16_t type) {
    const bridge_message_t message = {
        .channel = BRIDGE_CHANNEL_STATUS,
        .type = type,
        .length = 0u,
    };
    (void)bridge_bus_publish_bt_event(&message, false);
}

static void command_poll(btstack_timer_source_t *timer) {
    bridge_message_t message;

    while (bridge_bus_take_app_command(&message)) {
        if (message.channel != BRIDGE_CHANNEL_CONTROL) continue;

        switch (message.type) {
            case BT_COMMAND_PING:
                publish_status(BT_EVENT_COMMAND_ACK);
                break;
            case BT_COMMAND_CLASSIC_CANCEL:
            case BT_COMMAND_CLASSIC_RETRY:
                classic_keyboard_handle_command(message.type);
                break;
            default:
                break;
        }
    }

    btstack_run_loop_set_timer(timer, COMMAND_POLL_INTERVAL_MS);
    btstack_run_loop_add_timer(timer);
}

static void packet_handler(
    uint8_t packet_type,
    uint16_t channel,
    uint8_t *packet,
    uint16_t size) {
    (void)channel;
    (void)size;

    if (packet_type != HCI_EVENT_PACKET) return;

    if (hci_event_packet_get_type(packet) == BTSTACK_EVENT_STATE &&
        btstack_event_state_get_state(packet) == HCI_STATE_WORKING) {
        publish_status(BT_EVENT_STACK_WORKING);
        classic_keyboard_on_stack_working();
    }
}

static void runtime_init(void) {
    // One dual-mode stack owner on Core1. Profile adapters register with this
    // lifecycle; they never initialize CYW43, L2CAP or HCI power themselves.
    l2cap_init();
    sm_init();
    gatt_client_init();
    sdp_init();

    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    sm_set_authentication_requirements(SM_AUTHREQ_BONDING);

    gap_set_bondable_mode(1);
    gap_ssp_set_io_capability(SSP_IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    gap_ssp_set_authentication_requirement(
        SSP_IO_AUTHREQ_MITM_PROTECTION_NOT_REQUIRED_GENERAL_BONDING);
    gap_ssp_set_auto_accept(1);
    gap_set_local_name("BLU2USB Mouse + Keyboard 00:00:00:00:00:00");

    classic_keyboard_init();

    g_hci_event_callback.callback = packet_handler;
    hci_add_event_handler(&g_hci_event_callback);

    btstack_run_loop_set_timer_handler(&g_command_timer, command_poll);
    btstack_run_loop_set_timer(&g_command_timer, COMMAND_POLL_INTERVAL_MS);
    btstack_run_loop_add_timer(&g_command_timer);

    hci_power_control(HCI_POWER_ON);
}

void bt_runtime_core1_main(void) {
    flash_safe_execute_core_init();

    if (cyw43_arch_init() != PICO_OK) {
        publish_status(BT_EVENT_STACK_ERROR);
        while (true) tight_loop_contents();
    }

    runtime_init();
    btstack_run_loop_execute();

    publish_status(BT_EVENT_STACK_ERROR);
    while (true) tight_loop_contents();
}
