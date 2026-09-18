#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "../bridge_bus.h"

static bridge_message_t make_message(uint16_t type, canonical_source_t source) {
    bridge_message_t message = {
        .channel = BRIDGE_CHANNEL_INPUT,
        .type = type,
        .length = CANONICAL_INPUT_SOURCE_PREFIX_SIZE,
    };
    canonical_source_write_input_prefix(message.payload, source);
    return message;
}

static void fill_bt_queue(void) {
    const canonical_source_t source = canonical_source_make(
        CANONICAL_SOURCE_KIND_CLASSIC_KEYBOARD, 0u);
    for (uint16_t i = 0u; i < BRIDGE_QUEUE_CAPACITY; ++i) {
        bridge_message_t message = make_message(i, source);
        assert(bridge_bus_publish_bt_event(&message, false));
    }
}

int main(void) {
    bridge_bus_init();

    const canonical_source_t classic = canonical_source_make(
        CANONICAL_SOURCE_KIND_CLASSIC_KEYBOARD, 0u);
    bridge_message_t out = make_message(10u, classic);
    bridge_message_t in = {0};
    assert(bridge_bus_publish_bt_event(&out, false));
    assert(bridge_bus_take_bt_event(&in));
    assert(in.type == 10u && in.length == CANONICAL_INPUT_SOURCE_PREFIX_SIZE);
    assert(canonical_source_equal(
        canonical_source_from_input_prefix(in.payload), classic));
    assert(!bridge_bus_take_bt_event(&in));

    /* Failed publications preserve exact kind + instance identities. */
    bridge_bus_init();
    fill_bt_queue();
    const canonical_source_t ble10 = canonical_source_make(
        CANONICAL_SOURCE_KIND_BLE_HOGP_KEYBOARD, 10u);
    const canonical_source_t ble11 = canonical_source_make(
        CANONICAL_SOURCE_KIND_BLE_HOGP_KEYBOARD, 11u);
    out = make_message(0xEEEEu, ble10);
    assert(!bridge_bus_publish_bt_event(&out, true));
    out = make_message(0xEEEFu, ble11);
    assert(!bridge_bus_publish_bt_event(&out, true));
    out = make_message(0xEEF0u, ble10);
    assert(!bridge_bus_publish_bt_event(&out, true));
    assert(bridge_bus_take_bt_overflow());
    assert(!bridge_bus_take_bt_overflow());

    canonical_source_t dropped[BRIDGE_RELEASE_SOURCE_CAPACITY];
    const uint8_t count = bridge_bus_take_release_sources(
        dropped, BRIDGE_RELEASE_SOURCE_CAPACITY);
    assert(count == 2u);
    assert(canonical_source_equal(dropped[0], ble10));
    assert(canonical_source_equal(dropped[1], ble11));
    assert(bridge_bus_take_release_sources(
        dropped, BRIDGE_RELEASE_SOURCE_CAPACITY) == 0u);
    assert(!bridge_bus_take_release_required());

    bridge_bus_init();
    out = make_message(0x1234u, classic);
    out.channel = BRIDGE_CHANNEL_CONTROL;
    assert(bridge_bus_send_app_command(&out));
    assert(bridge_bus_take_app_command(&in));
    assert(in.channel == BRIDGE_CHANNEL_CONTROL && in.type == 0x1234u);

    bridge_bus_init();
    for (uint16_t i = 0u; i < BRIDGE_QUEUE_CAPACITY; ++i) {
        out = make_message(i, classic);
        out.channel = BRIDGE_CHANNEL_CONTROL;
        assert(bridge_bus_send_app_command(&out));
    }
    out = make_message(0xFFFFu, classic);
    out.channel = BRIDGE_CHANNEL_CONTROL;
    assert(!bridge_bus_send_app_command(&out));
    assert(bridge_bus_take_app_overflow());
    assert(!bridge_bus_take_app_overflow());

    /* If more distinct release identities are dropped than the bounded latch
     * can retain, explicit global release fallback is raised. */
    bridge_bus_init();
    fill_bt_queue();
    for (uint16_t instance = 0u;
         instance < BRIDGE_RELEASE_SOURCE_CAPACITY;
         ++instance) {
        const canonical_source_t source = canonical_source_make(
            CANONICAL_SOURCE_KIND_BLE_HOGP_KEYBOARD, instance);
        out = make_message((uint16_t)(0x9000u + instance), source);
        assert(!bridge_bus_publish_bt_event(&out, true));
    }
    const canonical_source_t seventeenth = canonical_source_make(
        CANONICAL_SOURCE_KIND_BLE_HOGP_KEYBOARD,
        BRIDGE_RELEASE_SOURCE_CAPACITY);
    out = make_message(0x9fffu, seventeenth);
    assert(!bridge_bus_publish_bt_event(&out, true));
    assert(bridge_bus_take_release_required());

    /* Invalid/absent source prefix also uses explicit global fallback. */
    bridge_bus_init();
    fill_bt_queue();
    memset(&out, 0, sizeof(out));
    out.channel = BRIDGE_CHANNEL_INPUT;
    out.type = 0x9999u;
    out.length = 0u;
    assert(!bridge_bus_publish_bt_event(&out, true));
    assert(bridge_bus_take_release_required());

    bridge_bus_init();
    memset(&out, 0, sizeof(out));
    out.length = BRIDGE_MESSAGE_PAYLOAD_SIZE + 1u;
    assert(!bridge_bus_send_app_command(&out));
    assert(!bridge_bus_publish_bt_event(&out, true));
    assert(!bridge_bus_take_release_required());

    return 0;
}
