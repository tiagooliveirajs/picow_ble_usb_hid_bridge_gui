#include "classic_hid.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "btstack.h"
#include "keyboard_queue.h"
#include "pico/cyw43_arch.h"
#include "poc_log.h"

#define TARGET_NAME "Bluetooth keyboard 3.0"
#define TARGET_NAME_ALIAS "BKB-3G"
#define INQUIRY_DURATION_1280MS 5u
#define MAX_DISCOVERED_DEVICES 20u
#define HID_DESCRIPTOR_STORAGE_SIZE 512u

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
    APP_WAITING_FOR_BTSTACK = 0,
    APP_INQUIRY,
    APP_RESOLVING_NAMES,
    APP_BONDING,
    APP_CONNECTING,
    APP_CONNECTED,
} app_state_t;

static volatile app_state_t g_app_state = APP_WAITING_FOR_BTSTACK;
static discovered_device_t g_devices[MAX_DISCOVERED_DEVICES];
static uint8_t g_device_count;
static bd_addr_t g_target_addr;
static volatile uint16_t g_hid_host_cid;
static volatile bool g_hid_descriptor_available;
static uint8_t g_hid_descriptor_storage[HID_DESCRIPTOR_STORAGE_SIZE];
static btstack_packet_callback_registration_t g_hci_event_callback_registration;

static void start_inquiry(void);
static void request_next_remote_name(void);
static void bond_target(const bd_addr_t address);
static void connect_target(const bd_addr_t address);

bool classic_hid_is_ready(void) {
    return g_app_state == APP_CONNECTED &&
           g_hid_descriptor_available &&
           g_hid_host_cid != 0u;
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
    g_device_count = 0u;
    g_hid_descriptor_available = false;
    g_app_state = APP_INQUIRY;

    poc_logf("SCAN: Bluetooth Classic inquiry started");
    poc_logf("SCAN: targets '%s' or '%s'", TARGET_NAME, TARGET_NAME_ALIAS);
    poc_logf("ACTION: put BKB-3G in pairing mode with FN+1, FN+2 or FN+3");

    const uint8_t status = gap_inquiry_start(INQUIRY_DURATION_1280MS);
    if (status != ERROR_CODE_SUCCESS) {
        poc_logf("ERROR: gap_inquiry_start -> 0x%02x", status);
    }
}

static void bond_target(const bd_addr_t address) {
    memcpy(g_target_addr, address, sizeof(bd_addr_t));
    gap_inquiry_stop();

    g_app_state = APP_BONDING;
    g_hid_host_cid = 0u;
    g_hid_descriptor_available = false;

    poc_logf("TARGET: %s", bd_addr_to_str(g_target_addr));
    poc_logf("BOND: starting dedicated Classic bonding before HID");
    poc_logf("BOND: stale local link key will be discarded by BTstack");
    poc_logf("BOND: GT T1/40062 uses Level 2 bonding (MITM not required / Just Works capable)");

    // The BKB-3G rejects unauthenticated HID L2CAP channels with 0x66
    // (L2CAP_CONNECTION_RESPONSE_RESULT_REFUSED_SECURITY).  Dedicated bonding
    // intentionally creates/authenticates an ACL first, stores the new link key,
    // disconnects, and emits GAP_EVENT_DEDICATED_BONDING_COMPLETED.  Only then
    // do we open the HID Control/Interrupt channels.
    //
    // The Goldentec GT T1 / code 40062 manual describes first pairing as
    // selecting "BKB-3G" and waiting for the white LED to stop blinking; it
    // does not require a displayed passkey. Requiring MITM (Level 3) caused
    // ERROR_CODE_INSUFFICIENT_SECURITY (0x2f) before association completed.
    // Request Level 2 instead: authenticated/encrypted Classic link with
    // bonding, but no MITM requirement.
    const int status = gap_dedicated_bonding(g_target_addr, 0);
    if (status != ERROR_CODE_SUCCESS) {
        poc_logf("ERROR: gap_dedicated_bonding immediate status 0x%02x",
                 (unsigned)status);
        start_inquiry();
        return;
    }

    poc_logf("BOND: ACL/authentication procedure started");
}

