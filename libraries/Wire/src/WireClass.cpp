
#include "WireClass.h"
#include <hal/stc_c_hal.h>

TwoWire Wire;
void TwoWire::begin(uint8_t address) { Wire_beginSlave(address); }
void TwoWire::onReceive(void (*callback)(int)) { Wire_onReceive(callback); }
void TwoWire::onRequest(void (*callback)(void)) { Wire_onRequest(callback); }

void TwoWire::begin()
{
    Wire_begin();
}

void TwoWire::end()
{
    Wire_end();
}

void TwoWire::setPins(uint8_t data, uint8_t clock)
{
    Wire_setPins(data, clock);
}

uint8_t TwoWire::setPinsChecked(uint8_t data, uint8_t clock)
{
    return Wire_setPinsChecked(data, clock);
}
uint8_t TwoWire::configurationError() { return Wire_configurationError(); }
uint8_t TwoWire::lastError() { return Wire_lastError(); }

void TwoWire::setClock(uint32_t clock)
{
    Wire_setClock((unsigned long)clock);
}

void TwoWire::setWireTimeout(uint32_t timeout, bool resetWithTimeout)
{
    Wire_setWireTimeout(timeout, resetWithTimeout ? 1u : 0u);
}

bool TwoWire::getWireTimeoutFlag()
{
    return Wire_getWireTimeoutFlag() != 0u;
}

void TwoWire::clearWireTimeoutFlag()
{
    Wire_clearWireTimeoutFlag();
}

void TwoWire::beginTransmission(uint8_t address)
{
    Wire_beginTransmission(address);
}

uint8_t TwoWire::endTransmission(bool sendStop)
{
    return Wire_endTransmissionStop(sendStop ? 1u : 0u);
}

size_t TwoWire::requestFrom(uint8_t address, size_t quantity, bool sendStop)
{
    uint8_t bounded = quantity > 255u ? 255u : (uint8_t)quantity;
    return (size_t)Wire_requestFromStop(address, bounded,
                                        sendStop ? 1u : 0u);
}

uint8_t TwoWire::requestFrom(uint8_t address, uint8_t quantity,
                             uint32_t internalAddress,
                             uint8_t internalAddressSize, uint8_t sendStop)
{
    return Wire_requestFromInternal(address, quantity, internalAddress,
                                    internalAddressSize,
                                    sendStop != 0u ? 1u : 0u);
}

size_t TwoWire::write(uint8_t value)
{
    size_t written = Wire_write(value);
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
    while (written < length && Wire_write(buffer[written]) != 0u) {
        ++written;
    }
    if (written != length) {
        setWriteError();
    }
    return written;
}

int TwoWire::available()
{
    return Wire_available();
}

int TwoWire::read()
{
    return Wire_read();
}

int TwoWire::peek()
{
    return Wire_peek();
}
