#include "hat.h"
#include <stddef.h>
#include <stdint.h>

typedef struct { blu2usb_control_t control; uint8_t pin; } hat_pin_map_t;
static const hat_pin_map_t k_pin_map[] = {
    {BLU2USB_CONTROL_JOY_UP,BLU2USB_HAT_PIN_JOY_UP},
    {BLU2USB_CONTROL_JOY_DOWN,BLU2USB_HAT_PIN_JOY_DOWN},
    {BLU2USB_CONTROL_JOY_LEFT,BLU2USB_HAT_PIN_JOY_LEFT},
    {BLU2USB_CONTROL_JOY_RIGHT,BLU2USB_HAT_PIN_JOY_RIGHT},
    {BLU2USB_CONTROL_JOY_PRESS,BLU2USB_HAT_PIN_JOY_PRESS},
    {BLU2USB_CONTROL_KEY_A,BLU2USB_HAT_PIN_KEY_A},
    {BLU2USB_CONTROL_KEY_B,BLU2USB_HAT_PIN_KEY_B},
    {BLU2USB_CONTROL_KEY_X,BLU2USB_HAT_PIN_KEY_X},
    {BLU2USB_CONTROL_KEY_Y,BLU2USB_HAT_PIN_KEY_Y}
};

bool blu2usb_hat_control_for_pin(uint8_t pin, blu2usb_control_t *control) {
    if (control == NULL) return false;
    for (size_t i = 0; i < sizeof(k_pin_map)/sizeof(k_pin_map[0]); ++i) {
        if (k_pin_map[i].pin == pin) { *control = k_pin_map[i].control; return true; }
    }
    return false;
}

uint8_t blu2usb_hat_pin_for_control(blu2usb_control_t control) {
    for (size_t i = 0; i < sizeof(k_pin_map)/sizeof(k_pin_map[0]); ++i)
        if (k_pin_map[i].control == control) return k_pin_map[i].pin;
    return UINT8_MAX;
}
