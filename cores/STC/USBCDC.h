/* SPDX-License-Identifier: MIT */
#ifndef STC_USB_CDC_H
#define STC_USB_CDC_H
#include "Stream.h"
#include "stc_family.h"
#if STC_CORE_USB_LAYOUT && !STC_USB_HID_ONLY
class USBCDC : public Stream {
public:
    USBCDC();
    void begin(unsigned long baud = 115200);
    void begin(unsigned long baud, uint16_t config);
    void end();
    int available();
    int peek();
    int read();
    int availableForWrite();
    void flush();
    size_t write(uint8_t value);
    size_t write(const uint8_t *buffer, size_t size);
    using Print::write;
    operator bool();
    bool dtr();
    bool rts();
    unsigned long baud();
    uint8_t stopbits();
    uint8_t paritytype();
    uint8_t numbits();
    int32_t readBreak();
    void setTxTimeoutMs(unsigned long timeout) { _txTimeout = timeout; }
    void enableReboot(bool enabled);
    bool rebootEnabled();
private:
    unsigned long _txTimeout;
};
extern USBCDC USBSerial;
#define SerialUSB USBSerial
#endif
#endif
