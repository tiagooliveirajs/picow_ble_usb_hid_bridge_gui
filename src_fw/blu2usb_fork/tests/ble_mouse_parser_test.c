#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "ble_mouse_parser.h"

typedef struct { canonical_mouse_event_t events[32]; size_t count; } sink_t;
static bool emit(void *ctx,const canonical_mouse_event_t *event){sink_t *s=ctx;if(!s||!event||s->count>=32u)return false;s->events[s->count++]=*event;return true;}

static const uint8_t k_composite_report_map[] = {
    0x05,0x01,0x09,0x06,0xa1,0x01,0x85,0x01,
    0x05,0x07,0x19,0xe0,0x29,0xe7,0x15,0x00,
    0x25,0x01,0x75,0x01,0x95,0x08,0x81,0x02,
    0x95,0x01,0x75,0x08,0x81,0x01,0xc0,
    0x05,0x01,0x09,0x02,0xa1,0x01,0x85,0x02,
    0x09,0x01,0xa1,0x00,
    0x05,0x09,0x19,0x01,0x29,0x05,0x15,0x00,
    0x25,0x01,0x95,0x05,0x75,0x01,0x81,0x02,
    0x95,0x01,0x75,0x03,0x81,0x01,
    0x05,0x01,0x09,0x30,0x09,0x31,0x09,0x38,
    0x15,0x81,0x25,0x7f,0x75,0x08,0x95,0x03,0x81,0x06,
    0x05,0x0c,0x0a,0x38,0x02,0x15,0x81,0x25,0x7f,
    0x75,0x08,0x95,0x01,0x81,0x06,
    0xc0,0xc0,
};
static const uint8_t k_keyboard_only_report_map[]={0x05,0x01,0x09,0x06,0xa1,0x01,0x85,0x01,0x05,0x07,0x19,0xe0,0x29,0xe7,0x15,0x00,0x25,0x01,0x75,0x01,0x95,0x08,0x81,0x02,0xc0};

int main(void){
    ble_mouse_parser_t p;
    canonical_source_t src=canonical_source_make(CANONICAL_SOURCE_KIND_BLE_HOGP_MOUSE,1u);
    assert(ble_mouse_parser_configure(&p,src,k_composite_report_map,sizeof(k_composite_report_map)));
    assert(ble_mouse_parser_has_mouse(&p));assert(p.report_count==2u);assert(p.field_count==9u);
    sink_t s={0};
    const uint8_t keyboard[2]={0,0};assert(ble_mouse_parser_parse_report(&p,1u,keyboard,sizeof(keyboard),emit,&s));assert(s.count==0u);
    const uint8_t mouse[5]={0x11u,10u,(uint8_t)-5,1u,(uint8_t)-2};assert(ble_mouse_parser_parse_report(&p,2u,mouse,sizeof(mouse),emit,&s));assert(s.count==4u);
    assert(s.events[0].type==CANONICAL_MOUSE_EVENT_BUTTON&&s.events[0].data.button.button==CANONICAL_MOUSE_BUTTON_LEFT&&s.events[0].data.button.pressed);
    assert(s.events[1].type==CANONICAL_MOUSE_EVENT_BUTTON&&s.events[1].data.button.button==CANONICAL_MOUSE_BUTTON_FORWARD&&s.events[1].data.button.pressed);
    assert(s.events[2].type==CANONICAL_MOUSE_EVENT_MOVE&&s.events[2].data.move.dx==10&&s.events[2].data.move.dy==-5);
    assert(s.events[3].type==CANONICAL_MOUSE_EVENT_WHEEL&&s.events[3].data.wheel.vertical==1&&s.events[3].data.wheel.horizontal==-2);
    const uint8_t released[5]={0,0,0,0,0};size_t before=s.count;assert(ble_mouse_parser_parse_report(&p,2u,released,sizeof(released),emit,&s));assert(s.count==before+2u);assert(!s.events[before].data.button.pressed&&!s.events[before+1].data.button.pressed);
    const uint8_t framed[6]={2u,1u,3u,(uint8_t)-4,0u,0u};const uint8_t *payload=NULL;size_t payload_len=0;assert(ble_mouse_parser_normalize_report(&p,2u,framed,sizeof(framed),&payload,&payload_len));assert(payload==&framed[1]&&payload_len==5u);
    uint8_t bad[6];memcpy(bad,framed,sizeof(bad));bad[0]=7u;assert(!ble_mouse_parser_normalize_report(&p,2u,bad,sizeof(bad),&payload,&payload_len));
    assert(!ble_mouse_parser_configure(&p,src,k_keyboard_only_report_map,sizeof(k_keyboard_only_report_map)));
    return 0;
}
