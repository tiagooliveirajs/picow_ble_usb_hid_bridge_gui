#ifndef BLU2USB_FORK_BT_RUNTIME_H
#define BLU2USB_FORK_BT_RUNTIME_H

#include <stdint.h>

#define BT_COMMAND_PING 1u

#define BT_EVENT_STACK_WORKING 1u
#define BT_EVENT_COMMAND_ACK 2u
#define BT_EVENT_STACK_ERROR 3u

void bt_runtime_core1_main(void);

#endif
