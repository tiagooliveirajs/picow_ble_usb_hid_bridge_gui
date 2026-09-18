#include "ble_mouse.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "btstack.h"
#include "ble/le_device_db.h"

#include "ble_mouse_parser.h"
#include "bridge_bus.h"
#include "bt_runtime.h"
#include "canonical_hid.h"
#include "canonical_source.h"

#define BLE_MOUSE_DESCRIPTOR_STORAGE_SIZE 2048u
#define BLE_MOUSE_REJECTED_DEVICE_CAPACITY 4u
#define BLE_APPEARANCE_HID_GENERIC 960u
#define BLE_APPEARANCE_HID_MOUSE 962u
#define BLE_APPEARANCE_HID_LAST 1023u
#define BLE_MOUSE_BONDED_RECONNECT_TIMEOUT_MS 8000u
#define BLE_MOUSE_RECOVERY_SCAN_WINDOW_MS 3000u

_Static_assert(sizeof(canonical_mouse_event_t) <= BRIDGE_MESSAGE_PAYLOAD_SIZE,
               "canonical Mouse event must fit the bounded Core1->Core0 bridge");

typedef enum {
    BLE_MOUSE_WAITING_FOR_STACK = 0,
    BLE_MOUSE_SCANNING,
    BLE_MOUSE_CONNECTING,
    BLE_MOUSE_SECURING,
    BLE_MOUSE_CONNECTING_HIDS,
    BLE_MOUSE_READY,
    BLE_MOUSE_DISCONNECTING,
    BLE_MOUSE_CANCELLED,
} ble_mouse_state_t;

typedef struct {
    bool used;
    bd_addr_type_t address_type;
    bd_addr_t address;
} rejected_device_t;

static ble_mouse_state_t g_state = BLE_MOUSE_WAITING_FOR_STACK;
static bd_addr_t g_remote_address;
static bd_addr_type_t g_remote_address_type = BD_ADDR_TYPE_UNKNOWN;
static hci_con_handle_t g_connection_handle = HCI_CON_HANDLE_INVALID;
static uint16_t g_hids_cid;
static uint8_t g_descriptor_storage[BLE_MOUSE_DESCRIPTOR_STORAGE_SIZE];
static ble_mouse_parser_t g_parser;
static rejected_device_t g_rejected[BLE_MOUSE_REJECTED_DEVICE_CAPACITY];
static size_t g_rejected_next;
static btstack_packet_callback_registration_t g_hci_registration;
static btstack_packet_callback_registration_t g_sm_registration;
static btstack_timer_source_t g_reconnect_timer;
static bool g_reconnect_timer_active;
static bool g_reconnect_cancel_pending;
static bool g_reconnect_after_disconnect;
static bool g_scan_after_disconnect;
static bool g_recovery_cycle_active;

static void start_scan(bool resume_bonded_recovery);
static bool start_bonded_reconnect(void);
static void reconnect_or_scan(void);

static canonical_source_t mouse_source(void) {
    return canonical_source_make(CANONICAL_SOURCE_KIND_BLE_HOGP_MOUSE, 1u);
}

static void publish_status(uint16_t type) {
    const bridge_message_t message = {
        .channel = BRIDGE_CHANNEL_STATUS,
        .type = type,
        .length = 0u,
    };
    (void)bridge_bus_publish_bt_event(&message, false);
}

static void publish_release_source(void) {
    bridge_message_t message = {
        .channel = BRIDGE_CHANNEL_INPUT,
        .type = BT_EVENT_BLE_MOUSE_RELEASE_SOURCE,
        .length = CANONICAL_INPUT_SOURCE_PREFIX_SIZE,
    };
    canonical_source_write_input_prefix(message.payload, mouse_source());
    (void)bridge_bus_publish_bt_event(&message, true);
}

static bool publish_mouse_event(void *context, const canonical_mouse_event_t *event) {
    (void)context;
    if (event == NULL) return false;
    bridge_message_t message = {
        .channel = BRIDGE_CHANNEL_INPUT,
        .type = BT_EVENT_BLE_MOUSE_EVENT,
        .length = (uint16_t)sizeof(*event),
    };
    memcpy(message.payload, event, sizeof(*event));
    const bool release_sensitive = event->type == CANONICAL_MOUSE_EVENT_BUTTON;
    return bridge_bus_publish_bt_event(&message, release_sensitive);
}