static void connect_target(const bd_addr_t address) {
    memcpy(g_target_addr, address, sizeof(bd_addr_t));
    gap_inquiry_stop();

    g_app_state = APP_CONNECTING;
    g_hid_descriptor_available = false;

    poc_logf("CONNECT: bonding complete; opening Bluetooth Classic HID host connection");

    uint16_t new_cid = 0u;
    const uint8_t status =
        hid_host_connect(g_target_addr, HID_PROTOCOL_MODE_REPORT, &new_cid);

    if (status != ERROR_CODE_SUCCESS) {
        poc_logf("ERROR: hid_host_connect immediate status 0x%02x", status);
        g_hid_host_cid = 0u;
        start_inquiry();
        return;
    }

    g_hid_host_cid = new_cid;
    poc_logf("CONNECT: HID cid allocated 0x%04x", new_cid);
}

static void request_next_remote_name(void) {
    g_app_state = APP_RESOLVING_NAMES;

    for (uint8_t i = 0u; i < g_device_count; ++i) {
        if (g_devices[i].name_state != DEVICE_NAME_UNKNOWN) continue;

        g_devices[i].name_state = DEVICE_NAME_REQUESTED;
        poc_logf("NAME: resolving %s", bd_addr_to_str(g_devices[i].address));

        const uint8_t status = gap_remote_name_request(
            g_devices[i].address,
            g_devices[i].page_scan_repetition_mode,
            (uint16_t)(g_devices[i].clock_offset | 0x8000u));

        if (status == ERROR_CODE_SUCCESS) return;

        poc_logf("NAME: request failed immediately, status 0x%02x", status);
        g_devices[i].name_state = DEVICE_NAME_RESOLVED;
    }

    poc_logf("SCAN: target absent from this cycle; restarting");
    start_inquiry();
}

static void handle_inquiry_result(uint8_t *packet) {
    if (g_app_state != APP_INQUIRY) return;

    bd_addr_t address;
    gap_event_inquiry_result_get_bd_addr(packet, address);

    if (device_index_for_address(address) >= 0) return;

    const uint32_t cod = gap_event_inquiry_result_get_class_of_device(packet);
    int rssi = 127;
    if (gap_event_inquiry_result_get_rssi_available(packet)) {
        rssi = (int8_t)gap_event_inquiry_result_get_rssi(packet);
    }

    if (gap_event_inquiry_result_get_name_available(packet)) {
        char name[249];
        uint16_t name_len = gap_event_inquiry_result_get_name_len(packet);
        if (name_len >= sizeof(name)) name_len = sizeof(name) - 1u;
        memcpy(name, gap_event_inquiry_result_get_name(packet), name_len);
        name[name_len] = '\0';

        poc_logf("FOUND: %s COD=0x%06lx RSSI=%d name='%s'",
                 bd_addr_to_str(address), (unsigned long)cod, rssi, name);

        if (target_name_matches(name)) {
            poc_logf("MATCH: target found in EIR");
            bond_target(address);
            return;
        }
    } else {
        poc_logf("FOUND: %s COD=0x%06lx RSSI=%d name=<not in EIR>",
                 bd_addr_to_str(address), (unsigned long)cod, rssi);
    }

    if (g_device_count >= MAX_DISCOVERED_DEVICES) {
        poc_logf("WARN: discovery table full; ignoring additional devices");
        return;
    }

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
    if (g_app_state != APP_RESOLVING_NAMES) return;

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
        poc_logf("NAME: %s -> '%s'", bd_addr_to_str(address), name);

        if (target_name_matches(name)) {
            poc_logf("MATCH: target found via remote-name request");
            bond_target(address);
            return;
        }
    } else {
        poc_logf("NAME: %s failed, status 0x%02x",
                 bd_addr_to_str(address), packet[2]);
    }

    request_next_remote_name();
}

static bool add_key_usage(uint8_t keys[6], uint8_t *key_count, uint16_t usage) {
    if (usage == 0u) return true;

    for (uint8_t i = 0u; i < *key_count; ++i) {
        if (keys[i] == (uint8_t)usage) return true;
    }

    if (*key_count >= 6u) return false;

    keys[*key_count] = (uint8_t)usage;
    ++(*key_count);
    return true;
}

