#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "hat.h"
#include "ui_model.h"
#include "ui_renderer.h"

typedef struct { unsigned fills; unsigned writes; uint16_t colors[4]; } fake_display_t;
static bool fake_fill(void *ctx,uint16_t x,uint16_t y,uint16_t w,uint16_t h,uint16_t color){fake_display_t *f=ctx;(void)y;(void)h;assert(x==0u&&w==BLU2USB_RENDERER_WIDTH);if(f->fills<4u)f->colors[f->fills]=color;f->fills++;return true;}
static bool fake_write(void *ctx,uint16_t x,uint16_t y,uint16_t w,uint16_t h,const uint16_t *pixels){fake_display_t *f=ctx;assert(x<240u&&y<240u);assert(w==10u&&h==14u&&pixels!=0);f->writes++;return true;}
static void assert_text(const blu2usb_ui_frame_t *frame,unsigned row,unsigned col,const char *text){for(size_t i=0;text[i];++i)assert(frame->cells[row][col+i].character==text[i]);}
static void send(blu2usb_ux_model_t *ux,blu2usb_control_t c,bool pressed){(void)blu2usb_ux_input(ux,c,pressed);}
static void click(blu2usb_ux_model_t *ux,blu2usb_control_t c){send(ux,c,true);send(ux,c,false);}

static void test_geometry_colors_and_hat(void){
    assert(BLU2USB_RENDERER_TEXT_ROWS==9u&&BLU2USB_RENDERER_TEXT_COLS==21u);
    assert(BLU2USB_RENDERER_TEXT_X==7u&&BLU2USB_RENDERER_TEXT_Y==8u);
    assert(BLU2USB_RENDERER_GLYPH_WIDTH==10u&&BLU2USB_RENDERER_GLYPH_HEIGHT==14u&&BLU2USB_RENDERER_CHAR_ADVANCE==11u);
    assert(BLU2USB_RENDERER_TITLE_BODY_GAP==17u&&BLU2USB_RENDERER_BODY_LINE_GAP==12u&&BLU2USB_RENDERER_HINT_TOP_GAP==11u&&BLU2USB_RENDERER_HINT_BOTTOM_GAP==12u&&BLU2USB_RENDERER_LEARN_LINE_GAP==11u);
    assert(blu2usb_renderer_tone_rgb565(BLU2USB_UI_TONE_TITLE)==BLU2USB_COLOR_MAGENTA);
    assert(blu2usb_renderer_tone_rgb565(BLU2USB_UI_TONE_STATIC)==BLU2USB_COLOR_OFF_WHITE_YELLOW);
    assert(blu2usb_renderer_tone_rgb565(BLU2USB_UI_TONE_ACTIONABLE)==BLU2USB_COLOR_LIGHT_GRAY);
    assert(blu2usb_renderer_tone_rgb565(BLU2USB_UI_TONE_EMPHASIZED)==BLU2USB_COLOR_WHITE);
    assert(blu2usb_renderer_tone_rgb565(BLU2USB_UI_TONE_CURRENT)==BLU2USB_COLOR_CYAN);
    blu2usb_control_t c=BLU2USB_CONTROL_COUNT;
    assert(blu2usb_hat_control_for_pin(2,&c)&&c==BLU2USB_CONTROL_JOY_UP);assert(blu2usb_hat_control_for_pin(3,&c)&&c==BLU2USB_CONTROL_JOY_PRESS);assert(blu2usb_hat_control_for_pin(15,&c)&&c==BLU2USB_CONTROL_KEY_A);assert(blu2usb_hat_control_for_pin(16,&c)&&c==BLU2USB_CONTROL_JOY_LEFT);assert(blu2usb_hat_control_for_pin(17,&c)&&c==BLU2USB_CONTROL_KEY_B);assert(blu2usb_hat_control_for_pin(18,&c)&&c==BLU2USB_CONTROL_JOY_DOWN);assert(blu2usb_hat_control_for_pin(19,&c)&&c==BLU2USB_CONTROL_KEY_X);assert(blu2usb_hat_control_for_pin(20,&c)&&c==BLU2USB_CONTROL_JOY_RIGHT);assert(blu2usb_hat_control_for_pin(21,&c)&&c==BLU2USB_CONTROL_KEY_Y);
}

