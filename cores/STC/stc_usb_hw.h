/* SPDX-License-Identifier: MIT */
#ifndef STC_USB_HARDWARE_H
#define STC_USB_HARDWARE_H
#ifdef STC_USB_TEST
#include "usb_test_hw.h"
#elif STC_CORE_USB_LAYOUT && defined(__SDCC)
#include <stc_sfr.h>
static __sfr __at (0xdc) usb_clock;
static __sfr __at (0xec) usb_data;
static __sfr __at (0xf4) usb_control;
static __sfr __at (0xfc) usb_address;
static __sfr __at (0xaf) usb_ie2;
static __sfr __at (0xc7) usb_iap_control;
static uint8_t usb_hw_fault;
static uint8_t usb_wait(void)
{
    uint16_t budget = 4096;
    while (usb_address & 0x80u) if (!--budget) { usb_hw_fault = 1; return 0; }
    return 1;
}
static uint8_t usb_hw_read(uint8_t reg)
{
    if (!usb_wait()) return 0;
    usb_address = reg | 0x80u;
    if (!usb_wait()) return 0;
    return usb_data;
}
static void usb_hw_write(uint8_t reg, uint8_t value)
{
    if (!usb_wait()) return;
    usb_address = reg & 0x7fu; usb_data = value;
}
static uint8_t usb_hw_start(void)
{
    uint16_t budget = 65535u;
    P_SW2 |= 0x80u;
    usb_ie2 &= (uint8_t)~0x80u;
    STC_XFR8(0x7efe07UL) |= 0x80u;
    while (!(STC_XFR8(0x7efe07UL) & 1u)) if (!--budget) return 0;
    usb_clock = 0; usb_control = 0x80u; usb_hw_fault = 0;
    return 1;
}
static void usb_hw_stop(void) { usb_control = 0; }
static void usb_hw_attach(void) { usb_control = 0x90u; }
static void usb_hw_reboot(void) { IE = 0; usb_iap_control = 0x60; }
#endif
#endif
