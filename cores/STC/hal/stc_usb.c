/* SPDX-License-Identifier: MIT
 * Shared, cooperatively polled STC USB device controller. Class drivers are
 * separately linked, so CDC-only sketches do not pay for the HID report store.
 * Wire descriptors are byte arrays, independent of the MCS251 structure ABI.
 */
#include "stc_usb.h"
#include "stc_usb_service.h"
#include "stc_usb_hw.h"
#ifndef USB_VID
#define USB_VID 0x1209u
#endif
#ifdef USB_PID
#define STC_USB_INITIAL_PID USB_PID
#else
#define STC_USB_INITIAL_PID 0x0001u
#endif
static uint8_t error, start_pending;
#define started stc_usb_active
#if STC_CORE_USB_LAYOUT
#if STC_USB_CDC_ONLY
#define hid_class ((const stc_usb_class *)0)
#else
static const stc_usb_class *hid_class;
#endif
#if STC_USB_HID_ONLY
#define cdc_class ((const stc_usb_class *)0)
#else
static const stc_usb_class *cdc_class;
#endif
static uint8_t device_descriptor[] = {
    18,1,0,2,0,0,0,64,
    USB_VID & 255u,USB_VID >> 8,STC_USB_INITIAL_PID & 255u,STC_USB_INITIAL_PID >> 8,
    0,2,1,2,0,1
};
static uint8_t config_descriptor[107], config_length;
static const uint8_t language[] = {4,3,9,4};
static const uint8_t manufacturer[] = {22,3,'S',0,'T',0,'C',0,' ',0,'M',0,'C',0,'S',0,'2',0,'5',0,'1',0};
#if !STC_USB_CDC_ONLY
static const uint8_t product_hid[] = {16,3,'S',0,'T',0,'C',0,' ',0,'H',0,'I',0,'D',0};
#endif
#if !STC_USB_HID_ONLY
static const uint8_t product_cdc[] = {24,3,'S',0,'T',0,'C',0,' ',0,'U',0,'S',0,'B',0,' ',0,'C',0,'D',0,'C',0};
#endif
static uint8_t configuration, suspended, polling, halt_in, halt_out;
static uint8_t setup_data[8], reply[64], control_state, control_zlp, control_expected;
static const uint8_t *control_data;
static uint16_t control_left, host_length;
static stc_usb_control_callback control_callback;

