#include "classic_keyboard.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "btstack.h"

#include "bridge_bus.h"
#include "bt_runtime.h"
#include "classic_keyboard_report.h"
#include "keyboard_input.h"

#define TARGET_NAME "Bluetooth keyboard 3.0"
#define TARGET_NAME_ALIAS "BKB-3G"
#define INQUIRY_DURATION_1280MS 5u
#define MAX_DISCOVERED_DEVICES 20u
#define HID_DESCRIPTOR_STORAGE_SIZE 512u
#define REMOTE_NAME_TIMEOUT_MS 5000u
#define BONDING_TIMEOUT_MS 15000u
#define HID_CONNECT_TIMEOUT_MS 15000u
#define RETRY_DELAY_MS 500u
#define MANUAL_RETRY_DELAY_MS 100u

typedef enum {
    DEVICE_NAME_UNKNOWN = 0,
    DEVICE_NAME_REQUESTED,
    DEVICE_NAME_RESOLVED,
} device_name_state_t;

typedef struct {
    bd_addr_t address;
    uint8_t page_scan_repetition_mode;
    uint16_t clock_offset;
    device_name_state_t name_state;
} discovered_device_t;

typedef enum {
    CLASSIC_WAITING_FOR_STACK = 0,
    CLASSIC_INQUIRY,
    CLASSIC_RESOLVING_NAMES,
    CLASSIC_BONDING,
    CLASSIC_WAITING_FOR_HID_START,
    CLASSIC_CONNECTING,
    CLASSIC_READY,
    CLASSIC_RETRY_WAIT,
    CLASSIC_CANCELLED,
} classic_state_t;

static volatile classic_state_t g_state = CLASSIC_WAITING_FOR_STACK;
static discovered_device_t g_devices[MAX_DISCOVERED_DEVICES];
static uint8_t g_device_count;
static bd_addr_t g_target_addr;
static bool g_target_valid;
static bool g_stale_key_recovery_attempted;
static volatile uint16_t g_hid_host_cid;
static volatile bool g_hid_descriptor_available;
static hci_con_handle_t g_target_acl_handle = HCI_CON_HANDLE_INVALID;
static uint8_t g_hid_descriptor_storage[HID_DESCRIPTOR_STORAGE_SIZE];
static btstack_packet_callback_registration_t g_hci_event_callback_registration;
static btstack_context_callback_registration_t g_start_hid_callback;
static btstack_timer_source_t g_phase_timer;
static btstack_timer_source_t g_retry_timer;

static void start_inquiry(void);
static void request_next_remote_name(void);
static void bond_target(const bd_addr_t address);
static void connect_target(const bd_addr_t address);
static void schedule_retry(uint32_t delay_ms);

static void publish_status(uint16_t event_type) {
    const bridge_message_t message = {
        .channel = BRIDGE_CHANNEL_STATUS,
        .type = event_type,
        .length = 0u,
    };
    (void)bridge_bus_publish_bt_event(&message, false);
}

static void publish_snapshot(const keyboard_input_snapshot_t *snapshot) {
    bridge_message_t message = {
        .channel = BRIDGE_CHANNEL_INPUT,
        .type = BT_EVENT_KEYBOARD_SNAPSHOT,
        .length = (uint16_t)sizeof(*snapshot),
    };
    memcpy(message.payload, snapshot, sizeof(*snapshot));
    (void)bridge_bus_publish_bt_event(&message, true);
}

static void publish_neutral_snapshot(void) {
    const keyboard_input_snapshot_t neutral = {
        .source = KEYBOARD_SOURCE_CLASSIC_HID,
    };
    publish_snapshot(&neutral);
}

static void clear_target_runtime(void) {
    g_hid_host_cid = 0u;
    g_hid_descriptor_available = false;
    g_target_acl_handle = HCI_CON_HANDLE_INVALID;
}

static void stop_active_transport(void) {
    gap_inquiry_stop();
    btstack_run_loop_remove_timer(&g_phase_timer);

    if (g_hid_host_cid != 0u) {
        const uint16_t cid = g_hid_host_cid;
        g_hid_host_cid = 0u;
        hid_host_disconnect(cid);
    }

    if (g_target_acl_handle != HCI_CON_HANDLE_INVALID) {
        const hci_con_handle_t handle = g_target_acl_handle;
        g_target_acl_handle = HCI_CON_HANDLE_INVALID;
        (void)gap_disconnect(handle);
    }
}

static void retry_timeout(btstack_timer_source_t *timer) {
    (void)timer;
    if (g_state != CLASSIC_RETRY_WAIT) return;
    start_inquiry();
}

