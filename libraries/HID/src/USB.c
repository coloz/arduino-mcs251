/* SPDX-License-Identifier: MIT
 * Optional HID class for the shared core USB controller (endpoint 1).
 */
#include "USB_backend.h"
#define DESCRIPTOR_CAPACITY 512u
static uint8_t report_descriptor[DESCRIPTOR_CAPACITY];
static STC_HID_HOT uint16_t descriptor_length;
static uint8_t registered_size[STC_USB_REPORT_IDS];
static STC_HID_HOT uint8_t keyboard_leds;
#if STC_CORE_USB_LAYOUT
static uint8_t hid_descriptor[] = {
    9,4,0,0,2,3,0,0,0,
    9,0x21,0x11,0x01,0,1,0x22,0,0,
    7,5,0x81,3,64,0,1,
    7,5,0x01,3,64,0,1
};
static uint8_t output[64];
static STC_HID_HOT uint8_t output_length, output_id, output_expected;
static uint8_t last_report[STC_USB_REPORT_IDS][64], report_length[STC_USB_REPORT_IDS];
static uint8_t idle[STC_USB_REPORT_IDS];
static STC_HID_HOT uint8_t idle_cursor;
/* HID idle is at most 1020 ms; 16-bit modular timestamps are sufficient. */
static uint16_t last_sent[STC_USB_REPORT_IDS];
static void hid_reset(void) STC_REENTRANT
{
    uint8_t i;
    stc_usb_ep_init(1, 1, 64); output_length = keyboard_leds = 0;
    memset(last_report, 0, sizeof(last_report));
    for (i = 0; i < STC_USB_REPORT_IDS; ++i) {
        idle[i] = 0; report_length[i] = registered_size[i]; last_report[i][0] = i;
    }
}
static uint8_t hid_output(const uint8_t *data) STC_REENTRANT
{
    if (data[0] != output_id) return 0;
    if (output_id == 2) keyboard_leds = data[1] & 31u;
    else { memcpy(output, data, output_expected); output_length = output_expected; }
    return 1;
}
static uint8_t hid_setup(const uint8_t *s) STC_REENTRANT
{
    uint8_t type = s[0], req = s[1], id = s[2], kind = s[3], i;
    uint16_t length = (uint16_t)s[6] | ((uint16_t)s[7] << 8);
    if (type == 0x81 && req == 6 && !id) {
        if (kind == 0x21) { stc_usb_control_send(hid_descriptor + 9, 9); return 1; }
        if (kind == 0x22) { stc_usb_control_send(report_descriptor, descriptor_length); return 1; }
    }
    if (id >= STC_USB_REPORT_IDS) return 0;
    if (type == 0xa1 && req == 1 && kind == 1 && id && report_length[id]) {
        stc_usb_control_send(last_report[id], report_length[id]); return 1;
    }
    if (type == 0xa1 && req == 2 && !kind && length == 1) {
        stc_usb_control_send(idle + id, 1); return 1;
    }
    if (type == 0x21 && req == 10 && !length) {
        if (!id) for (i = 1; i < STC_USB_REPORT_IDS; ++i) idle[i] = kind;
        else idle[id] = kind;
        stc_usb_control_status(); return 1;
    }
    if (type == 0x21 && req == 9 && kind == 2 && length && length <= 64 &&
        (id == 2 ? length == 2 : !output_length)) {
        output_id = id; output_expected = (uint8_t)length;
        stc_usb_control_receive(output_expected, hid_output); return 1;
    }
    return 0;
}
static void hid_poll(void) STC_REENTRANT
{
    int count;
    uint16_t now;
    if (!output_length && (count = stc_usb_ep_available(1)) >= 0) {
        if (count <= 64) {
            stc_usb_ep_receive(1, output, (uint8_t)count);
            if (count == 2 && output[0] == 2) keyboard_leds = output[1] & 31u;
            else output_length = (uint8_t)count;
        } else { stc_usb_ep_receive(1, 0, (uint8_t)count); stc_usb_set_error(STC_USB_INVALID); }
    }
    if (++idle_cursor >= STC_USB_REPORT_IDS) idle_cursor = 1;
    now = (uint16_t)millis();
    if (idle[idle_cursor] && report_length[idle_cursor] &&
        (uint16_t)(now - last_sent[idle_cursor]) >= (uint16_t)idle[idle_cursor] * 4u && stc_usb_ep_ready(1)) {
        stc_usb_ep_send(1, last_report[idle_cursor], report_length[idle_cursor]); last_sent[idle_cursor] = now;
    }
}
static const stc_usb_class hid_class = {hid_descriptor, sizeof(hid_descriptor), hid_reset, hid_poll, hid_setup};
#endif
uint8_t stc_usb_append(const uint8_t *data, uint16_t size) STC_REENTRANT
{
    uint16_t i;
    if (stc_usb_started()) { stc_usb_set_error(STC_USB_BUSY); return 0; }
    if (!data || !size) { stc_usb_set_error(STC_USB_INVALID); return 0; }
    if (size > DESCRIPTOR_CAPACITY - descriptor_length) { stc_usb_set_error(STC_USB_DESCRIPTOR_FULL); return 0; }
#if STC_CORE_USB_LAYOUT
    if (!stc_usb_register_class(&hid_class, 0)) return 0;
#endif
    for (i = 0; i < size; ++i) report_descriptor[descriptor_length++] = data[i];
#if STC_CORE_USB_LAYOUT
    hid_descriptor[16] = (uint8_t)descriptor_length; hid_descriptor[17] = (uint8_t)(descriptor_length >> 8);
#endif
    stc_usb_set_error(STC_USB_OK); return 1;
}
uint8_t stc_usb_register_report(uint8_t id, uint8_t size) STC_REENTRANT
{
    if (stc_usb_started() || !id || id >= STC_USB_REPORT_IDS || size > 63u) { stc_usb_set_error(STC_USB_INVALID); return 0; }
    registered_size[id] = size + 1u; return 1;
}
uint8_t stc_usb_keyboard_leds(void) STC_REENTRANT { stc_usb_poll(); return keyboard_leds; }
int stc_usb_send(uint8_t id, const uint8_t *data, uint8_t size) STC_REENTRANT
{
#if STC_CORE_USB_LAYOUT
    uint8_t i;
    uint16_t budget = 60000u;
    unsigned long start = millis();
    if (!id || id >= STC_USB_REPORT_IDS || size > 63u || (!data && size) ||
        (registered_size[id] && size + 1u != registered_size[id])) { stc_usb_set_error(STC_USB_INVALID); return -STC_USB_INVALID; }
    if (!stc_usb_configured()) { stc_usb_set_error(STC_USB_NOT_CONFIGURED); return -STC_USB_NOT_CONFIGURED; }
    while (!stc_usb_ep_ready(1)) {
        if (!stc_usb_configured()) { stc_usb_set_error(STC_USB_NOT_CONFIGURED); return -STC_USB_NOT_CONFIGURED; }
        if (!--budget || millis() - start >= 100UL) { stc_usb_set_error(STC_USB_TIMEOUT); return -STC_USB_TIMEOUT; }
    }
    last_report[id][0] = id;
    for (i = 0; i < size; ++i) last_report[id][i + 1u] = data[i];
    report_length[id] = size + 1u;
    stc_usb_ep_send(1, last_report[id], size + 1u); last_sent[id] = (uint16_t)millis();
    if (id == 1 && size == 4) last_report[id][2] = last_report[id][3] = last_report[id][4] = 0;
    stc_usb_set_error(STC_USB_OK); return size + 1;
#else
    (void)id; (void)data; (void)size; stc_usb_set_error(STC_USB_UNSUPPORTED); return -STC_USB_UNSUPPORTED;
#endif
}
uint8_t stc_usb_available(void) STC_REENTRANT
{
    stc_usb_poll();
#if STC_CORE_USB_LAYOUT
    return output_length;
#else
    return 0;
#endif
}
int stc_usb_receive(uint8_t *data, uint8_t size) STC_REENTRANT
{
#if STC_CORE_USB_LAYOUT
    uint8_t i, n;
    stc_usb_poll(); n = output_length;
    if (!n) return 0;
    if (!data || size < n) { stc_usb_set_error(STC_USB_INVALID); return -STC_USB_INVALID; }
    for (i = 0; i < n; ++i) data[i] = output[i];
    output_length = 0; return n;
#else
    (void)data; (void)size; return 0;
#endif
}