static bool advertisement_has_hid_service(const uint8_t *packet) {
    const uint8_t *data = gap_event_advertising_report_get_data(packet);
    const uint8_t length = gap_event_advertising_report_get_data_length(packet);
    return ad_data_contains_uuid16(length, data,
        ORG_BLUETOOTH_SERVICE_HUMAN_INTERFACE_DEVICE);
}

static uint16_t advertisement_appearance(const uint8_t *packet) {
    const uint8_t *data = gap_event_advertising_report_get_data(packet);
    const uint8_t length = gap_event_advertising_report_get_data_length(packet);
    ad_context_t context;
    for (ad_iterator_init(&context, length, (uint8_t *)data);
         ad_iterator_has_more(&context); ad_iterator_next(&context)) {
        if (ad_iterator_get_data_type(&context) == BLUETOOTH_DATA_TYPE_APPEARANCE &&
            ad_iterator_get_data_len(&context) >= 2u)
            return little_endian_read_16(ad_iterator_get_data(&context), 0u);
    }
    return 0u;
}

static bool appearance_is_explicit_non_mouse_hid(uint16_t appearance) {
    return appearance >= BLE_APPEARANCE_HID_GENERIC &&
        appearance <= BLE_APPEARANCE_HID_LAST &&
        appearance != BLE_APPEARANCE_HID_GENERIC &&
        appearance != BLE_APPEARANCE_HID_MOUSE;
}

static bool address_is_rejected(const bd_addr_t address, bd_addr_type_t type) {
    for (size_t i = 0u; i < BLE_MOUSE_REJECTED_DEVICE_CAPACITY; ++i)
        if (g_rejected[i].used && g_rejected[i].address_type == type &&
            memcmp(g_rejected[i].address, address, sizeof(bd_addr_t)) == 0)
            return true;
    return false;
}

static void reject_address(const bd_addr_t address, bd_addr_type_t type) {
    if (address_is_rejected(address, type)) return;
    rejected_device_t *slot = &g_rejected[g_rejected_next];
    slot->used = true;
    slot->address_type = type;
    memcpy(slot->address, address, sizeof(bd_addr_t));
    g_rejected_next = (g_rejected_next + 1u) % BLE_MOUSE_REJECTED_DEVICE_CAPACITY;
}

static void stop_reconnect_timer(void) {
    if (!g_reconnect_timer_active) return;
    (void)btstack_run_loop_remove_timer(&g_reconnect_timer);
    g_reconnect_timer_active = false;
}

static void arm_reconnect_timer(uint32_t timeout_ms) {
    stop_reconnect_timer();
    btstack_run_loop_set_timer(&g_reconnect_timer, timeout_ms);
    btstack_run_loop_add_timer(&g_reconnect_timer);
    g_reconnect_timer_active = true;
}

static void start_scan(bool resume_bonded_recovery) {
    stop_reconnect_timer();
    g_reconnect_cancel_pending = false;
    g_recovery_cycle_active = resume_bonded_recovery;
    g_state = BLE_MOUSE_SCANNING;
    publish_status(BT_EVENT_BLE_MOUSE_SCANNING);
    gap_set_scan_parameters(0u, 48u, 48u);
    gap_start_scan();

    if (resume_bonded_recovery) {
        arm_reconnect_timer(BLE_MOUSE_RECOVERY_SCAN_WINDOW_MS);
    }
}

static void reconnect_timeout_handler(btstack_timer_source_t *timer) {
    (void)timer;
    g_reconnect_timer_active = false;

    if (g_state == BLE_MOUSE_SCANNING && g_recovery_cycle_active) {
        gap_stop_scan();
        if (!start_bonded_reconnect()) start_scan(false);
        return;
    }

    if (g_state != BLE_MOUSE_CONNECTING) return;

    const bool resume_bonded_recovery = g_recovery_cycle_active;
    g_reconnect_cancel_pending = true;
    if (gap_connect_cancel() != ERROR_CODE_SUCCESS) {
        g_reconnect_cancel_pending = false;
        start_scan(resume_bonded_recovery);
    }
}

