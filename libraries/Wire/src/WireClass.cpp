
#include "WireClass.h"
#include <hal/stc_c_hal.h>

TwoWire Wire;
#if STC_CORE_I2C_COUNT > 1
TwoWire Wire1(1);
#define WIRE_CALL(name, invalid, ...) (_bus >= STC_CORE_I2C_COUNT ? (invalid) : (_bus == 1 ? Wire1_##name(__VA_ARGS__) : Wire_##name(__VA_ARGS__)))
#else
#define WIRE_CALL(name, invalid, ...) (_bus == 0 ? Wire_##name(__VA_ARGS__) : (invalid))
#endif
void TwoWire::begin(uint8_t address) { WIRE_CALL(beginSlave, (void)0, address); }
void TwoWire::onReceive(void (*callback)(int)) { WIRE_CALL(onReceive, (void)0, callback); }
void TwoWire::onRequest(void (*callback)(void)) { WIRE_CALL(onRequest, (void)0, callback); }

void TwoWire::begin()
{
    WIRE_CALL(begin, (void)0, );
}

void TwoWire::end()
{
    WIRE_CALL(end, (void)0, );
}

void TwoWire::setPins(uint8_t data, uint8_t clock)
{
    WIRE_CALL(setPins, (void)0, data, clock);
}

uint8_t TwoWire::setPinsChecked(uint8_t data, uint8_t clock)
{
    return WIRE_CALL(setPinsChecked, WIRE_STATUS_OTHER_ERROR, data, clock);
}
uint8_t TwoWire::configurationError() { return WIRE_CALL(configurationError, WIRE_STATUS_OTHER_ERROR, ); }
uint8_t TwoWire::lastError() { return WIRE_CALL(lastError, WIRE_STATUS_OTHER_ERROR, ); }

void TwoWire::setClock(uint32_t clock)
{
    WIRE_CALL(setClock, (void)0, (unsigned long)clock);
}

void TwoWire::setWireTimeout(uint32_t timeout, bool resetWithTimeout)
{
    WIRE_CALL(setWireTimeout, (void)0, timeout, resetWithTimeout ? 1u : 0u);
}

bool TwoWire::getWireTimeoutFlag()
{
    return WIRE_CALL(getWireTimeoutFlag, 0u, ) != 0u;
}

void TwoWire::clearWireTimeoutFlag()
{
    WIRE_CALL(clearWireTimeoutFlag, (void)0, );
}

void TwoWire::beginTransmission(uint8_t address)
{
    WIRE_CALL(beginTransmission, (void)0, address);
}

uint8_t TwoWire::endTransmission(bool sendStop)
{
    return WIRE_CALL(endTransmissionStop, WIRE_STATUS_OTHER_ERROR, sendStop ? 1u : 0u);
}

size_t TwoWire::requestFrom(uint8_t address, size_t quantity, bool sendStop)
{
    uint8_t bounded = quantity > 255u ? 255u : (uint8_t)quantity;
    return (size_t)WIRE_CALL(requestFromStop, 0u, address, bounded,
                                        sendStop ? 1u : 0u);
}

uint8_t TwoWire::requestFrom(uint8_t address, uint8_t quantity,
                             uint32_t internalAddress,
                             uint8_t internalAddressSize, uint8_t sendStop)
{
    return WIRE_CALL(requestFromInternal, 0u, address, quantity, internalAddress,
                                    internalAddressSize,
                                    sendStop != 0u ? 1u : 0u);
}

size_t TwoWire::write(uint8_t value)
{
    size_t written = WIRE_CALL(write, 0u, value);
    if (written == 0u) {
        setWriteError();
    }
    return written;
}

size_t TwoWire::write(const uint8_t *buffer, size_t length)
{
    size_t written = 0u;

    if (buffer == 0 && length != 0u) {
        setWriteError();
        return 0u;
    }
    while (written < length && WIRE_CALL(write, 0u, buffer[written]) != 0u) {
        ++written;
    }
    if (written != length) {
        setWriteError();
    }
    return written;
}

int TwoWire::available()
{
    return WIRE_CALL(available, 0, );
}

int TwoWire::read()
{
    return WIRE_CALL(read, -1, );
}

int TwoWire::peek()
{
    return WIRE_CALL(peek, -1, );
}
