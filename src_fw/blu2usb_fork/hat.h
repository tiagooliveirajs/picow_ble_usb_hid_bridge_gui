#ifndef BLU2USB_FORK_HAT_H
#define BLU2USB_FORK_HAT_H

#include <stdbool.h>
#include <stdint.h>
#include "ui_control.h"

#define BLU2USB_HAT_PIN_JOY_UP 2u
#define BLU2USB_HAT_PIN_JOY_PRESS 3u
#define BLU2USB_HAT_PIN_KEY_A 15u
#define BLU2USB_HAT_PIN_JOY_LEFT 16u
#define BLU2USB_HAT_PIN_KEY_B 17u
#define BLU2USB_HAT_PIN_JOY_DOWN 18u
#define BLU2USB_HAT_PIN_KEY_X 19u
#define BLU2USB_HAT_PIN_JOY_RIGHT 20u
#define BLU2USB_HAT_PIN_KEY_Y 21u

typedef struct {
    blu2usb_control_t control;
    bool pressed;
} blu2usb_hat_event_t;

bool blu2usb_hat_control_for_pin(uint8_t pin, blu2usb_control_t *control);
uint8_t blu2usb_hat_pin_for_control(blu2usb_control_t control);
void blu2usb_hat_pico_init(void);
void blu2usb_hat_pico_task(void);
bool blu2usb_hat_pico_poll_event(blu2usb_hat_event_t *event);

#endif