static bool start_bonded_reconnect(void) {
    const int count = le_device_db_count();
    if (count <= 0) {
        g_recovery_cycle_active = false;
        return false;
    }

    stop_reconnect_timer();
    g_reconnect_cancel_pending = false;
    g_recovery_cycle_active = true;
    (void)gap_whitelist_clear();
    (void)gap_load_resolving_list_from_le_device_db();

    unsigned added = 0u;
    for (int index = 0; index < count; ++index) {
        int address_type = 0;
        bd_addr_t address;
        sm_key_t irk;
        memset(address, 0, sizeof(address));
        memset(irk, 0, sizeof(irk));
        le_device_db_info(index, &address_type, address, irk);
        if (gap_whitelist_add((bd_addr_type_t)address_type, address) != ERROR_CODE_SUCCESS)
            continue;
        if (added == 0u) {
            memcpy(g_remote_address, address, sizeof(bd_addr_t));
            g_remote_address_type = (bd_addr_type_t)address_type;
        }
        ++added;
    }

    if (added == 0u || gap_connect_with_whitelist() != ERROR_CODE_SUCCESS) {
        return false;
    }

    g_state = BLE_MOUSE_CONNECTING;
    publish_status(BT_EVENT_BLE_MOUSE_CONNECTING);
    arm_reconnect_timer(BLE_MOUSE_BONDED_RECONNECT_TIMEOUT_MS);
    return true;
}

static void reconnect_or_scan(void) {
    if (!start_bonded_reconnect()) start_scan(false);
}

static void disconnect_current(bool reconnect_bonded, bool scan_after) {
    const bool was_ready = g_state == BLE_MOUSE_READY;
    stop_reconnect_timer();
    g_reconnect_after_disconnect = reconnect_bonded;
    g_scan_after_disconnect = scan_after;
    g_state = BLE_MOUSE_DISCONNECTING;
    if (was_ready) {
        publish_release_source();
        publish_status(BT_EVENT_BLE_MOUSE_DISCONNECTED);
    }
    if (g_connection_handle != HCI_CON_HANDLE_INVALID) {
        (void)gap_disconnect(g_connection_handle);
        return;
    }

    g_reconnect_after_disconnect = false;
    g_scan_after_disconnect = false;
    if (reconnect_bonded) reconnect_or_scan();
    else if (scan_after) start_scan(false);
    else {
        g_recovery_cycle_active = false;
        g_state = BLE_MOUSE_CANCELLED;
    }
}

static void disconnect_and_rescan(void) {
    disconnect_current(false, true);
}

static void connect_hid_service(void);

static void handle_gatt_client_event(uint8_t packet_type, uint16_t channel,
                                     uint8_t *packet, uint16_t size) {
    (void)packet_type;
    (void)channel;
    (void)size;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_GATTSERVICE_META) return;

    switch (hci_event_gattservice_meta_get_subevent_code(packet)) {
    case GATTSERVICE_SUBEVENT_HID_SERVICE_CONNECTED: {
        const uint8_t status = gattservice_subevent_hid_service_connected_get_status(packet);
        if (status != ERROR_CODE_SUCCESS) {
            disconnect_and_rescan();
            return;
        }
        const uint8_t *descriptor =
            hids_client_descriptor_storage_get_descriptor_data(g_hids_cid, 0u);
        const uint16_t descriptor_len =
            hids_client_descriptor_storage_get_descriptor_len(g_hids_cid, 0u);
        const bool parser_ready = descriptor != NULL && descriptor_len > 0u &&
            ble_mouse_parser_configure(&g_parser, mouse_source(), descriptor, descriptor_len) &&
            ble_mouse_parser_has_mouse(&g_parser);
        if (!parser_ready) {
            if (descriptor != NULL && descriptor_len > 0u)
                reject_address(g_remote_address, g_remote_address_type);
            disconnect_and_rescan();
            return;
        }
        g_state = BLE_MOUSE_READY;
        g_reconnect_after_disconnect = false;
        g_scan_after_disconnect = false;
        g_recovery_cycle_active = false;
        publish_status(BT_EVENT_BLE_MOUSE_READY);
        break;
    }

    case GATTSERVICE_SUBEVENT_HID_SERVICE_DISCONNECTED:
        if (g_state == BLE_MOUSE_READY) {
            disconnect_current(true, false);
        } else if (g_state == BLE_MOUSE_SECURING ||
                   g_state == BLE_MOUSE_CONNECTING_HIDS) {
            disconnect_and_rescan();
        }
        break;

    case GATTSERVICE_SUBEVENT_HID_REPORT: {
        if (g_state != BLE_MOUSE_READY) break;
        const uint8_t report_id = gattservice_subevent_hid_report_get_report_id(packet);
        const uint8_t *report = gattservice_subevent_hid_report_get_report(packet);
        const uint16_t report_len = gattservice_subevent_hid_report_get_report_len(packet);
        if (!ble_mouse_parser_parse_report(&g_parser, report_id, report, report_len,
                                           publish_mouse_event, NULL))
            disconnect_and_rescan();
        break;
    }

    default:
        break;
    }
}

