#ifndef BLU2USB_FORK_BRIDGE_BUS_H
#define BLU2USB_FORK_BRIDGE_BUS_H

#include <stdbool.h>
#include <stdint.h>

#define BRIDGE_MESSAGE_PAYLOAD_SIZE 32u
#define BRIDGE_QUEUE_CAPACITY 32u

#define BRIDGE_CHANNEL_CONTROL 1u
#define BRIDGE_CHANNEL_STATUS 2u
#define BRIDGE_CHANNEL_INPUT 3u

typedef struct {
    uint16_t channel;
    uint16_t type;
    uint16_t length;
    uint8_t payload[BRIDGE_MESSAGE_PAYLOAD_SIZE];
} bridge_message_t;

void bridge_bus_init(void);

bool bridge_bus_send_app_command(const bridge_message_t *message);
bool bridge_bus_take_app_command(bridge_message_t *message);

bool bridge_bus_publish_bt_event(const bridge_message_t *message, bool release_sensitive);
bool bridge_bus_take_bt_event(bridge_message_t *message);

bool bridge_bus_take_app_overflow(void);
bool bridge_bus_take_bt_overflow(void);

/* If a release-sensitive BT->Core0 publication is dropped because the bounded
 * queue is full, this latch guarantees Core0 sees a release-all request even
 * when no queue slot is available. */
bool bridge_bus_take_release_required(void);

#endif
