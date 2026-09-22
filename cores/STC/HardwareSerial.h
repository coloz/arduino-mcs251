#ifndef STCXX_HARDWARE_SERIAL_H
#define STCXX_HARDWARE_SERIAL_H

#include <stddef.h>
#include <stdint.h>

#include "Stream.h"
#include "stc_family.h"

#define SERIAL_5N1 0x00u
#define SERIAL_6N1 0x02u
#define SERIAL_7N1 0x04u
#define SERIAL_8N1 0x06u
#define SERIAL_5N2 0x08u
#define SERIAL_6N2 0x0au
#define SERIAL_7N2 0x0cu
#define SERIAL_8N2 0x0eu
#define SERIAL_5E1 0x20u
#define SERIAL_6E1 0x22u
#define SERIAL_7E1 0x24u
#define SERIAL_8E1 0x26u
#define SERIAL_5E2 0x28u
#define SERIAL_6E2 0x2au
#define SERIAL_7E2 0x2cu
#define SERIAL_8E2 0x2eu
#define SERIAL_5O1 0x30u
#define SERIAL_6O1 0x32u
#define SERIAL_7O1 0x34u
#define SERIAL_8O1 0x36u
#define SERIAL_5O2 0x38u
#define SERIAL_6O2 0x3au
#define SERIAL_7O2 0x3cu
#define SERIAL_8O2 0x3eu

#define STCXX_SERIAL_CONFIG_8N1_ONLY 1

struct HardwareSerialBackend {
    bool (*begin)(uint8_t, unsigned long);
    void (*end)(uint8_t);
    bool (*setPins)(uint8_t, uint8_t, uint8_t);
    int (*available)(uint8_t);
    int (*peek)(uint8_t);
    int (*read)(uint8_t);
    int (*availableForWrite)(uint8_t);
    void (*flush)(uint8_t);
    size_t (*write)(uint8_t, uint8_t);
    bool (*overflow)(uint8_t);
};

class HardwareSerial : public Stream
{
public:
    HardwareSerial();
    /* Numbered construction is for available UART2..UART8 only. Use the
     * default constructor or Serial1 for UART1 so unused drivers stay unlinked.
     * An unavailable number starts with configurationError() set. */
    explicit HardwareSerial(uint8_t port);

    void begin(unsigned long baud) { begin(baud, SERIAL_8N1); }
    void begin(unsigned long baud, uint16_t configuration);
    void end();
    /* Select a complete hardware RX/TX route before begin(), or after end(). */
    bool setPinsChecked(uint8_t rx, uint8_t tx);
    void setPins(uint8_t rx, uint8_t tx) { (void)setPinsChecked(rx, tx); }
    int available();
    int peek();
    int read();
    int availableForWrite();
    void flush();
    size_t write(uint8_t value);
    using Print::write;

    size_t write(unsigned long value) { return write((uint8_t)value); }
    size_t write(long value) { return write((uint8_t)value); }
    size_t write(unsigned int value) { return write((uint8_t)value); }
    size_t write(int value) { return write((uint8_t)value); }

    bool overflow();
    bool configurationError() const { return _configurationError; }
    operator bool() const { return !_configurationError; }

private:
    uint8_t _port;
    const HardwareSerialBackend *_backend;
    bool _configurationError;
};

extern HardwareSerial Serial1;
#if STC_CORE_UART_AVAILABLE_MASK & 2
extern HardwareSerial Serial2;
#endif
extern HardwareSerial Serial3;
extern HardwareSerial Serial4;
#if STC_CORE_UART_COUNT > 4
extern HardwareSerial Serial5;
extern HardwareSerial Serial6;
extern HardwareSerial Serial7;
extern HardwareSerial Serial8;
#endif
namespace arduino {
using ::HardwareSerial;
}
#define Serial0 Serial1
#if ARDUINO_USB_CDC_ON_BOOT
# include "USBCDC.h"
# define Serial USBSerial
#else
# define Serial Serial1
#endif

#endif
