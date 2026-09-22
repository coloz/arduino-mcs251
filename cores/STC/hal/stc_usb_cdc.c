/* SPDX-License-Identifier: MIT
 * One USB CDC ACM function. No UART pins, clocks, or interrupts are used.
 * Fixed rings provide bounded RAM use; a full RX ring leaves OUT unacknowledged
 * (USB NAK/backpressure) rather than dropping bytes. TX is bounded as well.
 */
#include "stc_usb_cdc.h"
#if STC_CORE_USB_LAYOUT
#define CDC_RX_SIZE 128u
#define CDC_TX_SIZE 128u
static const uint8_t cdc_descriptor[] = {
    8,11,0,2,2,2,0,0,             /* IAD, interfaces 0 and 1 */
    9,4,0,0,1,2,2,0,0,            /* ACM control */
    5,0x24,0,0x10,1,              /* CDC 1.10 */
    5,0x24,1,0,1,                 /* no call management, data interface 1 */
    4,0x24,2,6,                   /* line coding/state, SERIAL_STATE, BREAK */
    5,0x24,6,0,1,                 /* union master 0 / slave 1 */
    7,5,0x83,3,16,0,16,           /* notifications */
    9,4,1,0,2,10,0,0,0,           /* data */
    7,5,0x02,2,64,0,0,
    7,5,0x82,2,64,0,0
};
static uint8_t line_coding[7] = {0,0xc2,1,0,0,0,8};
static uint8_t line_state, active, rx[CDC_RX_SIZE], tx[CDC_TX_SIZE];
static uint8_t rx_head, rx_tail, rx_count, tx_head, tx_tail, tx_count;
static uint8_t packet[64], notification_pending, tx_zlp;
static uint8_t reboot_enabled = 1, reboot_pending;
static unsigned long reboot_at;
static int32_t break_value = -1;

static void cdc_reset(void) STC_REENTRANT
{
    line_state = rx_head = rx_tail = rx_count = tx_head = tx_tail = tx_count = 0;
    tx_zlp = reboot_pending = 0; notification_pending = 1; break_value = -1;
    stc_usb_ep_init(2, 1, 64); stc_usb_ep_init(3, 0, 16);
}
static uint8_t set_line_coding(const uint8_t *data) STC_REENTRANT
{
    uint8_t i;
    if (data[4] > 2 || data[5] > 4 || (data[6] < 5 || (data[6] > 8 && data[6] != 16))) return 0;
    for (i = 0; i < 7; ++i) line_coding[i] = data[i];
    if (stc_cdc_baud() != 1200UL) reboot_pending = 0;
    return 1;
}
static uint8_t cdc_setup(const uint8_t *s) STC_REENTRANT
{
    uint16_t length = (uint16_t)s[6] | ((uint16_t)s[7] << 8);
    uint16_t value = (uint16_t)s[2] | ((uint16_t)s[3] << 8);
    if (s[0] == 0xa1 && s[1] == 0x21 && !value && length == 7) {
        stc_usb_control_send(line_coding, 7); return 1;
    }
    if (s[0] != 0x21) return 0;
    if (s[1] == 0x20 && !value && length == 7) {
        stc_usb_control_receive(7, set_line_coding); return 1;
    }
    if (s[1] == 0x22 && !length && value <= 3) {
        /* Require a real DTR falling edge. Delay the reset so EP0 status can
         * complete, and cancel a transient close if DTR is asserted again. */
        if (reboot_enabled && active && (line_state & 1u) && !(value & 1u) && stc_cdc_baud() == 1200UL) {
            reboot_pending = 1; reboot_at = millis();
        }
        if (value & 1u) reboot_pending = 0;
        line_state = (uint8_t)value; notification_pending = 1;
        stc_usb_control_status(); return 1;
    }
    if (s[1] == 0x23 && !length) {
        break_value = value; stc_usb_control_status(); return 1;
    }
    return 0;
}
static void cdc_poll(void) STC_REENTRANT
{
    int count;
    uint8_t n, i;
    if (reboot_pending && millis() - reboot_at >= 120UL) { stc_usb_reboot_to_isp(); return; }
    if (notification_pending && stc_usb_ep_ready(3)) {
        const uint8_t notification[] = {0xa1,0x20,0,0,0,0,2,0,3,0};
        /* Carrier present while this virtual port is enabled. */
        memcpy(packet, notification, 10); if (!active) packet[8] = 0;
        stc_usb_ep_send(3, packet, 10); notification_pending = 0;
    }
    if (!active) return;
    count = stc_usb_ep_available(2);
    if (count > 64) { stc_usb_ep_receive(2, 0, (uint8_t)count); stc_usb_set_error(STC_USB_INVALID); }
    else if (count >= 0 && (uint8_t)count <= CDC_RX_SIZE - rx_count) {
        n = (uint8_t)count; stc_usb_ep_receive(2, packet, n);
        for (i = 0; i < n; ++i) { rx[rx_head] = packet[i]; rx_head = (rx_head + 1u) % CDC_RX_SIZE; }
        rx_count += n;
    }
    if ((line_state & 1u) && stc_usb_ep_ready(2)) {
        n = tx_count > 64u ? 64u : tx_count;
        if (n || tx_zlp) {
            for (i = 0; i < n; ++i) { packet[i] = tx[tx_tail]; tx_tail = (tx_tail + 1u) % CDC_TX_SIZE; }
            tx_count -= n; stc_usb_ep_send(2, packet, n);
            /* Terminate an exact multiple of wMaxPacketSize with a ZLP. */
            tx_zlp = n == 64u;
        }
    }
}
static const stc_usb_class cdc_class = {cdc_descriptor, sizeof(cdc_descriptor), cdc_reset, cdc_poll, cdc_setup};

