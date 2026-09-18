#ifndef BLU2USB_FORK_BLE_MOUSE_H
#define BLU2USB_FORK_BLE_MOUSE_H

#include <stdbool.h>
#include <stdint.h>

void ble_mouse_init(void);
void ble_mouse_handle_command(uint16_t command_type);
bool ble_mouse_is_ready(void);

#endif
