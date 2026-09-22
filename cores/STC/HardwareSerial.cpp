#include "HardwareSerial.h"

bool HardwareSerial::setPinsChecked(uint8_t rx, uint8_t tx)
{
    _configurationError = !_backend->setPins(_port, rx, tx);
    return !_configurationError;
}
void HardwareSerial::begin(unsigned long baud, uint16_t configuration)
{
    if (configuration != SERIAL_8N1) {
        end(); _configurationError = true; return;
    }
    _configurationError = !_backend->begin(_port, baud);
}
void HardwareSerial::end() { _backend->end(_port); }
int HardwareSerial::available() { return _backend->available(_port); }
int HardwareSerial::peek() { return _backend->peek(_port); }
int HardwareSerial::read() { return _backend->read(_port); }
int HardwareSerial::availableForWrite() { return _backend->availableForWrite(_port); }
void HardwareSerial::flush() { _backend->flush(_port); }
size_t HardwareSerial::write(uint8_t value)
{
    size_t written = _backend->write(_port, value);
    if (!written) setWriteError();
    return written;
}
bool HardwareSerial::overflow() { return _backend->overflow(_port); }