uint8_t stc_cdc_register(void) STC_REENTRANT { return stc_usb_register_class(&cdc_class, 1); }
uint8_t stc_cdc_begin(void) STC_REENTRANT
{
    if (!stc_cdc_register()) return 0;
    active = 1; notification_pending = 1; return stc_usb_begin();
}
void stc_cdc_end(void) STC_REENTRANT
{
    active = rx_count = tx_count = tx_zlp = reboot_pending = 0;
    rx_head = rx_tail = tx_head = tx_tail = 0; notification_pending = 1;
    /* Leave descriptors and other classes attached, like Serial.end() on
     * Arduino native USB. begin() can reopen the same configured function. */
}
int stc_cdc_available(void) STC_REENTRANT { stc_usb_poll(); return active ? rx_count : 0; }
int stc_cdc_peek(void) STC_REENTRANT { return stc_cdc_available() ? rx[rx_tail] : -1; }
int stc_cdc_read(void) STC_REENTRANT
{
    int result = stc_cdc_peek();
    if (result >= 0) { rx_tail = (rx_tail + 1u) % CDC_RX_SIZE; --rx_count; }
    return result;
}
uint8_t stc_cdc_connected(void) STC_REENTRANT { return stc_usb_configured() && active && (line_state & 1u); }
uint8_t stc_cdc_line_state(void) STC_REENTRANT { stc_usb_poll(); return line_state; }
uint32_t stc_cdc_baud(void) STC_REENTRANT
{
    return (uint32_t)line_coding[0] | ((uint32_t)line_coding[1] << 8) |
           ((uint32_t)line_coding[2] << 16) | ((uint32_t)line_coding[3] << 24);
}
uint8_t stc_cdc_line_format(uint8_t index) STC_REENTRANT { return index < 7 ? line_coding[index] : 0; }
int32_t stc_cdc_read_break(void) STC_REENTRANT { int32_t result = break_value; break_value = -1; return result; }
int stc_cdc_write_space(void) STC_REENTRANT { return stc_cdc_connected() ? CDC_TX_SIZE - tx_count : 0; }
size_t stc_cdc_write(const uint8_t *data, size_t size, unsigned long timeout) STC_REENTRANT
{
    size_t written = 0;
    unsigned long start = millis();
    uint16_t budget = 60000u;
    if (!data && size) { stc_usb_set_error(STC_USB_INVALID); return 0; }
    while (written < size) {
        if (!stc_cdc_connected()) { stc_usb_set_error(STC_USB_NOT_CONFIGURED); break; }
        if (tx_count < CDC_TX_SIZE) {
            tx[tx_head] = data[written++]; tx_head = (tx_head + 1u) % CDC_TX_SIZE; ++tx_count;
        } else if (!--budget || millis() - start >= timeout) { stc_usb_set_error(STC_USB_TIMEOUT); break; }
    }
    return written;
}
uint8_t stc_cdc_flush(unsigned long timeout) STC_REENTRANT
{
    unsigned long start = millis();
    uint16_t budget = 60000u;
    do {
        if (!stc_cdc_connected()) { stc_usb_set_error(STC_USB_NOT_CONFIGURED); return 0; }
        if (!tx_count && !tx_zlp && stc_usb_ep_ready(2)) return 1;
    } while (--budget && millis() - start < timeout);
    stc_usb_set_error(STC_USB_TIMEOUT); return 0;
}
void stc_cdc_enable_reboot(uint8_t enabled) STC_REENTRANT { reboot_enabled = enabled != 0; if (!enabled) reboot_pending = 0; }
uint8_t stc_cdc_reboot_enabled(void) STC_REENTRANT { return reboot_enabled; }
void stc_cdc_boot(void) STC_REENTRANT
{
    if (stc_cdc_register()) { active = 1; stc_usb_schedule_start(); }
}
#endif
