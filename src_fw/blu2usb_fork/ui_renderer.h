#ifndef BLU2USB_FORK_UI_RENDERER_H
#define BLU2USB_FORK_UI_RENDERER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "ui_model.h"

#define BLU2USB_RENDERER_WIDTH 240u
#define BLU2USB_RENDERER_HEIGHT 240u
#define BLU2USB_RENDERER_TEXT_ROWS 9u
#define BLU2USB_RENDERER_TEXT_COLS 21u
#define BLU2USB_RENDERER_GLYPH_SCALE 2u
#define BLU2USB_RENDERER_GLYPH_WIDTH 10u
#define BLU2USB_RENDERER_GLYPH_HEIGHT 14u
#define BLU2USB_RENDERER_CHAR_ADVANCE 11u
#define BLU2USB_RENDERER_LINE_ADVANCE 27u
#define BLU2USB_RENDERER_TEXT_X 7u
#define BLU2USB_RENDERER_TEXT_Y 8u
#define BLU2USB_RENDERER_TITLE_BODY_GAP 17u
#define BLU2USB_RENDERER_BODY_LINE_GAP 12u
#define BLU2USB_RENDERER_BODY_BOTTOM_GAP 20u
#define BLU2USB_RENDERER_HINT_TOP_GAP 11u
#define BLU2USB_RENDERER_HINT_LINE_GAP 12u
#define BLU2USB_RENDERER_HINT_BOTTOM_GAP 12u
#define BLU2USB_RENDERER_LEARN_LINE_GAP 11u
#define BLU2USB_RENDERER_LEARN_BOTTOM_GAP 12u

#define BLU2USB_COLOR_BLACK 0x0000u
#define BLU2USB_COLOR_WHITE 0xffffu
#define BLU2USB_COLOR_MAGENTA 0xf81fu
#define BLU2USB_COLOR_CYAN 0x07ffu
#define BLU2USB_COLOR_OFF_WHITE_YELLOW 0xffb8u
#define BLU2USB_COLOR_LIGHT_GRAY 0xc618u
#define BLU2USB_COLOR_DARK_MAGENTA 0x0801u

typedef enum { BLU2USB_UI_TONE_TITLE=0,BLU2USB_UI_TONE_STATIC,BLU2USB_UI_TONE_ACTIONABLE,BLU2USB_UI_TONE_EMPHASIZED,BLU2USB_UI_TONE_CURRENT } blu2usb_ui_tone_t;
typedef struct { char character; blu2usb_ui_tone_t tone; } blu2usb_ui_cell_t;
typedef struct { blu2usb_ui_cell_t cells[BLU2USB_RENDERER_TEXT_ROWS][BLU2USB_RENDERER_TEXT_COLS]; bool learn_background; uint8_t hint_start_row; } blu2usb_ui_frame_t;
typedef struct { void *context; bool (*fill_rect)(void *,uint16_t,uint16_t,uint16_t,uint16_t,uint16_t); bool (*write_rgb565)(void *,uint16_t,uint16_t,uint16_t,uint16_t,const uint16_t *); } blu2usb_display_hal_t;

void blu2usb_ui_frame_reset(blu2usb_ui_frame_t *frame,bool learn_background,uint8_t hint_start_row);
bool blu2usb_ui_frame_set_text(blu2usb_ui_frame_t *frame,uint8_t row,uint8_t column,const char *text,blu2usb_ui_tone_t tone);
bool blu2usb_ui_frame_set_tone_span(blu2usb_ui_frame_t *frame,uint8_t row,uint8_t column,uint8_t length,blu2usb_ui_tone_t tone);
void blu2usb_ui_project(const blu2usb_ux_model_t *ux,blu2usb_ui_frame_t *frame);
void blu2usb_ui_enforce_applied_visual_contract(const blu2usb_ux_model_t *ux,blu2usb_ui_frame_t *frame);
uint16_t blu2usb_renderer_tone_rgb565(blu2usb_ui_tone_t tone);
uint16_t blu2usb_renderer_background_rgb565(const blu2usb_ui_frame_t *frame,uint8_t row);
uint16_t blu2usb_renderer_separator_boundary_y(const blu2usb_ui_frame_t *frame);
uint16_t blu2usb_renderer_text_y(const blu2usb_ui_frame_t *frame,uint8_t row);
bool blu2usb_renderer_render(const blu2usb_display_hal_t *display,const blu2usb_ui_frame_t *frame);

#endif
