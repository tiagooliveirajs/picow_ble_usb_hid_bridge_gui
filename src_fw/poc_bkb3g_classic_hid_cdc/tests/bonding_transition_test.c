// Exercises production transition code with the SDK's real BTstack headers.
// HCI/USB hardware is not emulated; only the run-loop scheduling boundary is.
#include <assert.h>
#include <stdio.h>
#include "../classic_hid.c"

static btstack_context_callback_registration_t *pending;
static bool old_acl_present;
static unsigned connects, scans, queued, warnings;
static uint8_t connect_status;

void poc_logf(const char *format, ...) {
    if (strncmp(format, "DIAG:", 5) == 0) ++warnings;
}
bool sdp_client_ready(void) { return true; }
int gap_inquiry_stop(void) { return ERROR_CODE_SUCCESS; }
int gap_inquiry_start(uint8_t duration) {
    assert(duration == INQUIRY_DURATION_1280MS);
    ++scans;
    return ERROR_CODE_SUCCESS;
}
void btstack_run_loop_execute_on_main_thread(btstack_context_callback_registration_t *cb) {
    assert(pending == NULL);
    pending = cb;
    ++queued;
}
int btstack_run_loop_remove_timer(btstack_timer_source_t *timer) {
    (void)timer;
    return true;
}
void btstack_run_loop_set_timer_handler(btstack_timer_source_t *timer,
                                       void (*process)(btstack_timer_source_t *)) {
    timer->process = process;
}
void btstack_run_loop_set_timer(btstack_timer_source_t *timer, uint32_t timeout) {
    assert(timeout == 30000u);
    timer->timeout = timeout;
}
void btstack_run_loop_add_timer(btstack_timer_source_t *timer) { (void)timer; }
uint8_t hid_host_connect(bd_addr_t addr, hid_protocol_mode_t mode, uint16_t *cid) {
    // A synchronous launch from the bonding event violates this invariant.
    assert(!old_acl_present);
    assert(bd_addr_cmp(addr, g_target_addr) == 0);
    assert(mode == HID_PROTOCOL_MODE_REPORT);
    ++connects;
    *cid = 1;
    return connect_status;
}

static void reset_test(void) {
    g_app_state = APP_BONDING;
    const bd_addr_t address = {0x20, 0x20, 0x01, 0x60, 0x0b, 0x94};
    memcpy(g_target_addr, address, sizeof(address));
    g_hid_host_cid = 0;
    g_hid_descriptor_available = false;
    pending = NULL;
    old_acl_present = true;
    connects = scans = queued = warnings = 0;
    connect_status = ERROR_CODE_SUCCESS;
}
static void complete_bond(uint8_t status) {
    uint8_t event[9] = {GAP_EVENT_DEDICATED_BONDING_COMPLETED, 7, status};
    reverse_bd_addr(g_target_addr, &event[3]);
    handle_bonding_complete(event);
}
static void drain_callback(void) {
    assert(pending != NULL);
    old_acl_present = false; // HCI handler has now completed its cleanup.
    btstack_context_callback_registration_t *cb = pending;
    pending = NULL;
    cb->callback(cb->context);
}
int main(void) {
    reset_test();
    complete_bond(ERROR_CODE_SUCCESS);
    assert(connects == 0 && queued == 1 && g_app_state == APP_WAITING_FOR_HID_START);
    complete_bond(ERROR_CODE_SUCCESS); // Duplicate must not queue twice.
    assert(queued == 1);
    drain_callback();
    assert(connects == 1 && g_app_state == APP_CONNECTING && g_hid_host_cid == 1);
    connect_diagnostic_timeout(&g_connect_diagnostic_timer);
    assert(warnings == 1 && scans == 0 && connects == 1);

    reset_test();
    complete_bond(ERROR_CODE_INSUFFICIENT_SECURITY);
    assert(pending == NULL && connects == 0 && scans == 1);

    reset_test();
    uint8_t other[9] = {GAP_EVENT_DEDICATED_BONDING_COMPLETED, 7, 0};
    handle_bonding_complete(other);
    assert(pending == NULL && g_app_state == APP_BONDING);

    reset_test();
    complete_bond(ERROR_CODE_SUCCESS);
    g_app_state = APP_CONNECTING; // Incoming HID won before queued callback.
    g_hid_host_cid = 7;
    drain_callback();
    assert(connects == 0 && g_hid_host_cid == 7);

    reset_test();
    complete_bond(ERROR_CODE_SUCCESS);
    connect_status = BTSTACK_MEMORY_ALLOC_FAILED;
    drain_callback();
    assert(connects == 1 && scans == 1 && g_hid_host_cid == 0);

    reset_test();
    g_app_state = APP_CONNECTED;
    g_hid_descriptor_available = true;
    connect_diagnostic_timeout(&g_connect_diagnostic_timer);
    assert(warnings == 0);
    puts("PASS: deferred launch, duplicate/foreign/failure guards, incoming race, immediate error, diagnostics");
}
