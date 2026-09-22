#ifndef STCXX_CLIENT_H
#define STCXX_CLIENT_H

#include <stddef.h>
#include <stdint.h>

#include "IPAddress.h"
#include "Stream.h"

class Client : public Stream
{
public:
    virtual int connect(IPAddress address, uint16_t port) = 0;
    virtual int connect(const char *host, uint16_t port) = 0;
    virtual size_t write(uint8_t value) = 0;
    virtual size_t write(const uint8_t *buffer, size_t size) = 0;
    virtual int available() = 0;
    virtual int read() = 0;
    virtual int read(uint8_t *buffer, size_t size) = 0;
    virtual int peek() = 0;
    virtual void flush() = 0;
    virtual void stop() = 0;
    virtual uint8_t connected() = 0;
    virtual operator bool() = 0;

    using Print::write;

protected:
    uint8_t *rawIPAddress(IPAddress &address)
    {
        return address.raw_address();
    }
};

namespace arduino {
using ::Client;
} // namespace arduino

#endif
