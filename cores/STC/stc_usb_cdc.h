/* SPDX-License-Identifier: MIT */
#ifndef STC_USB_CDC_BACKEND_H
#define STC_USB_CDC_BACKEND_H
#include "stc_usb.h"
#ifdef __cplusplus
extern "C" {
#endif
uint8_t stc_cdc_register(void) STC_REENTRANT;
uint8_t stc_cdc_begin(void) STC_REENTRANT;
void stc_cdc_end(void) STC_REENTRANT;
int stc_cdc_available(void) STC_REENTRANT;
int stc_cdc_peek(void) STC_REENTRANT;
int stc_cdc_read(void) STC_REENTRANT;
uint8_t stc_cdc_connected(void) STC_REENTRANT;
uint8_t stc_cdc_line_state(void) STC_REENTRANT;
uint32_t stc_cdc_baud(void) STC_REENTRANT;
uint8_t stc_cdc_line_format(uint8_t index) STC_REENTRANT;
int32_t stc_cdc_read_break(void) STC_REENTRANT;
int stc_cdc_write_space(void) STC_REENTRANT;
size_t stc_cdc_write(const uint8_t *data, size_t size, unsigned long timeout) STC_REENTRANT;
uint8_t stc_cdc_flush(unsigned long timeout) STC_REENTRANT;
void stc_cdc_enable_reboot(uint8_t enabled) STC_REENTRANT;
uint8_t stc_cdc_reboot_enabled(void) STC_REENTRANT;
#ifdef __cplusplus
}
#endif
#endif
