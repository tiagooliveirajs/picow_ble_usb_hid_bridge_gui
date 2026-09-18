#ifndef BLU2USB_FORK_CLASSIC_KEYBOARD_H
#define BLU2USB_FORK_CLASSIC_KEYBOARD_H

#include <stdbool.h>
#include <stdint.h>

void classic_keyboard_init(void);
void classic_keyboard_on_stack_working(void);
void classic_keyboard_handle_command(uint16_t command_type);
bool classic_keyboard_is_ready(void);

#endif