static void schedule_retry(uint32_t delay_ms) {
    btstack_run_loop_remove_timer(&g_phase_timer);
    btstack_run_loop_remove_timer(&g_retry_timer);
    clear_target_runtime();
    g_state = CLASSIC_RETRY_WAIT;
    publish_status(BT_EVENT_CLASSIC_RETRYING);
    btstack_run_loop_set_timer_handler(&g_retry_timer, retry_timeout);
    btstack_run_loop_set_timer(&g_retry_timer, delay_ms);
    btstack_run_loop_add_timer(&g_retry_timer);
}

static void phase_timeout(btstack_timer_source_t *timer) {
    (void)timer;

    if (g_state == CLASSIC_RESOLVING_NAMES) {
        for (uint8_t i = 0u; i < g_device_count; ++i) {
            if (g_devices[i].name_state == DEVICE_NAME_REQUESTED) {
                g_devices[i].name_state = DEVICE_NAME_RESOLVED;
                break;
            }
        }
        request_next_remote_name();
        return;
    }

    if (g_state == CLASSIC_BONDING ||
        g_state == CLASSIC_WAITING_FOR_HID_START ||
        g_state == CLASSIC_CONNECTING) {
        stop_active_transport();
        publish_neutral_snapshot();
        schedule_retry(RETRY_DELAY_MS);
    }
}

static void arm_phase_timeout(uint32_t timeout_ms) {
    btstack_run_loop_remove_timer(&g_phase_timer);
    btstack_run_loop_set_timer_handler(&g_phase_timer, phase_timeout);
    btstack_run_loop_set_timer(&g_phase_timer, timeout_ms);
    btstack_run_loop_add_timer(&g_phase_timer);
}

static bool target_name_matches(const char *name) {
    return name != NULL &&
           (strcmp(name, TARGET_NAME) == 0 || strcmp(name, TARGET_NAME_ALIAS) == 0);
}

static int device_index_for_address(const bd_addr_t address) {
    for (uint8_t i = 0u; i < g_device_count; ++i) {
        if (bd_addr_cmp(address, g_devices[i].address) == 0) return (int)i;
    }
    return -1;
}

static void start_inquiry(void) {
    btstack_run_loop_remove_timer(&g_phase_timer);
    btstack_run_loop_remove_timer(&g_retry_timer);
    g_device_count = 0u;
    g_hid_descriptor_available = false;
    g_state = CLASSIC_INQUIRY;
    publish_status(BT_EVENT_CLASSIC_SCANNING);

    const uint8_t status = gap_inquiry_start(INQUIRY_DURATION_1280MS);
    if (status != ERROR_CODE_SUCCESS) {
        schedule_retry(RETRY_DELAY_MS);
    }
}

static void bond_target(const bd_addr_t address) {
    const bool target_changed = !g_target_valid ||
        bd_addr_cmp(address, g_target_addr) != 0;
    if (target_changed) g_stale_key_recovery_attempted = false;

    memcpy(g_target_addr, address, sizeof(bd_addr_t));
    g_target_valid = true;
    gap_inquiry_stop();
    clear_target_runtime();
    g_state = CLASSIC_BONDING;
    publish_status(BT_EVENT_CLASSIC_BONDING);
    arm_phase_timeout(BONDING_TIMEOUT_MS);

    const int status = gap_dedicated_bonding(g_target_addr, 0);
    if (status != ERROR_CODE_SUCCESS) {
        schedule_retry(RETRY_DELAY_MS);
    }
}

static void connect_target(const bd_addr_t address) {
    if (!g_target_valid || bd_addr_cmp(address, g_target_addr) != 0) {
        memcpy(g_target_addr, address, sizeof(bd_addr_t));
        g_target_valid = true;
    }

    gap_inquiry_stop();
    btstack_run_loop_remove_timer(&g_retry_timer);
    g_state = CLASSIC_CONNECTING;
    g_hid_descriptor_available = false;
    publish_status(BT_EVENT_CLASSIC_CONNECTING);
    arm_phase_timeout(HID_CONNECT_TIMEOUT_MS);

    uint16_t new_cid = 0u;
    const uint8_t status =
        hid_host_connect(g_target_addr, HID_PROTOCOL_MODE_REPORT, &new_cid);
    if (status != ERROR_CODE_SUCCESS) {
        g_hid_host_cid = 0u;
        schedule_retry(RETRY_DELAY_MS);
        return;
    }

    g_hid_host_cid = new_cid;
}

static void start_hid_after_bonding(void *context) {
    (void)context;
    if (g_state != CLASSIC_WAITING_FOR_HID_START || !g_target_valid) return;
    connect_target(g_target_addr);
}