static void reset_classes(void)
{
    halt_in = halt_out = 0;
    if (hid_class) hid_class->reset();
    if (cdc_class) cdc_class->reset();
    usb_hw_write(14, 0);
}
static void bus_reset(void)
{
    usb_hw_write(0, 0);
    configuration = suspended = control_state = 0;
    control_callback = 0;
    reset_classes();
}
static void stall(void) { control_state = 0; control_callback = 0; usb_hw_write(17, 0x60); }
static void control_in(void)
{
    uint8_t n, i;
    if (usb_hw_read(17) & 2u) return;
    n = control_left > 64u ? 64u : (uint8_t)control_left;
    for (i = 0; i < n; ++i) usb_hw_write(32, *control_data++);
    control_left -= n;
    if (!control_left && (!control_zlp || !n)) {
        usb_hw_write(17, 0x0a); control_state = 0;
    } else usb_hw_write(17, 2);
}
static uint8_t valid_interface(uint8_t index)
{
    return index < (uint16_t)((cdc_class ? 2u : 0u) + (hid_class ? 1u : 0u));
}
static uint8_t valid_endpoint(uint8_t index)
{
    return (hid_class && (index == 1 || index == 0x81)) ||
           (cdc_class && (index == 2 || index == 0x82 || index == 0x83));
}
static void request(void)
{
    uint8_t type = setup_data[0], req = setup_data[1], id = setup_data[2], kind = setup_data[3];
    uint8_t value = id, index = setup_data[4];
    uint8_t mask, ep;
    host_length = (uint16_t)setup_data[6] | ((uint16_t)setup_data[7] << 8);
    /* Only string descriptor language IDs have a nonzero wIndex high byte. */
    if (setup_data[5] && !(type == 0x80u && req == 6 && kind == 3 && setup_data[5] == 4 && index == 9)) {
        stall(); return;
    }
    if (type == 0x80u && req == 6u) {
        if (!index && kind == 1 && !id) { stc_usb_control_send(device_descriptor, 18); return; }
        if (!index && kind == 2 && !id) { stc_usb_control_send(config_descriptor, config_length); return; }
        if (kind == 3 && ((!index && !setup_data[5]) || (index == 9 && setup_data[5] == 4))) {
            if (!id) { stc_usb_control_send(language, sizeof(language)); return; }
            if (id == 1) { stc_usb_control_send(manufacturer, sizeof(manufacturer)); return; }
            if (id == 2) {
#if STC_USB_CDC_ONLY
                stc_usb_control_send(product_cdc, sizeof(product_cdc));
#elif STC_USB_HID_ONLY
                stc_usb_control_send(product_hid, sizeof(product_hid));
#else
                if (cdc_class) stc_usb_control_send(product_cdc, sizeof(product_cdc));
                else stc_usb_control_send(product_hid, sizeof(product_hid));
#endif
                return;
            }
        }
    }
    if (!type && req == 5 && !kind && value < 128 && !index && !host_length && !configuration) {
        usb_hw_write(0, (uint8_t)value); stc_usb_control_status(); return;
    }
    if (!type && req == 9 && !kind && value <= 1 && !index && !host_length) {
        reset_classes(); configuration = (uint8_t)value;
        stc_usb_control_status(); return;
    }
    if (type == 0x80u && req == 8 && !kind && !value && !index && host_length == 1) {
        reply[0] = configuration; stc_usb_control_send(reply, 1); return;
    }
    if (!req && !kind && !value && host_length == 2) {
        reply[0] = reply[1] = 0;
        if ((type == 0x80u && !index) ||
            (type == 0x81u && configuration && valid_interface(index)) ||
            (type == 0x82u && (index == 0 || index == 0x80))) {
            stc_usb_control_send(reply, 2); return;
        }
        if (type == 0x82u && configuration && valid_endpoint(index)) {
            mask = (uint8_t)(1u << (index & 15u));
            reply[0] = ((index & 0x80u ? halt_in : halt_out) & mask) != 0;
            stc_usb_control_send(reply, 2); return;
        }
    }
    if (configuration && type == 2 && (req == 1 || req == 3) && !kind && !value && !host_length && valid_endpoint(index)) {
        ep = (uint8_t)index & 15u; mask = (uint8_t)(1u << ep);
        usb_hw_write(14, ep);
        if (index & 0x80u) {
            if (req == 3) halt_in |= mask; else halt_in &= (uint8_t)~mask;
            usb_hw_write(17, req == 3 ? 0x10 : 0x48);
        } else {
            if (req == 3) halt_out |= mask; else halt_out &= (uint8_t)~mask;
            usb_hw_write(20, req == 3 ? 0x20 : 0x90);
        }
        usb_hw_write(14, 0); stc_usb_control_status(); return;
    }
    if (configuration && valid_interface(index) && !kind && !value) {
        if (type == 0x81u && req == 10 && host_length == 1) {
            reply[0] = 0; stc_usb_control_send(reply, 1); return;
        }
        if (type == 1 && req == 11 && !host_length) { stc_usb_control_status(); return; }
    }
    /* Class requests require configuration; HID descriptors are also queried
     * before SET_CONFIGURATION. Route by the complete 16-bit interface ID. */
    if ((type & 0x1fu) == 1u) {
        if (hid_class && index == (cdc_class ? 2u : 0u) &&
            ((type == 0x81 && req == 6) || configuration) && hid_class->setup(setup_data)) return;
        if (cdc_class && index == 0 && configuration && cdc_class->setup(setup_data)) return;
    }
    stall();
}
static void endpoint_zero(void)
{
    uint8_t csr, n, i;
    usb_hw_write(14, 0); csr = usb_hw_read(17);
    if (csr & 4u) { usb_hw_write(17, 0); control_state = 0; control_callback = 0; }
    if (csr & 16u) { usb_hw_write(17, 0x80); control_state = 0; control_callback = 0; }
    if (control_state == 1) { control_in(); return; }
    if (!(csr & 1u)) return;
    n = usb_hw_read(22);
    if (control_state == 2) {
        if (n != control_expected) { stall(); return; }
        for (i = 0; i < n; ++i) reply[i] = usb_hw_read(32);
        if (!control_callback || !control_callback(reply)) { stall(); return; }
        stc_usb_control_status(); return;
    }
    if (!n) { usb_hw_write(17, 0x40); return; }
    if (n != 8) { stall(); return; }
    for (i = 0; i < 8; ++i) setup_data[i] = usb_hw_read(32);
    request();
}
#endif

