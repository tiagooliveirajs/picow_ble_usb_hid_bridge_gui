#ifndef BLU2USB_FORK_UI_RUNTIME_H
#define BLU2USB_FORK_UI_RUNTIME_H

#include <stdbool.h>
#include <stdint.h>

bool ui_runtime_init(void);
void ui_runtime_task(void);
void ui_runtime_on_bt_event(uint16_t event_type);

#endif
