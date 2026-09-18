#ifndef BLU2USB_FORK_STORAGE_OWNER_H
#define BLU2USB_FORK_STORAGE_OWNER_H

#include <stdbool.h>
#include <stdint.h>

typedef void (*storage_owner_flash_callback_t)(void *context);

bool storage_owner_init_core0(void);
bool storage_owner_flash_safe_execute(
    storage_owner_flash_callback_t callback,
    void *context,
    uint32_t timeout_ms);

#endif