static void handle_bonding_complete(uint8_t *packet) {
    bd_addr_t address;
    gap_event_dedicated_bonding_completed_get_address(packet, address);
    const uint8_t status = gap_event_dedicated_bonding_completed_get_status(packet);

    if (g_state != CLASSIC_BONDING || !g_target_valid ||
        bd_addr_cmp(address, g_target_addr) != 0) {
        return;
    }

    if (status != ERROR_CODE_SUCCESS) {
        publish_neutral_snapshot();
        /* A reset Keyboard can forget its Classic link key while Pico keeps
         * the old one in BTstack TLV. Drop that one target key once, then let
         * the normal bounded discovery/bonding path establish a fresh key. */
        if (!g_stale_key_recovery_attempted) {
            gap_drop_link_key_for_bd_addr(g_target_addr);
            g_stale_key_recovery_attempted = true;
            schedule_retry(MANUAL_RETRY_DELAY_MS);
        } else {
            schedule_retry(RETRY_DELAY_MS);
        }
        return;
    }

    g_stale_key_recovery_attempted = false;
    btstack_run_loop_remove_timer(&g_phase_timer);
    g_state = CLASSIC_WAITING_FOR_HID_START;
    g_start_hid_callback.callback = start_hid_after_bonding;
    g_start_hid_callback.context = NULL;
    btstack_run_loop_execute_on_main_thread(&g_start_hid_callback);
}

static void request_next_remote_name(void) {
    g_state = CLASSIC_RESOLVING_NAMES;

    for (uint8_t i = 0u; i < g_device_count; ++i) {
        if (g_devices[i].name_state != DEVICE_NAME_UNKNOWN) continue;

        g_devices[i].name_state = DEVICE_NAME_REQUESTED;
        const uint8_t status = gap_remote_name_request(
            g_devices[i].address,
            g_devices[i].page_scan_repetition_mode,
            (uint16_t)(g_devices[i].clock_offset | 0x8000u));

        if (status == ERROR_CODE_SUCCESS) {
            arm_phase_timeout(REMOTE_NAME_TIMEOUT_MS);
            return;
        }
        g_devices[i].name_state = DEVICE_NAME_RESOLVED;
    }

    schedule_retry(RETRY_DELAY_MS);
}

static void handle_inquiry_result(uint8_t *packet) {
    if (g_state != CLASSIC_INQUIRY) return;

    bd_addr_t address;
    gap_event_inquiry_result_get_bd_addr(packet, address);
    if (device_index_for_address(address) >= 0) return;

    if (gap_event_inquiry_result_get_name_available(packet)) {
        char name[249];
        uint16_t name_len = gap_event_inquiry_result_get_name_len(packet);
        if (name_len >= sizeof(name)) name_len = sizeof(name) - 1u;
        memcpy(name, gap_event_inquiry_result_get_name(packet), name_len);
        name[name_len] = '\0';
        if (target_name_matches(name)) {
            bond_target(address);
            return;
        }
    }

    if (g_device_count >= MAX_DISCOVERED_DEVICES) return;

    memcpy(g_devices[g_device_count].address, address, sizeof(bd_addr_t));
    g_devices[g_device_count].page_scan_repetition_mode =
        gap_event_inquiry_result_get_page_scan_repetition_mode(packet);
    g_devices[g_device_count].clock_offset =
        gap_event_inquiry_result_get_clock_offset(packet);
    g_devices[g_device_count].name_state =
        gap_event_inquiry_result_get_name_available(packet)
            ? DEVICE_NAME_RESOLVED
            : DEVICE_NAME_UNKNOWN;
    ++g_device_count;
}

static void handle_remote_name_complete(uint8_t *packet) {
    if (g_state != CLASSIC_RESOLVING_NAMES) return;
    btstack_run_loop_remove_timer(&g_phase_timer);

    bd_addr_t address;
    reverse_bd_addr(&packet[3], address);
    const int index = device_index_for_address(address);
    if (index < 0) {
        request_next_remote_name();
        return;
    }

    g_devices[index].name_state = DEVICE_NAME_RESOLVED;
    if (packet[2] == ERROR_CODE_SUCCESS) {
        const char *name = (const char *)&packet[9];
        if (target_name_matches(name)) {
            bond_target(address);
            return;
        }
    }
    request_next_remote_name();
}

