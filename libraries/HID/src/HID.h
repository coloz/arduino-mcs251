/* SPDX-License-Identifier: MIT */
#ifndef STC_HID_H
#define STC_HID_H
#include <Arduino.h>
#if STC_USB_CDC_ONLY
#error The 16 KB USB CDC profile cannot also include HID; disable CDC or select a larger chip
#endif
#include "USB_backend.h"
#include <USB.h>
#define _USING_HID 1
#define HID_REPORT_DESCRIPTOR_TYPE 0x22
#define HID_REPORT_TYPE_INPUT 1
#define HID_REPORT_TYPE_OUTPUT 2
#define HID_REPORT_TYPE_FEATURE 3

class HIDSubDescriptor {
public:
    HIDSubDescriptor(const void *descriptor, uint16_t size)
        : next(0), data(descriptor), length(size), registered(false) {}
    HIDSubDescriptor *next;
    const void *data;
    const uint16_t length;
    bool registered;
};
class HID_ {
public:
    int begin() { return stc_usb_begin(); }
    int SendReport(uint8_t id, const void *data, int length) {
        // Pass an invalid sentinel through the backend so lastError is updated.
        return stc_usb_send(id, static_cast<const uint8_t *>(data),
                            (uint8_t)((length < 0 || length > 63) ? 64 : length));
    }
    void AppendDescriptor(HIDSubDescriptor *node) {
        if (node && !node->registered)
            node->registered = stc_usb_append(static_cast<const uint8_t *>(node->data), node->length) != 0;
    }
    bool configured() { return stc_usb_configured() != 0; }
    uint8_t lastError() const { return stc_usb_error(); }
    int available() { return stc_usb_available(); }
    int read(uint8_t *data, uint8_t size) { return stc_usb_receive(data, size); }
    void poll() { stc_usb_poll(); }
    // Optional for custom reports: supplies the initial GET_REPORT length.
    bool RegisterReport(uint8_t id, uint8_t size) { return stc_usb_register_report(id, size) != 0; }
};
HID_ &HID();
#endif
