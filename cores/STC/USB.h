/* SPDX-License-Identifier: MIT */
#ifndef STC_CORE_USB_H
#define STC_CORE_USB_H
#include <Arduino.h>
#include "stc_usb.h"
#ifdef __cplusplus
#include "USBCDC.h"
class USBDeviceClass {
public:
    bool begin() { return stc_usb_begin() != 0; }
    bool attach() { return begin(); }
    bool detach() { stc_usb_end(); return true; }
    bool configured() { return stc_usb_configured() != 0; }
    void poll() { stc_usb_poll(); }
};
extern USBDeviceClass USBDevice;
#define USB USBDevice
#endif
#endif