static void handle_hid_report(uint8_t *packet) {
    if (!g_hid_descriptor_available || g_hid_host_cid == 0u) return;

    const uint8_t *descriptor =
        hid_descriptor_storage_get_descriptor_data(g_hid_host_cid);
    const uint16_t descriptor_len =
        hid_descriptor_storage_get_descriptor_len(g_hid_host_cid);
    const uint8_t *report = hid_subevent_report_get_report(packet);
    const uint16_t report_len = hid_subevent_report_get_report_len(packet);

    keyboard_input_snapshot_t snapshot;
    if (classic_keyboard_parse_report(
            descriptor, descriptor_len, report, report_len, &snapshot)) {
        publish_snapshot(&snapshot);
    }
}

static bool incoming_connection_matches_target(uint8_t *packet) {
    if (!g_target_valid) return false;
    bd_addr_t incoming_addr;
    hid_subevent_incoming_connection_get_address(packet, incoming_addr);
    return bd_addr_cmp(incoming_addr, g_target_addr) == 0;
}

static bool state_allows_known_target_reconnect(void) {
    return g_state == CLASSIC_BONDING ||
           g_state == CLASSIC_WAITING_FOR_HID_START ||
           g_state == CLASSIC_CONNECTING ||
           g_state == CLASSIC_RETRY_WAIT ||
           g_state == CLASSIC_INQUIRY ||
           g_state == CLASSIC_RESOLVING_NAMES;
}

static void handle_hid_meta(uint8_t *packet, uint16_t size) {
    (void)size;
    switch (hci_event_hid_meta_get_subevent_code(packet)) {
        case HID_SUBEVENT_INCOMING_CONNECTION: {
            const uint16_t cid = hid_subevent_incoming_connection_get_hid_cid(packet);
            if (!state_allows_known_target_reconnect() ||
                !incoming_connection_matches_target(packet)) {
                hid_host_decline_connection(cid);
                break;
            }

            /* A bonded Keyboard normally reconnects by initiating HID itself.
             * Stop discovery/retry work and accept the known target directly. */
            gap_inquiry_stop();
            btstack_run_loop_remove_timer(&g_phase_timer);
            btstack_run_loop_remove_timer(&g_retry_timer);
            g_hid_host_cid = cid;
            g_hid_descriptor_available = false;
            g_state = CLASSIC_CONNECTING;
            publish_status(BT_EVENT_CLASSIC_CONNECTING);
            arm_phase_timeout(HID_CONNECT_TIMEOUT_MS);
            hid_host_accept_connection(cid, HID_PROTOCOL_MODE_REPORT);
            break;
        }

        case HID_SUBEVENT_CONNECTION_OPENED: {
            const uint8_t status = hid_subevent_connection_opened_get_status(packet);
            if (status != ERROR_CODE_SUCCESS) {
                publish_neutral_snapshot();
                schedule_retry(RETRY_DELAY_MS);
                break;
            }
            g_hid_host_cid = hid_subevent_connection_opened_get_hid_cid(packet);
            g_state = CLASSIC_CONNECTING;
            g_hid_descriptor_available = false;
            arm_phase_timeout(HID_CONNECT_TIMEOUT_MS);
            break;
        }

        case HID_SUBEVENT_DESCRIPTOR_AVAILABLE: {
            const uint8_t status = hid_subevent_descriptor_available_get_status(packet);
            const uint16_t descriptor_len =
                hid_descriptor_storage_get_descriptor_len(g_hid_host_cid);
            if (status != ERROR_CODE_SUCCESS || descriptor_len == 0u) {
                publish_neutral_snapshot();
                stop_active_transport();
                schedule_retry(RETRY_DELAY_MS);
                break;
            }
            g_hid_descriptor_available = true;
            g_stale_key_recovery_attempted = false;
            g_state = CLASSIC_READY;
            btstack_run_loop_remove_timer(&g_phase_timer);
            publish_status(BT_EVENT_CLASSIC_READY);
            break;
        }

        case HID_SUBEVENT_REPORT:
            if (g_state == CLASSIC_READY) handle_hid_report(packet);
            break;

        case HID_SUBEVENT_CONNECTION_CLOSED:
            /* Ignore a late close from the previous HID session once retry,
             * discovery or fresh bonding has already started. */
            if (g_state == CLASSIC_CONNECTING || g_state == CLASSIC_READY) {
                publish_neutral_snapshot();
                schedule_retry(RETRY_DELAY_MS);
            }
            break;

        case HID_SUBEVENT_SET_PROTOCOL_RESPONSE:
        case HID_SUBEVENT_SNIFF_SUBRATING_PARAMS:
        default:
            break;
    }
}