static void normalize_and_enqueue_report(const uint8_t *report, uint16_t report_len) {
    if (!g_hid_descriptor_available ||
        report == NULL ||
        report_len < 2u ||
        report[0] != 0xA1u) {
        return;
    }

    const uint16_t cid = g_hid_host_cid;
    const uint8_t *descriptor = hid_descriptor_storage_get_descriptor_data(cid);
    const uint16_t descriptor_len = hid_descriptor_storage_get_descriptor_len(cid);
    if (descriptor == NULL || descriptor_len == 0u) return;

    uint8_t usb_report[POC_USB_KEYBOARD_REPORT_LEN] = {0};
    uint8_t key_count = 0u;
    bool saw_keyboard_page = false;
    bool rollover = false;

    btstack_hid_parser_t parser;
    btstack_hid_parser_init(
        &parser,
        descriptor,
        descriptor_len,
        HID_REPORT_TYPE_INPUT,
        &report[1],
        (uint16_t)(report_len - 1u));

    while (btstack_hid_parser_has_more(&parser)) {
        uint16_t usage_page;
        uint16_t usage;
        int32_t value;
        btstack_hid_parser_get_field(&parser, &usage_page, &usage, &value);

        if (usage_page != 0x0007u) continue;

        saw_keyboard_page = true;
        if (value == 0) continue;

        if (usage >= 0x00E0u && usage <= 0x00E7u) {
            usb_report[0] |= (uint8_t)(1u << (usage - 0x00E0u));
            continue;
        }

        if (usage > 0x00FFu ||
            !add_key_usage(&usb_report[2], &key_count, usage)) {
            rollover = true;
        }
    }

    if (!saw_keyboard_page) {
        poc_logf("REPORT: no Keyboard usage page in this input report");
        return;
    }

    if (rollover) {
        for (uint8_t i = 2u; i < POC_USB_KEYBOARD_REPORT_LEN; ++i) {
            usb_report[i] = 0x01u;
        }
    }

    keyboard_queue_push(usb_report);
    poc_logf("USB-KBD: mod=%02x keys=%02x %02x %02x %02x %02x %02x",
             usb_report[0],
             usb_report[2], usb_report[3], usb_report[4],
             usb_report[5], usb_report[6], usb_report[7]);
}

static void handle_hid_report(uint8_t *packet) {
    const uint8_t *report = hid_subevent_report_get_report(packet);
    const uint16_t report_len = hid_subevent_report_get_report_len(packet);

    poc_log_hex("CLASSIC HID input", report, report_len);
    normalize_and_enqueue_report(report, report_len);
}

