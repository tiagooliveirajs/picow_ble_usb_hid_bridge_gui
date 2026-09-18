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

/* For release-sensitive INPUT messages, payload[0] is the stable canonical
 * source ID. If publication fails because the bounded BT->Core0 queue is full,
 * the source bit is latched independently so Core0 can tear down only that
 * source after draining older queued snapshots. */
uint32_t bridge_bus_take_release_sources(void);

/* Fallback for a release-sensitive publication whose source cannot be safely
 * identified. This is intentionally separate from the normal source-aware path. */
bool bridge_bus_take_release_required(void);

#endif
