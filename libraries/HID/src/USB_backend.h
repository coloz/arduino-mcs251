/* SPDX-License-Identifier: MIT */
#ifndef STC_USB_BACKEND_H
#define STC_USB_BACKEND_H
#include <stc_usb.h>

/* Seven scalar state bytes on USB targets; descriptor/report buffers retain
 * their full capacities in XDATA. Override for applications short of DATA. */
#ifndef STC_HID_STATE_IN_DATA
# if defined(STC32G12K128) || defined(STC32G144K246) || defined(AI8051U_34K64) || \
     defined(STC32G12K64) || defined(STC32G8K64) || defined(STC32CL8K64)
#  define STC_HID_STATE_IN_DATA 1
# else
#  define STC_HID_STATE_IN_DATA 0
# endif
#endif
#if STC_HID_STATE_IN_DATA != 0 && STC_HID_STATE_IN_DATA != 1
# error "STC_HID_STATE_IN_DATA must be 0 or 1"
#endif
#if defined(__SDCC) && STC_HID_STATE_IN_DATA
# define STC_HID_HOT __data
#else
# define STC_HID_HOT
#endif
#define STC_USB_REPORT_IDS 16u
#ifdef __cplusplus
extern "C" {
#endif
uint8_t stc_usb_append(const uint8_t *data, uint16_t size) STC_REENTRANT;
uint8_t stc_usb_begin(void) STC_REENTRANT;
void stc_usb_end(void) STC_REENTRANT;
void stc_usb_poll(void) STC_REENTRANT;
uint8_t stc_usb_configured(void) STC_REENTRANT;
uint8_t stc_usb_error(void) STC_REENTRANT;
int stc_usb_send(uint8_t id, const uint8_t *data, uint8_t size) STC_REENTRANT;
int stc_usb_receive(uint8_t *data, uint8_t size) STC_REENTRANT;
uint8_t stc_usb_available(void) STC_REENTRANT;
uint8_t stc_usb_register_report(uint8_t id, uint8_t size) STC_REENTRANT;
uint8_t stc_usb_keyboard_leds(void) STC_REENTRANT;
#ifdef __cplusplus
}
#endif
#endif
