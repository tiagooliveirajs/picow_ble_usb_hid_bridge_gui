#include "storage_owner.h"

#include "pico/flash.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"

bool storage_owner_init_core0(void) {
    if (get_core_num() != 0u) return false;
    flash_safe_execute_core_init();
    return true;
}

bool storage_owner_flash_safe_execute(
    storage_owner_flash_callback_t callback,
    void *context,
    uint32_t timeout_ms) {
    if (get_core_num() != 0u || callback == NULL) return false;
    return flash_safe_execute(callback, context, timeout_ms) == PICO_OK;
}
