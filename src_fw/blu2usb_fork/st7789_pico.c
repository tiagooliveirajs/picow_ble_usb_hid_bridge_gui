#include "st7789_pico.h"

#include <stddef.h>
#include <stdint.h>
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"

#define LCD_SPI spi1
#define LCD_SPI_BAUD_HZ (10u * 1000u * 1000u)
#define LCD_PIN_DC 8u
#define LCD_PIN_CS 9u
#define LCD_PIN_SCK 10u
#define LCD_PIN_MOSI 11u
#define LCD_PIN_RST 12u
#define LCD_PIN_BL 13u

static void lcd_select(bool selected) { gpio_put(LCD_PIN_CS,!selected); }
static void lcd_command(uint8_t command) { gpio_put(LCD_PIN_DC,false); lcd_select(true); (void)spi_write_blocking(LCD_SPI,&command,1u); lcd_select(false); }
static void lcd_data(const uint8_t *data,size_t length) { if(data==NULL||length==0u)return; gpio_put(LCD_PIN_DC,true); lcd_select(true); (void)spi_write_blocking(LCD_SPI,data,length); lcd_select(false); }
static void lcd_data_u8(uint8_t value) { lcd_data(&value,1u); }
static void lcd_window(uint16_t x,uint16_t y,uint16_t width,uint16_t height) { uint8_t c[4]; const uint16_t x1=(uint16_t)(x+width-1u),y1=(uint16_t)(y+height-1u); lcd_command(0x2au); c[0]=(uint8_t)(x>>8); c[1]=(uint8_t)x; c[2]=(uint8_t)(x1>>8); c[3]=(uint8_t)x1; lcd_data(c,sizeof(c)); lcd_command(0x2bu); c[0]=(uint8_t)(y>>8); c[1]=(uint8_t)y; c[2]=(uint8_t)(y1>>8); c[3]=(uint8_t)y1; lcd_data(c,sizeof(c)); lcd_command(0x2cu); }
static bool valid_rect(uint16_t x,uint16_t y,uint16_t width,uint16_t height) { return width>0u&&height>0u&&x<BLU2USB_RENDERER_WIDTH&&y<BLU2USB_RENDERER_HEIGHT&&(uint32_t)x+width<=BLU2USB_RENDERER_WIDTH&&(uint32_t)y+height<=BLU2USB_RENDERER_HEIGHT; }
static bool pico_fill_rect(void *context,uint16_t x,uint16_t y,uint16_t width,uint16_t height,uint16_t rgb565) { (void)context; if(!valid_rect(x,y,width,height))return false; static uint8_t row[BLU2USB_RENDERER_WIDTH*2u]; for(uint16_t column=0u;column<width;++column){row[(size_t)column*2u]=(uint8_t)(rgb565>>8);row[(size_t)column*2u+1u]=(uint8_t)rgb565;} lcd_window(x,y,width,height); gpio_put(LCD_PIN_DC,true); lcd_select(true); for(uint16_t line=0u;line<height;++line)(void)spi_write_blocking(LCD_SPI,row,(size_t)width*2u); lcd_select(false); return true; }
static bool pico_write_rgb565(void *context,uint16_t x,uint16_t y,uint16_t width,uint16_t height,const uint16_t *pixels) { (void)context; if(pixels==NULL||!valid_rect(x,y,width,height))return false; lcd_window(x,y,width,height); gpio_put(LCD_PIN_DC,true); lcd_select(true); uint8_t bytes[256u]; const size_t count=(size_t)width*height; size_t offset=0u; while(offset<count){size_t chunk=count-offset;if(chunk>128u)chunk=128u;for(size_t i=0u;i<chunk;++i){const uint16_t p=pixels[offset+i];bytes[i*2u]=(uint8_t)(p>>8);bytes[i*2u+1u]=(uint8_t)p;}(void)spi_write_blocking(LCD_SPI,bytes,chunk*2u);offset+=chunk;} lcd_select(false); return true; }
void blu2usb_st7789_pico_set_backlight(bool enabled) { gpio_put(LCD_PIN_BL,enabled); }
static void configure_panel(void) { lcd_command(0x36u);lcd_data_u8(0x70u);lcd_command(0x3au);lcd_data_u8(0x05u);lcd_command(0xb2u);const uint8_t porch[]={0x0c,0x0c,0x00,0x33,0x33};lcd_data(porch,sizeof(porch));lcd_command(0xb7u);lcd_data_u8(0x35u);lcd_command(0xbbu);lcd_data_u8(0x19u);lcd_command(0xc0u);lcd_data_u8(0x2cu);lcd_command(0xc2u);lcd_data_u8(0x01u);lcd_command(0xc3u);lcd_data_u8(0x12u);lcd_command(0xc4u);lcd_data_u8(0x20u);lcd_command(0xc6u);lcd_data_u8(0x0fu);lcd_command(0xd0u);const uint8_t power[]={0xa4,0xa1};lcd_data(power,sizeof(power));lcd_command(0xe0u);const uint8_t gp[]={0xd0,0x04,0x0d,0x11,0x13,0x2b,0x3f,0x54,0x4c,0x18,0x0d,0x0b,0x1f,0x23};lcd_data(gp,sizeof(gp));lcd_command(0xe1u);const uint8_t gn[]={0xd0,0x04,0x0c,0x11,0x13,0x2c,0x3f,0x44,0x51,0x2f,0x1f,0x1f,0x20,0x23};lcd_data(gn,sizeof(gn));lcd_command(0x21u);lcd_command(0x11u);sleep_ms(120u);lcd_command(0x29u); }
bool blu2usb_st7789_pico_init(blu2usb_display_hal_t *display) { if(display==NULL)return false; (void)spi_init(LCD_SPI,LCD_SPI_BAUD_HZ); gpio_set_function(LCD_PIN_SCK,GPIO_FUNC_SPI); gpio_set_function(LCD_PIN_MOSI,GPIO_FUNC_SPI); const uint outputs[]={LCD_PIN_DC,LCD_PIN_CS,LCD_PIN_RST,LCD_PIN_BL}; for(size_t i=0u;i<sizeof(outputs)/sizeof(outputs[0]);++i){gpio_init(outputs[i]);gpio_set_dir(outputs[i],GPIO_OUT);} gpio_put(LCD_PIN_CS,true);gpio_put(LCD_PIN_DC,false);gpio_put(LCD_PIN_RST,true);blu2usb_st7789_pico_set_backlight(false);sleep_ms(100u);gpio_put(LCD_PIN_RST,false);sleep_ms(100u);gpio_put(LCD_PIN_RST,true);sleep_ms(100u);configure_panel();display->context=NULL;display->fill_rect=pico_fill_rect;display->write_rgb565=pico_write_rgb565;return true; }