static void connect_hid_service(void) {
    g_state = BLE_MOUSE_CONNECTING_HIDS;
    g_hids_cid = 0u;
    const uint8_t status = hids_client_connect(g_connection_handle,
        &handle_gatt_client_event, HID_PROTOCOL_MODE_REPORT, &g_hids_cid);
    if (status != ERROR_CODE_SUCCESS) disconnect_and_rescan();
}

static void hci_packet_handler(uint8_t packet_type, uint16_t channel,
                               uint8_t *packet, uint16_t size) {
    (void)channel;
    (void)size;
    if (packet_type != HCI_EVENT_PACKET) return;

    switch (hci_event_packet_get_type(packet)) {
    case BTSTACK_EVENT_STATE:
        if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING &&
            (g_state == BLE_MOUSE_WAITING_FOR_STACK || g_state == BLE_MOUSE_CANCELLED))
            reconnect_or_scan();
        break;

    case GAP_EVENT_ADVERTISING_REPORT: {
        if (g_state != BLE_MOUSE_SCANNING || !advertisement_has_hid_service(packet)) break;
        bd_addr_t address;
        gap_event_advertising_report_get_address(packet, address);
        const bd_addr_type_t type = gap_event_advertising_report_get_address_type(packet);
        const uint16_t appearance = advertisement_appearance(packet);
        if (address_is_rejected(address, type)) break;
        if (appearance_is_explicit_non_mouse_hid(appearance)) {
            reject_address(address, type);
            break;
        }

        const bool resume_bonded_recovery = g_recovery_cycle_active;
        gap_stop_scan();
        stop_reconnect_timer();
        memcpy(g_remote_address, address, sizeof(bd_addr_t));
        g_remote_address_type = type;
        g_reconnect_cancel_pending = false;
        g_state = BLE_MOUSE_CONNECTING;
        publish_status(BT_EVENT_BLE_MOUSE_CONNECTING);
        if (gap_connect(g_remote_address, g_remote_address_type) != ERROR_CODE_SUCCESS)
            start_scan(resume_bonded_recovery);
        break;
    }

    case HCI_EVENT_META_GAP:
        if (hci_event_gap_meta_get_subevent_code(packet) == GAP_SUBEVENT_LE_CONNECTION_COMPLETE &&
            g_state == BLE_MOUSE_CONNECTING) {
            stop_reconnect_timer();
            const bool resume_bonded_recovery = g_recovery_cycle_active;
            const uint8_t status = gap_subevent_le_connection_complete_get_status(packet);
            if (status != ERROR_CODE_SUCCESS) {
                g_connection_handle = HCI_CON_HANDLE_INVALID;
                g_reconnect_cancel_pending = false;
                start_scan(resume_bonded_recovery);
                break;
            }
            g_reconnect_cancel_pending = false;
            g_connection_handle =
                gap_subevent_le_connection_complete_get_connection_handle(packet);
            g_state = BLE_MOUSE_SECURING;
            sm_request_pairing(g_connection_handle);
        }
        break;

    case HCI_EVENT_DISCONNECTION_COMPLETE: {
        const hci_con_handle_t handle =
            hci_event_disconnection_complete_get_connection_handle(packet);
        if (g_connection_handle == HCI_CON_HANDLE_INVALID || handle != g_connection_handle)
            break;

        const bool was_ready = g_state == BLE_MOUSE_READY;
        const bool reconnect_bonded = was_ready || g_reconnect_after_disconnect;
        const bool scan_after = g_scan_after_disconnect;
        stop_reconnect_timer();
        g_connection_handle = HCI_CON_HANDLE_INVALID;
        g_hids_cid = 0u;
        memset(&g_parser, 0, sizeof(g_parser));
        if (was_ready) {
            publish_release_source();
            publish_status(BT_EVENT_BLE_MOUSE_DISCONNECTED);
        }
        g_reconnect_after_disconnect = false;
        g_scan_after_disconnect = false;
        if (reconnect_bonded) {
            reconnect_or_scan();
        } else if (scan_after) {
            start_scan(false);
        } else {
            g_recovery_cycle_active = false;
            g_state = BLE_MOUSE_CANCELLED;
        }
        break;
    }

    default:
        break;
    }
}