static void packet_handler(uint8_t packet_type,
                           uint16_t channel,
                           uint8_t *packet,
                           uint16_t size) {
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;

    bd_addr_t event_addr;
    const uint8_t event = hci_event_packet_get_type(packet);

    switch (event) {
        case BTSTACK_EVENT_STATE:
            if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING) {
                bd_addr_t local_addr;
                gap_local_bd_addr(local_addr);
                poc_logf("BTSTACK: HCI_STATE_WORKING local=%s",
                         bd_addr_to_str(local_addr));
                start_inquiry();
            }
            break;

        case GAP_EVENT_INQUIRY_RESULT:
            handle_inquiry_result(packet);
            break;

        case GAP_EVENT_INQUIRY_COMPLETE:
            if (g_app_state == APP_INQUIRY) {
                poc_logf("SCAN: inquiry complete, resolving missing names");
                request_next_remote_name();
            }
            break;

        case HCI_EVENT_REMOTE_NAME_REQUEST_COMPLETE:
            handle_remote_name_complete(packet);
            break;

        case GAP_EVENT_PAIRING_STARTED:
            poc_logf("PAIRING: started ssp=%u initiator=%u",
                     packet[10], packet[11]);
            break;

        case GAP_EVENT_PAIRING_COMPLETE:
            poc_logf("PAIRING: complete status=0x%02x", packet[10]);
            break;

        case GAP_EVENT_DEDICATED_BONDING_COMPLETED: {
            const uint8_t status = packet[2];
            poc_logf("BOND: dedicated bonding complete status=0x%02x", status);

            if (g_app_state != APP_BONDING) {
                poc_logf("BOND: completion received outside bonding state; ignoring");
                break;
            }

            if (status != ERROR_CODE_SUCCESS) {
                poc_logf("BOND: failed; keep keyboard in pairing mode and retrying discovery");
                g_hid_host_cid = 0u;
                g_hid_descriptor_available = false;
                start_inquiry();
                break;
            }

            poc_logf("BOND: authenticated link key established");
            connect_target(g_target_addr);
            break;
        }

        case HCI_EVENT_IO_CAPABILITY_REQUEST:
            poc_logf("SSP: controller requested our IO capability; local=NoInputNoOutput");
            break;

        case HCI_EVENT_IO_CAPABILITY_RESPONSE:
            hci_event_io_capability_response_get_bd_addr(packet, event_addr);
            poc_logf("SSP: remote IO capability from %s: io=0x%02x oob=0x%02x auth=0x%02x",
                     bd_addr_to_str(event_addr),
                     hci_event_io_capability_response_get_io_capability(packet),
                     hci_event_io_capability_response_get_oob_data_present(packet),
                     hci_event_io_capability_response_get_authentication_requirements(packet));
            break;

        case HCI_EVENT_PIN_CODE_REQUEST:
            hci_event_pin_code_request_get_bd_addr(packet, event_addr);
            poc_logf("PAIRING: legacy PIN requested by %s", bd_addr_to_str(event_addr));
            poc_logf("PAIRING: replying 0000; if required type 0000 + Enter on keyboard");
            gap_pin_code_response(event_addr, "0000");
            break;

        case HCI_EVENT_USER_CONFIRMATION_REQUEST: {
            const uint32_t value = little_endian_read_32(packet, 8);
            hci_event_user_confirmation_request_get_bd_addr(packet, event_addr);
            poc_logf("PAIRING: numeric confirmation %s = %06" PRIu32 " (accepting)",
                     bd_addr_to_str(event_addr), value);
            gap_ssp_confirmation_response(event_addr);
            break;
        }

        case HCI_EVENT_USER_PASSKEY_NOTIFICATION: {
            const uint32_t passkey = little_endian_read_32(packet, 8);
            poc_logf("PAIRING PASSKEY: %06" PRIu32, passkey);
            poc_logf("ACTION: type %06" PRIu32 " on BKB-3G then press Enter", passkey);
            break;
        }

        case HCI_EVENT_HID_META:
            switch (hci_event_hid_meta_get_subevent_code(packet)) {
                case HID_SUBEVENT_INCOMING_CONNECTION:
                    g_hid_host_cid =
                        hid_subevent_incoming_connection_get_hid_cid(packet);
                    g_app_state = APP_CONNECTING;
                    gap_inquiry_stop();
                    poc_logf("HID: incoming connection cid=0x%04x; accepting",
                             (uint16_t)g_hid_host_cid);
                    hid_host_accept_connection((uint16_t)g_hid_host_cid,
                                               HID_PROTOCOL_MODE_REPORT);
                    break;

                case HID_SUBEVENT_CONNECTION_OPENED: {
                    const uint8_t status =
                        hid_subevent_connection_opened_get_status(packet);
                    if (status != ERROR_CODE_SUCCESS) {
                        poc_logf("HID: connection failed status=0x%02x", status);
                        g_hid_host_cid = 0u;
                        g_hid_descriptor_available = false;
                        start_inquiry();
                        break;
                    }

                    g_hid_host_cid =
                        hid_subevent_connection_opened_get_hid_cid(packet);
                    g_app_state = APP_CONNECTED;
                    g_hid_descriptor_available = false;
                    poc_logf("HID: CONNECTION OPEN cid=0x%04x",
                             (uint16_t)g_hid_host_cid);
                    break;
                }

                case HID_SUBEVENT_DESCRIPTOR_AVAILABLE: {
                    const uint8_t status =
                        hid_subevent_descriptor_available_get_status(packet);
                    if (status != ERROR_CODE_SUCCESS) {
                        poc_logf("HID: descriptor unavailable status=0x%02x", status);
                        break;
                    }

                    const uint16_t cid = g_hid_host_cid;
                    const uint16_t descriptor_len =
                        hid_descriptor_storage_get_descriptor_len(cid);
                    const uint8_t *descriptor =
                        hid_descriptor_storage_get_descriptor_data(cid);

                    g_hid_descriptor_available = true;
                    poc_log_hex("HID report descriptor", descriptor, descriptor_len);
                    poc_logf("POC READY: Classic HID connected; USB keyboard path active");
                    break;
                }

                case HID_SUBEVENT_REPORT:
                    handle_hid_report(packet);
                    break;

                case HID_SUBEVENT_SET_PROTOCOL_RESPONSE: {
                    const uint8_t status =
                        hid_subevent_set_protocol_response_get_handshake_status(packet);
                    if (status == HID_HANDSHAKE_PARAM_TYPE_SUCCESSFUL) {
                        const uint8_t mode =
                            hid_subevent_set_protocol_response_get_protocol_mode(packet);
                        poc_logf("HID: protocol mode=%s",
                                 mode == HID_PROTOCOL_MODE_BOOT ? "BOOT" : "REPORT");
                    } else {
                        poc_logf("HID: set protocol failed status=0x%02x", status);
                    }
                    break;
                }

                case HID_SUBEVENT_CONNECTION_CLOSED: {
                    poc_logf("HID: connection closed; releasing USB keys");
                    const uint8_t neutral[POC_USB_KEYBOARD_REPORT_LEN] = {0};
                    keyboard_queue_push(neutral);
                    g_hid_host_cid = 0u;
                    g_hid_descriptor_available = false;
                    start_inquiry();
                    break;
                }

                case HID_SUBEVENT_SNIFF_SUBRATING_PARAMS:
                    // Informational HID event. It is not a pairing/connection error.
                    poc_logf("HID: sniff-subrating parameters received");
                    break;

                default:
                    poc_logf("HID: unhandled subevent 0x%02x",
                             hci_event_hid_meta_get_subevent_code(packet));
                    break;
            }
            break;

        default:
            break;
    }
}