static void test_relocated_geometry(void){blu2usb_ux_model_t ux;blu2usb_ui_frame_t f;blu2usb_ux_init(&ux);blu2usb_ui_project(&ux,&f);assert(f.hint_start_row==6u);assert(blu2usb_renderer_text_y(&f,0)==8u);assert(blu2usb_renderer_text_y(&f,1)==39u);assert(blu2usb_renderer_text_y(&f,2)==65u);assert(blu2usb_renderer_text_y(&f,6)==162u);assert(blu2usb_renderer_text_y(&f,7)==188u);assert(blu2usb_renderer_text_y(&f,8)==214u);assert(blu2usb_renderer_separator_boundary_y(&f)==151u);ux.screen=BLU2USB_SCREEN_LEARN_KEYS;blu2usb_ui_project(&ux,&f);assert(blu2usb_renderer_text_y(&f,1)==39u);assert(blu2usb_renderer_text_y(&f,2)==64u);assert(blu2usb_renderer_text_y(&f,8)==214u);}

static void test_release_navigation_help_and_lock(void){
    blu2usb_ux_model_t ux;blu2usb_ui_frame_t f;blu2usb_ux_init(&ux);
    send(&ux,BLU2USB_CONTROL_JOY_DOWN,true);assert(ux.selection==0u);send(&ux,BLU2USB_CONTROL_JOY_DOWN,false);assert(ux.selection==1u);
    click(&ux,BLU2USB_CONTROL_JOY_PRESS);assert(ux.screen==BLU2USB_SCREEN_MOUSE_OPTIONS);click(&ux,BLU2USB_CONTROL_KEY_B);assert(ux.screen==BLU2USB_SCREEN_HOME);
    ux.screen=BLU2USB_SCREEN_PAIR_MOUSE;blu2usb_ui_project(&ux,&f);assert_text(&f,8,0,"KEY X: HELP");click(&ux,BLU2USB_CONTROL_KEY_X);assert(ux.screen==BLU2USB_SCREEN_PAIR_MOUSE_HELP);click(&ux,BLU2USB_CONTROL_KEY_Y);assert(ux.screen==BLU2USB_SCREEN_PAIR_MOUSE);assert(!blu2usb_interaction_is_locked(&ux.interaction));
    ux.screen=BLU2USB_SCREEN_HOME;click(&ux,BLU2USB_CONTROL_KEY_Y);assert(blu2usb_interaction_is_locked(&ux.interaction));click(&ux,BLU2USB_CONTROL_JOY_LEFT);assert(!blu2usb_interaction_is_locked(&ux.interaction)&&ux.screen==BLU2USB_SCREEN_HOME);
}

static void test_learn_and_render(void){blu2usb_ux_model_t ux;blu2usb_ui_frame_t f;blu2usb_ux_init(&ux);ux.screen=BLU2USB_SCREEN_LEARN_KEYS;blu2usb_ui_project(&ux,&f);assert_text(&f,0,0,"PRESS TO LEARN A KEY");assert(f.cells[5][15].character=='K'&&f.cells[6][15].character=='K'&&f.cells[7][15].character=='K');send(&ux,BLU2USB_CONTROL_KEY_A,true);blu2usb_ui_project(&ux,&f);assert(f.cells[5][15].tone==BLU2USB_UI_TONE_EMPHASIZED);send(&ux,BLU2USB_CONTROL_KEY_A,false);send(&ux,BLU2USB_CONTROL_KEY_Y,true);send(&ux,BLU2USB_CONTROL_KEY_Y,false);assert(blu2usb_interaction_is_locked(&ux.interaction));click(&ux,BLU2USB_CONTROL_KEY_X);assert(!blu2usb_interaction_is_locked(&ux.interaction)&&ux.screen==BLU2USB_SCREEN_HOME);blu2usb_ui_project(&ux,&f);fake_display_t fake={0};const blu2usb_display_hal_t d={&fake,fake_fill,fake_write};assert(blu2usb_renderer_render(&d,&f));assert(fake.fills==2u&&fake.colors[0]==BLU2USB_COLOR_BLACK&&fake.colors[1]==BLU2USB_COLOR_DARK_MAGENTA&&fake.writes>0u);}

static void test_wrap_and_hidden_back(void){blu2usb_ux_model_t ux;blu2usb_ux_init(&ux);ux.screen=BLU2USB_SCREEN_OTHER_OPTIONS;ux.selection=0u;click(&ux,BLU2USB_CONTROL_JOY_UP);assert(ux.selection==2u);click(&ux,BLU2USB_CONTROL_JOY_DOWN);assert(ux.selection==0u);ux.screen=BLU2USB_SCREEN_LEFT_WILL_BECOME;click(&ux,BLU2USB_CONTROL_KEY_B);assert(ux.screen==BLU2USB_SCREEN_EDIT_CUSTOM);ux.screen=BLU2USB_SCREEN_APPLY_DEFAULT;click(&ux,BLU2USB_CONTROL_KEY_B);assert(ux.screen==BLU2USB_SCREEN_MOUSE_OPTIONS);}

int main(void){test_geometry_colors_and_hat();test_relocated_geometry();test_release_navigation_help_and_lock();test_learn_and_render();test_wrap_and_hidden_back();return 0;}
