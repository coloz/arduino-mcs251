/* SPDX-License-Identifier: MIT */
#ifndef STC_USB_SERVICE_H
#define STC_USB_SERVICE_H
#include "Arduino.h"
/* Installed by USB.begin(). Serviced from loop()/delay()/Stream waits. */
extern void (*stc_usb_service)(void) STC_REENTRANT;
/* UART1 and native USB share P3.0/P3.1. Refuse concurrent ownership. */
extern uint8_t stc_usb_active;
extern uint8_t stc_usb_uart1_active;
#endif
