#ifndef STCXX_HARDWARE_SERIAL_H
#define STCXX_HARDWARE_SERIAL_H

#include <stddef.h>
#include <stdint.h>

#include "Stream.h"

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

class HardwareSerial : public Stream
{
public:
    HardwareSerial() : _configurationError(false) {}

    void begin(unsigned long baud) { begin(baud, SERIAL_8N1); }
    void begin(unsigned long baud, uint16_t configuration);
    void end();
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
    bool _configurationError;
};

extern HardwareSerial Serial1;
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