static void packet_handler(uint8_t packet_type,
                           uint16_t channel,
                           uint8_t *packet,
                           uint16_t size) {
    (void)channel;
    if (packet_type != HCI_EVENT_PACKET) return;

    const uint8_t event = hci_event_packet_get_type(packet);
    bd_addr_t event_addr;

    switch (event) {
        case GAP_EVENT_INQUIRY_RESULT:
            handle_inquiry_result(packet);
            break;

        case GAP_EVENT_INQUIRY_COMPLETE:
            if (g_state == CLASSIC_INQUIRY) request_next_remote_name();
            break;

        case HCI_EVENT_REMOTE_NAME_REQUEST_COMPLETE:
            handle_remote_name_complete(packet);
            break;

        case GAP_EVENT_DEDICATED_BONDING_COMPLETED:
            handle_bonding_complete(packet);
            break;

        case HCI_EVENT_CONNECTION_COMPLETE:
            hci_event_connection_complete_get_bd_addr(packet, event_addr);
            if (g_target_valid && bd_addr_cmp(event_addr, g_target_addr) == 0 &&
                hci_event_connection_complete_get_status(packet) == ERROR_CODE_SUCCESS) {
                g_target_acl_handle =
                    hci_event_connection_complete_get_connection_handle(packet);
            }
            break;

        case HCI_EVENT_DISCONNECTION_COMPLETE: {
            const hci_con_handle_t handle =
                hci_event_disconnection_complete_get_connection_handle(packet);
            if (handle != g_target_acl_handle) break;
            g_target_acl_handle = HCI_CON_HANDLE_INVALID;

            // Dedicated bonding intentionally disconnects its ACL after the
            // completion event. In WAITING_FOR_HID_START that disconnect is
            // expected and must not race the deferred callback into retry.
            if (g_state == CLASSIC_WAITING_FOR_HID_START ||
                g_state == CLASSIC_BONDING ||
                g_state == CLASSIC_CANCELLED ||
                g_state == CLASSIC_RETRY_WAIT) {
                break;
            }

            if (g_state == CLASSIC_CONNECTING || g_state == CLASSIC_READY) {
                publish_neutral_snapshot();
                schedule_retry(RETRY_DELAY_MS);
            }
            break;
        }

        case HCI_EVENT_PIN_CODE_REQUEST:
            hci_event_pin_code_request_get_bd_addr(packet, event_addr);
            if (g_target_valid && bd_addr_cmp(event_addr, g_target_addr) == 0) {
                gap_pin_code_response(event_addr, "0000");
            }
            break;

        case HCI_EVENT_USER_CONFIRMATION_REQUEST:
            hci_event_user_confirmation_request_get_bd_addr(packet, event_addr);
            if (g_target_valid && bd_addr_cmp(event_addr, g_target_addr) == 0) {
                gap_ssp_confirmation_response(event_addr);
            }
            break;

        case HCI_EVENT_HID_META:
            handle_hid_meta(packet, size);
            break;

        default:
            break;
    }
}

void classic_keyboard_init(void) {
    clear_target_runtime();
    g_target_valid = false;
    g_stale_key_recovery_attempted = false;
    g_device_count = 0u;
    g_state = CLASSIC_WAITING_FOR_STACK;

    hid_host_init(g_hid_descriptor_storage, sizeof(g_hid_descriptor_storage));
    hid_host_register_packet_handler(packet_handler);

    gap_set_default_link_policy_settings(
        LM_LINK_POLICY_ENABLE_SNIFF_MODE | LM_LINK_POLICY_ENABLE_ROLE_SWITCH);
    hci_set_master_slave_policy(HCI_ROLE_MASTER);
    hci_set_inquiry_mode(INQUIRY_MODE_RSSI_AND_EIR);
    gap_discoverable_control(1);

    g_hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&g_hci_event_callback_registration);
}

void classic_keyboard_on_stack_working(void) {
    if (g_state == CLASSIC_WAITING_FOR_STACK ||
        g_state == CLASSIC_CANCELLED ||
        g_state == CLASSIC_RETRY_WAIT) {
        start_inquiry();
    }
}

void classic_keyboard_handle_command(uint16_t command_type) {
    switch (command_type) {
        case BT_COMMAND_CLASSIC_CANCEL:
            stop_active_transport();
            publish_neutral_snapshot();
            clear_target_runtime();
            g_state = CLASSIC_CANCELLED;
            publish_status(BT_EVENT_CLASSIC_CANCELLED);
            break;

        case BT_COMMAND_CLASSIC_RETRY:
            stop_active_transport();
            publish_neutral_snapshot();
            g_stale_key_recovery_attempted = false;
            schedule_retry(MANUAL_RETRY_DELAY_MS);
            break;

        default:
            break;
    }
}

bool classic_keyboard_is_ready(void) {
    return g_state == CLASSIC_READY &&
           g_hid_descriptor_available &&
           g_hid_host_cid != 0u;
}