static void classic_hid_init(void) {
    l2cap_init();

    hid_host_init(g_hid_descriptor_storage, sizeof(g_hid_descriptor_storage));
    hid_host_register_packet_handler(packet_handler);

    gap_set_default_link_policy_settings(
        LM_LINK_POLICY_ENABLE_SNIFF_MODE | LM_LINK_POLICY_ENABLE_ROLE_SWITCH);
    hci_set_master_slave_policy(HCI_ROLE_MASTER);
    hci_set_inquiry_mode(INQUIRY_MODE_RSSI_AND_EIR);

    gap_set_bondable_mode(1);

    // Goldentec GT T1 / 40062 ("BKB-3G") pairs without user-entered SSP
    // credentials according to its user manual. Advertise no local I/O and
    // allow Just Works / Level 2 bonding instead of forcing Passkey Entry.
    gap_ssp_set_io_capability(SSP_IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    gap_ssp_set_authentication_requirement(
        SSP_IO_AUTHREQ_MITM_PROTECTION_NOT_REQUIRED_GENERAL_BONDING);
    gap_ssp_set_auto_accept(1);

    gap_set_local_name("RP2350 Classic HID POC 00:00:00:00:00:00");
    gap_discoverable_control(1);

    g_hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&g_hci_event_callback_registration);

    hci_power_control(HCI_POWER_ON);
}

void classic_hid_core_main(void) {
    poc_logf("CORE1: starting CYW43 + BTstack Classic HID Host");

    const int cyw43_status = cyw43_arch_init();
    if (cyw43_status != PICO_OK) {
        poc_logf("FATAL: cyw43_arch_init failed status=%d", cyw43_status);
        while (true) {
            tight_loop_contents();
        }
    }

    poc_logf("CORE1: CYW43 initialized");
    classic_hid_init();
    btstack_run_loop_execute();

    // Not expected to return.
    poc_logf("FATAL: btstack_run_loop_execute returned");
    while (true) {
        tight_loop_contents();
    }
}
