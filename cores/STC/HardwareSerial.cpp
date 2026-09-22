
#include "HardwareSerial.h"

#include "hal/stc_c_hal.h"

HardwareSerial Serial1;

void HardwareSerial::begin(unsigned long baud, uint16_t configuration)
{
    if (configuration != SERIAL_8N1) {
        Serial_end();
        _configurationError = true;
        return;
    }
    _configurationError = false;
    Serial_begin(baud);
    _configurationError = !Serial_active();
}

void HardwareSerial::end()
{
    Serial_end();
}

int HardwareSerial::available()
{
    return Serial_available();
}

int HardwareSerial::peek()
{
    return Serial_peek();
}

int HardwareSerial::read()
{
    return Serial_read();
}

int HardwareSerial::availableForWrite()
{
    return Serial_availableForWrite();
}

void HardwareSerial::flush()
{
    Serial_flush();
}

size_t HardwareSerial::write(uint8_t value)
{
    size_t written = Serial_write(value);
    if (written == 0u) {
        setWriteError();
    }
    return written;
}

bool HardwareSerial::overflow()
{
    return Serial_overflow();
}
