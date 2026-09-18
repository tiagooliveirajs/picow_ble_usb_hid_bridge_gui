#ifndef POC_LOG_H
#define POC_LOG_H

#include <stddef.h>
#include <stdint.h>

void poc_log_init(void);
void poc_logf(const char *format, ...);
void poc_log_hex(const char *label, const uint8_t *data, size_t len);

// Must be called from Core 0, the same core that owns tud_task().
void poc_log_usb_task(void);

#endif
