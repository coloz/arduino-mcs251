/* SPDX-License-Identifier: MIT */
#include <Arduino.h>
#include "USBCDC.h"
#include "stc_usb_cdc.h"
#if STC_CORE_USB_LAYOUT && !STC_USB_HID_ONLY
USBCDC::USBCDC() : _txTimeout(100) { stc_cdc_register(); }
void USBCDC::begin(unsigned long baud) {
    (void)baud; // CDC line coding is set by the host, not a UART clock divisor.
    clearWriteError();
    if (!stc_cdc_begin()) setWriteError();
}
void USBCDC::begin(unsigned long baud, uint16_t config) { (void)config; begin(baud); }
void USBCDC::end() { stc_cdc_end(); }
int USBCDC::available() { return stc_cdc_available(); }
int USBCDC::peek() { return stc_cdc_peek(); }
int USBCDC::read() { return stc_cdc_read(); }
int USBCDC::availableForWrite() { return stc_cdc_write_space(); }
void USBCDC::flush() { if (!stc_cdc_flush(_txTimeout)) setWriteError(); }
size_t USBCDC::write(uint8_t value) { return write(&value, 1); }
size_t USBCDC::write(const uint8_t *data, size_t size) {
    size_t written = stc_cdc_write(data, size, _txTimeout);
    if (written != size) setWriteError();
    return written;
}
USBCDC::operator bool() { return stc_cdc_connected() != 0; }
bool USBCDC::dtr() { return (stc_cdc_line_state() & 1u) != 0; }
bool USBCDC::rts() { return (stc_cdc_line_state() & 2u) != 0; }
unsigned long USBCDC::baud() { return stc_cdc_baud(); }
uint8_t USBCDC::stopbits() { return stc_cdc_line_format(4); }
uint8_t USBCDC::paritytype() { return stc_cdc_line_format(5); }
uint8_t USBCDC::numbits() { return stc_cdc_line_format(6); }
int32_t USBCDC::readBreak() { return stc_cdc_read_break(); }
void USBCDC::enableReboot(bool enabled) { stc_cdc_enable_reboot(enabled); }
bool USBCDC::rebootEnabled() { return stc_cdc_reboot_enabled() != 0; }
USBCDC USBSerial;
#endif
