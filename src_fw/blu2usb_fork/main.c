#include <limits.h>
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
#include "ui_runtime.h"
#include "usb_hid.h"

#define CORE1_STACK_BYTES 8192u

_Static_assert(CORE1_STACK_BYTES >= 8192u,
               "Classic/dual-mode BT runtime requires at least the accepted 8 KiB Core1 stack");
_Static_assert(CANONICAL_SOURCE_CAPACITY >= 16u,
               "G06 canonical ownership capacity must not regress below 16 sources");

static uint32_t g_core1_stack[CORE1_STACK_BYTES / sizeof(uint32_t)] __attribute__((aligned(8)));
static canonical_hid_state_t g_canonical_hid;
static uint8_t g_last_mouse_buttons;
static bool g_last_mouse_valid;

static canonical_source_t ble_mouse_source(void) {
    return canonical_source_make(CANONICAL_SOURCE_KIND_BLE_HOGP_MOUSE, 1u);
}

static usb_keyboard_report_t to_usb_keyboard_report(const canonical_keyboard_report_t *canonical) {
    usb_keyboard_report_t report = {.modifiers = canonical->modifiers, .reserved = 0u};
    memcpy(report.keycodes, canonical->keycodes, sizeof(report.keycodes));
    return report;
}

static void recover_source(canonical_source_t source) {
    canonical_keyboard_report_t canonical_report;
    bool keyboard_changed = false;
    if (!canonical_hid_release_source(&g_canonical_hid, source,
                                      &canonical_report, &keyboard_changed)) return;

    if (canonical_source_kind_is_keyboard(source.kind) && keyboard_changed) {
        const usb_keyboard_report_t report = to_usb_keyboard_report(&canonical_report);
        usb_hid_replace_keyboard_state(&report);
    }
    if (canonical_source_kind_is_mouse(source.kind)) g_last_mouse_valid = false;
}

static void apply_keyboard_snapshot(const bridge_message_t *message) {
    if (message->length != sizeof(keyboard_input_snapshot_t)) return;
    keyboard_input_snapshot_t snapshot;
    memcpy(&snapshot, message->payload, sizeof(snapshot));
    const canonical_source_t source = keyboard_input_snapshot_source(&snapshot);
    canonical_keyboard_report_t canonical_report;
    bool changed = false;
    if (!canonical_hid_apply_keyboard_snapshot(&g_canonical_hid, &snapshot,
                                                &canonical_report, &changed) || !changed)
        return;
    const usb_keyboard_report_t report = to_usb_keyboard_report(&canonical_report);
    if (usb_hid_submit_keyboard(&report)) return;
    recover_source(source);
}

static void apply_mouse_event(const bridge_message_t *message) {
    if (message->length != sizeof(canonical_mouse_event_t)) return;
    canonical_mouse_event_t event;
    memcpy(&event, message->payload, sizeof(event));
    if (!canonical_hid_apply_mouse(&g_canonical_hid, &event)) return;
}

static void apply_release_source(const bridge_message_t *message) {
    if (message->length < CANONICAL_INPUT_SOURCE_PREFIX_SIZE) return;
    recover_source(canonical_source_from_input_prefix(message->payload));
}

static void recover_dropped_bt_sources(void) {
    canonical_source_t dropped[BRIDGE_RELEASE_SOURCE_CAPACITY];
    const uint8_t dropped_count = bridge_bus_take_release_sources(
        dropped, BRIDGE_RELEASE_SOURCE_CAPACITY);
    for (uint8_t i = 0u; i < dropped_count; ++i) recover_source(dropped[i]);

    (void)bridge_bus_take_bt_overflow();
    if (bridge_bus_take_release_required()) {
        canonical_keyboard_report_t neutral;
        canonical_hid_release_all(&g_canonical_hid, &neutral);
        usb_hid_release_all();
        g_last_mouse_valid = false;
    }
}

static void application_service(void) {
    bridge_message_t message;
    while (bridge_bus_take_bt_event(&message)) {
        if (message.channel == BRIDGE_CHANNEL_INPUT) {
            if (message.type == BT_EVENT_KEYBOARD_SNAPSHOT) {
                apply_keyboard_snapshot(&message);
                continue;
            }
            if (message.type == BT_EVENT_BLE_MOUSE_EVENT) {
                apply_mouse_event(&message);
                continue;
            }
            if (message.type == BT_EVENT_BLE_MOUSE_RELEASE_SOURCE) {
                apply_release_source(&message);
                continue;
            }
        }

        ui_runtime_on_bt_event(message.type);
        if (message.type == BT_EVENT_BLE_MOUSE_DISCONNECTED)
            recover_source(ble_mouse_source());
    }
    recover_dropped_bt_sources();
}

static int8_t clamp_i8(int32_t value) {
    if (value > INT8_MAX) return INT8_MAX;
    if (value < INT8_MIN) return INT8_MIN;
    return (int8_t)value;
}

static void service_usb_mouse(void) {
    canonical_hid_output_state_t output;
    canonical_hid_snapshot(&g_canonical_hid, &output);

    const int8_t dx = clamp_i8(output.dx);
    const int8_t dy = clamp_i8(output.dy);
    const int8_t wheel = clamp_i8(output.wheel_vertical);
    const int8_t pan = clamp_i8(output.wheel_horizontal);
    const bool relative = dx != 0 || dy != 0 || wheel != 0 || pan != 0;
    const bool buttons_changed = !g_last_mouse_valid || output.mouse_buttons != g_last_mouse_buttons;
    if (!relative && !buttons_changed) return;

    const usb_mouse_report_t report = {
        .buttons = output.mouse_buttons,
        .x = dx,
        .y = dy,
        .wheel = wheel,
        .pan = pan,
    };
    if (!usb_hid_submit_mouse(&report)) return;

    (void)canonical_hid_consume_relative(&g_canonical_hid, dx, dy, wheel, pan);
    g_last_mouse_buttons = output.mouse_buttons;
    g_last_mouse_valid = true;
}

int main(void) {
    board_init();
    bridge_bus_init();
    canonical_hid_init(&g_canonical_hid);
    g_last_mouse_buttons = 0u;
    g_last_mouse_valid = false;

    if (!storage_owner_init_core0()) while (true) tight_loop_contents();
    if (!usb_hid_init()) while (true) tight_loop_contents();
    if (!ui_runtime_init()) {
        while (true) {
            usb_hid_task();
            tight_loop_contents();
        }
    }

    multicore_launch_core1_with_stack(bt_runtime_core1_main,
                                      g_core1_stack, sizeof(g_core1_stack));
    const bridge_message_t ping = {
        .channel = BRIDGE_CHANNEL_CONTROL,
        .type = BT_COMMAND_PING,
        .length = 0u,
    };
    (void)bridge_bus_send_app_command(&ping);

    while (true) {
        usb_hid_task();
        application_service();
        service_usb_mouse();
        ui_runtime_task();
        tight_loop_contents();
    }
}