uint8_t stc_usb_register_class(const stc_usb_class *driver, uint8_t cdc) STC_REENTRANT
{
#if STC_CORE_USB_LAYOUT
#if STC_USB_CDC_ONLY
    const stc_usb_class *previous = cdc_class;
#elif STC_USB_HID_ONLY
    const stc_usb_class *previous = hid_class;
#else
    const stc_usb_class *previous = cdc ? cdc_class : hid_class;
#endif
#if STC_USB_CDC_ONLY
    if (!cdc) { error = STC_USB_UNSUPPORTED; return 0; }
#endif
#if STC_USB_HID_ONLY
    if (cdc) { error = STC_USB_UNSUPPORTED; return 0; }
#endif
    if (previous == driver) return 1;
    if (started) { error = STC_USB_BUSY; return 0; }
    if (!driver || !driver->descriptor || driver->length !=
#if STC_USB_CDC_ONLY
        66u
#elif STC_USB_HID_ONLY
        32u
#else
        (cdc ? 66u : 32u)
#endif
    ) { error = STC_USB_INVALID; return 0; }
#if STC_USB_CDC_ONLY
    cdc_class = driver;
#elif STC_USB_HID_ONLY
    hid_class = driver;
#else
    if (cdc) cdc_class = driver;
    if (!cdc) hid_class = driver;
#endif
    return 1;
#else
    (void)driver; (void)cdc; error = STC_USB_UNSUPPORTED; return 0;
#endif
}
uint8_t stc_usb_begin(void) STC_REENTRANT
{
#if STC_CORE_USB_LAYOUT
    if (started) return 1;
    if (stc_usb_uart1_active) { error = STC_USB_BUSY; return 0; }
    if (!hid_class && !cdc_class) { error = STC_USB_INVALID; return 0; }
    config_descriptor[0] = 9; config_descriptor[1] = 2;
    config_descriptor[3] = 0; config_descriptor[4] = (cdc_class ? 2 : 0) + (hid_class ? 1 : 0);
    config_descriptor[5] = 1; config_descriptor[6] = 0; config_descriptor[7] = 0x80; config_descriptor[8] = 50;
    config_length = 9;
    if (cdc_class) {
        memcpy(config_descriptor + config_length, cdc_class->descriptor, cdc_class->length);
        config_length += cdc_class->length;
    }
    if (hid_class) {
        memcpy(config_descriptor + config_length, hid_class->descriptor, hid_class->length);
        config_length += hid_class->length;
        config_descriptor[config_length - 30u] = cdc_class ? 2 : 0;
    }
    config_descriptor[2] = config_length;
    device_descriptor[4] = cdc_class ? 0xef : 0;
    device_descriptor[5] = cdc_class ? 2 : 0;
    device_descriptor[6] = cdc_class ? 1 : 0;
#ifndef USB_PID
    /* Different layouts must not reuse Windows' cached descriptors. These
     * pid.codes IDs are reserved for private experiments, not distribution. */
    device_descriptor[10] = cdc_class ? (hid_class ? 3 : 2) : 1;
#endif
    if (!usb_hw_start()) { error = STC_USB_TIMEOUT; usb_hw_stop(); return 0; }
    bus_reset();
    usb_hw_write(7, 1u | (hid_class ? 2u : 0u) | (cdc_class ? 12u : 0u));
    usb_hw_write(9, (hid_class ? 2u : 0u) | (cdc_class ? 4u : 0u));
    usb_hw_write(11, 7); usb_hw_write(1, 1);
    (void)usb_hw_read(6); (void)usb_hw_read(2); (void)usb_hw_read(4);
    started = 1; start_pending = 0; stc_usb_service = stc_usb_poll;
    usb_hw_attach(); error = STC_USB_OK; return 1;
#else
    error = STC_USB_UNSUPPORTED; return 0;
#endif
}
void stc_usb_schedule_start(void) STC_REENTRANT { start_pending = 1; stc_usb_service = stc_usb_poll; }
void stc_usb_end(void) STC_REENTRANT
{
    stc_usb_service = 0; start_pending = 0;
#if STC_CORE_USB_LAYOUT
    if (started) { usb_hw_stop(); reset_classes(); }
    configuration = control_state = 0; control_callback = 0;
#endif
    started = 0;
}
void stc_usb_poll(void) STC_REENTRANT
{
#if STC_CORE_USB_LAYOUT
    uint8_t events;
    if (polling) return;
    if (!started && start_pending) { start_pending = 0; stc_usb_begin(); }
    if (!started) return;
    polling = 1;
    events = usb_hw_read(6); (void)usb_hw_read(2); (void)usb_hw_read(4);
    if (events & 4u) bus_reset();
    if (events & 1u) suspended = 1;
    if (events & 2u) suspended = 0;
    endpoint_zero();
    if (configuration && !suspended) {
        if (hid_class) hid_class->poll();
        if (cdc_class) cdc_class->poll();
    }
    if (usb_hw_fault) { error = STC_USB_TIMEOUT; stc_usb_end(); }
    polling = 0;
#endif
}
uint8_t stc_usb_configured(void) STC_REENTRANT
{
    stc_usb_poll();
#if STC_CORE_USB_LAYOUT
    return started && configuration && !suspended;
#else
    return 0;
#endif
}
uint8_t stc_usb_started(void) STC_REENTRANT { return started; }
uint8_t stc_usb_error(void) STC_REENTRANT { return error; }
void stc_usb_set_error(uint8_t value) STC_REENTRANT { error = value; }
void stc_usb_control_status(void) STC_REENTRANT
{
#if STC_CORE_USB_LAYOUT
    control_state = 0; control_callback = 0; usb_hw_write(14, 0); usb_hw_write(17, 0x48);
#endif
}
void stc_usb_control_send(const uint8_t *data, uint16_t length) STC_REENTRANT
{
#if STC_CORE_USB_LAYOUT
    control_data = data; control_zlp = length < host_length && !(length % 64u);
    control_left = length < host_length ? length : host_length;
    control_state = 1; usb_hw_write(14, 0); usb_hw_write(17, 0x40); control_in();
#else
    (void)data; (void)length;
#endif
}
void stc_usb_control_receive(uint8_t length, stc_usb_control_callback callback) STC_REENTRANT
{
#if STC_CORE_USB_LAYOUT
    if (length > 64u) { stall(); return; }
    control_expected = length; control_callback = callback; control_state = 2;
    usb_hw_write(17, 0x40);
#else
    (void)length; (void)callback;
#endif
}
void stc_usb_ep_init(uint8_t ep, uint8_t receive, uint8_t packet_size) STC_REENTRANT
{
#if STC_CORE_USB_LAYOUT
    usb_hw_write(14, ep);
    usb_hw_write(16, packet_size / 8u); usb_hw_write(17, 0x48); usb_hw_write(18, 0x20);
    if (receive) { usb_hw_write(19, packet_size / 8u); usb_hw_write(20, 0x90); usb_hw_write(21, 0); }
#else
    (void)ep; (void)receive; (void)packet_size;
#endif
}
uint8_t stc_usb_ep_ready(uint8_t ep) STC_REENTRANT
{
#if STC_CORE_USB_LAYOUT
    usb_hw_write(14, ep); return !(usb_hw_read(17) & 1u) && !(halt_in & (1u << ep));
#else
    (void)ep; return 0;
#endif
}
void stc_usb_ep_send(uint8_t ep, const uint8_t *data, uint8_t length) STC_REENTRANT
{
#if STC_CORE_USB_LAYOUT
    uint8_t i;
    usb_hw_write(14, ep);
    for (i = 0; i < length; ++i) usb_hw_write(32u + ep, data[i]);
    usb_hw_write(17, 1);
#else
    (void)ep; (void)data; (void)length;
#endif
}
int stc_usb_ep_available(uint8_t ep) STC_REENTRANT
{
#if STC_CORE_USB_LAYOUT
    usb_hw_write(14, ep);
    if (!(halt_out & (1u << ep)) && (usb_hw_read(20) & 1u)) return usb_hw_read(22);
#else
    (void)ep;
#endif
    return -1;
}
void stc_usb_ep_receive(uint8_t ep, uint8_t *data, uint8_t length) STC_REENTRANT
{
#if STC_CORE_USB_LAYOUT
    uint8_t i;
    usb_hw_write(14, ep);
    for (i = 0; i < length; ++i) { uint8_t value = usb_hw_read(32u + ep); if (data) data[i] = value; }
    usb_hw_write(20, 0);
#else
    (void)ep; (void)data; (void)length;
#endif
}
void stc_usb_reboot_to_isp(void) STC_REENTRANT
{
#if STC_CORE_USB_LAYOUT
    stc_usb_end();
    usb_hw_reboot();
#endif
}
