/* SPDX-License-Identifier: MIT */
#include "stc_usb_service.h"
void (*stc_usb_service)(void) STC_REENTRANT;
uint8_t stc_usb_active;
uint8_t stc_usb_uart1_active;
