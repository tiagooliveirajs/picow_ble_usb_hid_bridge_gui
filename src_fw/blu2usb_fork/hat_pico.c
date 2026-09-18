#include "hat.h"
#include <stddef.h>
#include <stdint.h>
#include "hardware/gpio.h"
#include "pico/stdlib.h"

#define HAT_DEBOUNCE_MS 20u
#define HAT_SCAN_PERIOD_MS 1u
#define HAT_EVENT_QUEUE_LENGTH 32u

typedef struct {
    blu2usb_control_t control;
    uint8_t pin;
    bool raw_pressed;
    bool stable_pressed;
    uint32_t raw_changed_ms;
} input_state_t;

static input_state_t g_inputs[BLU2USB_CONTROL_COUNT];
static blu2usb_hat_event_t g_events[HAT_EVENT_QUEUE_LENGTH];
static uint8_t g_read_index;
static uint8_t g_write_index;
static uint32_t g_last_scan_ms;

static uint32_t now_ms(void) { return to_ms_since_boot(get_absolute_time()); }
static bool enqueue(blu2usb_control_t control,bool pressed) {
    const uint8_t next=(uint8_t)((g_write_index+1u)%HAT_EVENT_QUEUE_LENGTH);
    if(next==g_read_index)return false;
    g_events[g_write_index].control=control;
    g_events[g_write_index].pressed=pressed;
    g_write_index=next;
    return true;
}

void blu2usb_hat_pico_init(void) {
    const uint32_t now=now_ms();
    for(uint8_t i=0u;i<BLU2USB_CONTROL_COUNT;++i){
        input_state_t *input=&g_inputs[i];
        input->control=(blu2usb_control_t)i;
        input->pin=blu2usb_hat_pin_for_control(input->control);
        gpio_init(input->pin);gpio_set_dir(input->pin,GPIO_IN);gpio_pull_up(input->pin);
        input->raw_pressed=!gpio_get(input->pin);input->stable_pressed=input->raw_pressed;input->raw_changed_ms=now;
    }
    g_read_index=0u;g_write_index=0u;g_last_scan_ms=now;
}

void blu2usb_hat_pico_task(void) {
    const uint32_t now=now_ms(); if((uint32_t)(now-g_last_scan_ms)<HAT_SCAN_PERIOD_MS)return; g_last_scan_ms=now;
    for(uint8_t i=0u;i<BLU2USB_CONTROL_COUNT;++i){
        input_state_t *input=&g_inputs[i];const bool pressed=!gpio_get(input->pin);
        if(pressed!=input->raw_pressed){input->raw_pressed=pressed;input->raw_changed_ms=now;}
        if(pressed!=input->stable_pressed&&(uint32_t)(now-input->raw_changed_ms)>=HAT_DEBOUNCE_MS){input->stable_pressed=pressed;(void)enqueue(input->control,pressed);}
    }
}

bool blu2usb_hat_pico_poll_event(blu2usb_hat_event_t *event) {
    if(event==NULL||g_read_index==g_write_index)return false;
    *event=g_events[g_read_index];g_read_index=(uint8_t)((g_read_index+1u)%HAT_EVENT_QUEUE_LENGTH);return true;
}
