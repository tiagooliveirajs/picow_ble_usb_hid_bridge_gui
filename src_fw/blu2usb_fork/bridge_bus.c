#include "bridge_bus.h"

#include <stdatomic.h>
#include <string.h>

typedef struct {
    bridge_message_t entries[BRIDGE_QUEUE_CAPACITY];
    atomic_uint write_sequence;
    atomic_uint read_sequence;
    atomic_bool overflowed;
} bridge_ring_t;

static bridge_ring_t g_app_to_bt;
static bridge_ring_t g_bt_to_app;
static atomic_uint g_release_source_mask = ATOMIC_VAR_INIT(0u);
static atomic_bool g_release_required = ATOMIC_VAR_INIT(false);

static void ring_reset(bridge_ring_t *ring) {
    atomic_store_explicit(&ring->read_sequence, 0u, memory_order_relaxed);
    atomic_store_explicit(&ring->write_sequence, 0u, memory_order_relaxed);
    atomic_store_explicit(&ring->overflowed, false, memory_order_relaxed);
    memset(ring->entries, 0, sizeof(ring->entries));
}

static bool message_valid(const bridge_message_t *message) {
    return message != NULL && message->length <= BRIDGE_MESSAGE_PAYLOAD_SIZE;
}

static bool ring_publish(bridge_ring_t *ring, const bridge_message_t *message) {
    if (!message_valid(message)) return false;

    const unsigned int write_sequence =
        atomic_load_explicit(&ring->write_sequence, memory_order_relaxed);
    const unsigned int read_sequence =
        atomic_load_explicit(&ring->read_sequence, memory_order_acquire);

    if ((unsigned int)(write_sequence - read_sequence) >= BRIDGE_QUEUE_CAPACITY) {
        atomic_store_explicit(&ring->overflowed, true, memory_order_release);
        return false;
    }

    ring->entries[write_sequence % BRIDGE_QUEUE_CAPACITY] = *message;
    atomic_store_explicit(
        &ring->write_sequence, write_sequence + 1u, memory_order_release);
    return true;
}

static bool ring_poll(bridge_ring_t *ring, bridge_message_t *message) {
    if (message == NULL) return false;

    const unsigned int read_sequence =
        atomic_load_explicit(&ring->read_sequence, memory_order_relaxed);
    const unsigned int write_sequence =
        atomic_load_explicit(&ring->write_sequence, memory_order_acquire);

    if (read_sequence == write_sequence) return false;

    *message = ring->entries[read_sequence % BRIDGE_QUEUE_CAPACITY];
    atomic_store_explicit(
        &ring->read_sequence, read_sequence + 1u, memory_order_release);
    return true;
}

static bool ring_take_overflow(bridge_ring_t *ring) {
    return atomic_exchange_explicit(
        &ring->overflowed, false, memory_order_acq_rel);
}

static void latch_release_for_failed_message(const bridge_message_t *message) {
    if (message->channel == BRIDGE_CHANNEL_INPUT && message->length >= 1u) {
        const uint8_t source = message->payload[0];
        if (source > 0u && source < 32u) {
            atomic_fetch_or_explicit(
                &g_release_source_mask,
                (unsigned int)(UINT32_C(1) << source),
                memory_order_acq_rel);
            return;
        }
    }
    atomic_store_explicit(&g_release_required, true, memory_order_release);
}

void bridge_bus_init(void) {
    ring_reset(&g_app_to_bt);
    ring_reset(&g_bt_to_app);
    atomic_store_explicit(&g_release_source_mask, 0u, memory_order_relaxed);
    atomic_store_explicit(&g_release_required, false, memory_order_relaxed);
}

bool bridge_bus_send_app_command(const bridge_message_t *message) {
    return ring_publish(&g_app_to_bt, message);
}

bool bridge_bus_take_app_command(bridge_message_t *message) {
    return ring_poll(&g_app_to_bt, message);
}

bool bridge_bus_publish_bt_event(
    const bridge_message_t *message,
    bool release_sensitive) {
    if (ring_publish(&g_bt_to_app, message)) return true;

    if (release_sensitive && message_valid(message)) {
        latch_release_for_failed_message(message);
    }
    return false;
}

bool bridge_bus_take_bt_event(bridge_message_t *message) {
    return ring_poll(&g_bt_to_app, message);
}

bool bridge_bus_take_app_overflow(void) {
    return ring_take_overflow(&g_app_to_bt);
}

bool bridge_bus_take_bt_overflow(void) {
    return ring_take_overflow(&g_bt_to_app);
}

uint32_t bridge_bus_take_release_sources(void) {
    return (uint32_t)atomic_exchange_explicit(
        &g_release_source_mask, 0u, memory_order_acq_rel);
}

bool bridge_bus_take_release_required(void) {
    return atomic_exchange_explicit(
        &g_release_required, false, memory_order_acq_rel);
}
