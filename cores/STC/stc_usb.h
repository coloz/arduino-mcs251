/* SPDX-License-Identifier: MIT */
#ifndef STC_USB_DEVICE_BACKEND_H
#define STC_USB_DEVICE_BACKEND_H
#include <Arduino.h>
#define STC_USB_OK 0
#define STC_USB_UNSUPPORTED 1
#define STC_USB_INVALID 2
#define STC_USB_NOT_CONFIGURED 3
#define STC_USB_TIMEOUT 4
#define STC_USB_DESCRIPTOR_FULL 5
#define STC_USB_BUSY 6
#define STC_USB_PACKET_SIZE 64u

#ifdef __cplusplus
extern "C" {
#endif
/* A single optional HID class and a single optional CDC ACM function.
 * CDC owns interfaces 0/1 and endpoints 2/3; HID owns endpoint 1 and
 * interface 0 (alone) or 2 (composite). Register before USB attachment. */
typedef struct {
    const uint8_t *descriptor;
    uint8_t length;
    void (*reset)(void) STC_REENTRANT;
    void (*poll)(void) STC_REENTRANT;
    uint8_t (*setup)(const uint8_t *request) STC_REENTRANT;
} stc_usb_class;
typedef uint8_t (*stc_usb_control_callback)(const uint8_t *data) STC_REENTRANT;

uint8_t stc_usb_register_class(const stc_usb_class *driver, uint8_t cdc) STC_REENTRANT;
uint8_t stc_usb_begin(void) STC_REENTRANT;
void stc_usb_end(void) STC_REENTRANT;
void stc_usb_schedule_start(void) STC_REENTRANT;
void stc_usb_poll(void) STC_REENTRANT;
uint8_t stc_usb_configured(void) STC_REENTRANT;
uint8_t stc_usb_error(void) STC_REENTRANT;
void stc_usb_set_error(uint8_t value) STC_REENTRANT;
uint8_t stc_usb_started(void) STC_REENTRANT;
void stc_usb_control_send(const uint8_t *data, uint16_t length) STC_REENTRANT;
void stc_usb_control_status(void) STC_REENTRANT;
void stc_usb_control_receive(uint8_t length, stc_usb_control_callback callback) STC_REENTRANT;
void stc_usb_ep_init(uint8_t ep, uint8_t receive, uint8_t packet_size) STC_REENTRANT;
uint8_t stc_usb_ep_ready(uint8_t ep) STC_REENTRANT;
void stc_usb_ep_send(uint8_t ep, const uint8_t *data, uint8_t length) STC_REENTRANT;
int stc_usb_ep_available(uint8_t ep) STC_REENTRANT;
void stc_usb_ep_receive(uint8_t ep, uint8_t *data, uint8_t length) STC_REENTRANT;
void stc_usb_reboot_to_isp(void) STC_REENTRANT;
#ifdef __cplusplus
}
#endif
#endif
