#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "../bridge_bus.h"

static bridge_message_t make_message(uint16_t type, uint8_t marker) {
    bridge_message_t message = {
        .channel = BRIDGE_CHANNEL_INPUT,
        .type = type,
        .length = 1u,
    };
    message.payload[0] = marker;
    return message;
}

int main(void) {
    bridge_bus_init();

    bridge_message_t out = make_message(10u, 0x11u);
    bridge_message_t in = {0};
    assert(bridge_bus_publish_bt_event(&out, false));
    assert(bridge_bus_take_bt_event(&in));
    assert(in.type == 10u && in.length == 1u && in.payload[0] == 0x11u);
    assert(!bridge_bus_take_bt_event(&in));

    bridge_bus_init();
    for (uint16_t i = 0u; i < BRIDGE_QUEUE_CAPACITY; ++i) {
        bridge_message_t message = make_message(i, (uint8_t)i);
        assert(bridge_bus_publish_bt_event(&message, true));
    }
    out = make_message(0xEEEEu, 0xEEu);
    assert(!bridge_bus_publish_bt_event(&out, true));
    assert(bridge_bus_take_bt_overflow());
    assert(!bridge_bus_take_bt_overflow());
    assert(bridge_bus_take_release_required());
    assert(!bridge_bus_take_release_required());

    bridge_bus_init();
    out = make_message(0x1234u, 0x22u);
    out.channel = BRIDGE_CHANNEL_CONTROL;
    assert(bridge_bus_send_app_command(&out));
    assert(bridge_bus_take_app_command(&in));
    assert(in.channel == BRIDGE_CHANNEL_CONTROL && in.type == 0x1234u);

    bridge_bus_init();
    for (uint16_t i = 0u; i < BRIDGE_QUEUE_CAPACITY; ++i) {
        out = make_message(i, (uint8_t)i);
        out.channel = BRIDGE_CHANNEL_CONTROL;
        assert(bridge_bus_send_app_command(&out));
    }
    out = make_message(0xFFFFu, 0xFFu);
    out.channel = BRIDGE_CHANNEL_CONTROL;
    assert(!bridge_bus_send_app_command(&out));
    assert(bridge_bus_take_app_overflow());
    assert(!bridge_bus_take_app_overflow());

    bridge_bus_init();
    memset(&out, 0, sizeof(out));
    out.length = BRIDGE_MESSAGE_PAYLOAD_SIZE + 1u;
    assert(!bridge_bus_send_app_command(&out));
    assert(!bridge_bus_publish_bt_event(&out, true));
    assert(!bridge_bus_take_release_required());

    return 0;
}
