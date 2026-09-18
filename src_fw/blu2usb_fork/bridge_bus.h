#ifndef BLU2USB_FORK_BRIDGE_BUS_H
#define BLU2USB_FORK_BRIDGE_BUS_H

#include <stdbool.h>
#include <stdint.h>

#include "canonical_source.h"

#define BRIDGE_MESSAGE_PAYLOAD_SIZE 32u
#define BRIDGE_QUEUE_CAPACITY 32u
#define BRIDGE_RELEASE_SOURCE_CAPACITY CANONICAL_SOURCE_CAPACITY

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

/* Failed release-sensitive canonical INPUT publications retain exact kind +
 * instance identities in a bounded lock-free latch. Core0 drains queued older
 * snapshots first and then tears down only these sources. */
uint8_t bridge_bus_take_release_sources(
    canonical_source_t *sources,
    uint8_t capacity);

/* Used only if the failed publication has no valid source prefix or if the
 * bounded release-identity latch itself is exhausted. */
bool bridge_bus_take_release_required(void);

#endif
