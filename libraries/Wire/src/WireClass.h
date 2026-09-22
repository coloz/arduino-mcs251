#ifndef STCXX_WIRE_CLASS_H
#define STCXX_WIRE_CLASS_H

#include <stddef.h>
#include <stdint.h>

#include "stcxx_config.h"
#include "Stream.h"
#include "stc_family.h"

#ifndef WIRE_BUFFER_LENGTH
# define WIRE_BUFFER_LENGTH 32u
#endif
#if (WIRE_BUFFER_LENGTH < 1u) || (WIRE_BUFFER_LENGTH > 255u)
# error "WIRE_BUFFER_LENGTH must be between 1 and 255 bytes"
#endif
#ifndef BUFFER_LENGTH
# define BUFFER_LENGTH WIRE_BUFFER_LENGTH
#endif
#ifndef WIRE_HAS_END
# define WIRE_HAS_END 1
#endif
#ifndef WIRE_HAS_TIMEOUT
# define WIRE_HAS_TIMEOUT 1
#endif
#ifndef WIRE_HAS_SLAVE
# define WIRE_HAS_SLAVE 1
#endif

#ifndef WIRE_STATUS_SUCCESS
# define WIRE_STATUS_SUCCESS 0u
# define WIRE_STATUS_DATA_TOO_LONG 1u
# define WIRE_STATUS_ADDRESS_NACK 2u
# define WIRE_STATUS_DATA_NACK 3u
# define WIRE_STATUS_OTHER_ERROR 4u
# define WIRE_STATUS_TIMEOUT 5u
#endif

class TwoWire : public Stream
{
public:
    /* bus=0 is Wire, bus=1 is Wire1 when present. Unavailable buses are
     * inert and report WIRE_STATUS_OTHER_ERROR through configurationError(). */
    explicit TwoWire(uint8_t bus = 0) : _bus(bus) {}
    void begin();
    void begin(uint8_t address);
    void onReceive(void (*callback)(int));
    void onRequest(void (*callback)(void));
    void end();
    void setPins(uint8_t data, uint8_t clock);
    uint8_t setPinsChecked(uint8_t data, uint8_t clock);
    uint8_t configurationError();
    uint8_t lastError();
    void setClock(uint32_t clock);
    void setWireTimeout(uint32_t timeout = 25000UL,
                        bool resetWithTimeout = false);
    bool getWireTimeoutFlag();
    void clearWireTimeoutFlag();

    void beginTransmission(uint8_t address);
    uint8_t endTransmission(bool sendStop = true);
    size_t requestFrom(uint8_t address, size_t quantity,
                       bool sendStop = true);
    uint8_t requestFrom(uint8_t address, uint8_t quantity,
                        uint32_t internalAddress,
                        uint8_t internalAddressSize, uint8_t sendStop);

    size_t write(uint8_t value);
    size_t write(const uint8_t *buffer, size_t length);
    using Print::write;

    size_t write(unsigned long value) { return write((uint8_t)value); }
    size_t write(long value) { return write((uint8_t)value); }
    size_t write(unsigned int value) { return write((uint8_t)value); }
    size_t write(int value) { return write((uint8_t)value); }

    int available();
    int read();
    int peek();
private:
    uint8_t _bus;
};

extern TwoWire Wire;
#if STC_CORE_I2C_COUNT > 1
extern TwoWire Wire1;
#endif

#endif