static bool event_handle_matches(hci_con_handle_t handle) {
    return g_connection_handle != HCI_CON_HANDLE_INVALID && handle == g_connection_handle;
}

static void sm_packet_handler(uint8_t packet_type, uint16_t channel,
                              uint8_t *packet, uint16_t size) {
    (void)channel;
    (void)size;
    if (packet_type != HCI_EVENT_PACKET) return;

    bool ready = false;
    switch (hci_event_packet_get_type(packet)) {
    case SM_EVENT_JUST_WORKS_REQUEST: {
        const hci_con_handle_t handle = sm_event_just_works_request_get_handle(packet);
        if (event_handle_matches(handle)) sm_just_works_confirm(handle);
        break;
    }

    case SM_EVENT_NUMERIC_COMPARISON_REQUEST: {
        const hci_con_handle_t handle = sm_event_numeric_comparison_request_get_handle(packet);
        if (event_handle_matches(handle)) sm_numeric_comparison_confirm(handle);
        break;
    }

    case SM_EVENT_PAIRING_COMPLETE:
        if (!event_handle_matches(sm_event_pairing_complete_get_handle(packet))) break;
        if (sm_event_pairing_complete_get_status(packet) == ERROR_CODE_SUCCESS) {
            ready = true;
        } else {
            disconnect_and_rescan();
        }
        break;

    case SM_EVENT_REENCRYPTION_COMPLETE:
        if (!event_handle_matches(sm_event_reencryption_complete_get_handle(packet))) break;
        if (sm_event_reencryption_complete_get_status(packet) == ERROR_CODE_SUCCESS) {
            ready = true;
        } else if (sm_event_reencryption_complete_get_status(packet) ==
                   ERROR_CODE_PIN_OR_KEY_MISSING) {
            bd_addr_t address;
            sm_event_reencryption_complete_get_address(packet, address);
            const bd_addr_type_t type =
                (bd_addr_type_t)sm_event_reencryption_complete_get_addr_type(packet);
            gap_delete_bonding(type, address);
            sm_request_pairing(g_connection_handle);
        } else {
            disconnect_and_rescan();
        }
        break;

    default:
        break;
    }

    if (ready && g_state == BLE_MOUSE_SECURING) connect_hid_service();
}

void ble_mouse_init(void) {
    memset(&g_parser, 0, sizeof(g_parser));
    memset(g_rejected, 0, sizeof(g_rejected));
    memset(g_remote_address, 0, sizeof(g_remote_address));
    g_remote_address_type = BD_ADDR_TYPE_UNKNOWN;
    g_rejected_next = 0u;
    g_state = BLE_MOUSE_WAITING_FOR_STACK;
    g_connection_handle = HCI_CON_HANDLE_INVALID;
    g_hids_cid = 0u;
    g_reconnect_timer_active = false;
    g_reconnect_cancel_pending = false;
    g_reconnect_after_disconnect = false;
    g_scan_after_disconnect = false;
    g_recovery_cycle_active = false;

    hids_client_init(g_descriptor_storage, sizeof(g_descriptor_storage));
    g_hci_registration.callback = &hci_packet_handler;
    hci_add_event_handler(&g_hci_registration);
    g_sm_registration.callback = &sm_packet_handler;
    sm_add_event_handler(&g_sm_registration);
    btstack_run_loop_set_timer_handler(&g_reconnect_timer, reconnect_timeout_handler);
}

void ble_mouse_handle_command(uint16_t command_type) {
    switch (command_type) {
    case BT_COMMAND_BLE_MOUSE_RETRY:
        gap_stop_scan();
        stop_reconnect_timer();
        g_recovery_cycle_active = false;
        if (g_connection_handle != HCI_CON_HANDLE_INVALID) {
            disconnect_current(false, true);
        } else {
            (void)gap_connect_cancel();
            start_scan(false);
        }
        break;

    case BT_COMMAND_BLE_MOUSE_CANCEL:
        gap_stop_scan();
        stop_reconnect_timer();
        g_recovery_cycle_active = false;
        if (g_connection_handle != HCI_CON_HANDLE_INVALID) {
            disconnect_current(false, false);
        } else {
            (void)gap_connect_cancel();
            g_state = BLE_MOUSE_CANCELLED;
        }
        break;

    default:
        break;
    }
}

bool ble_mouse_is_ready(void) {
    return g_state == BLE_MOUSE_READY &&
           g_connection_handle != HCI_CON_HANDLE_INVALID && g_hids_cid != 0u;
}
