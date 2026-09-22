#ifndef STCXX_IP_ADDRESS_H
#define STCXX_IP_ADDRESS_H

#include <stdint.h>

#include "Printable.h"
#include "WString.h"

#define IPADDRESS_V4_BYTES_INDEX 12
#define IPADDRESS_V4_DWORD_INDEX 3

class EthernetClass;
class DhcpClass;
class DNSClient;
class Client;
class Server;
class UDP;

enum IPType {
    IPv4,
    IPv6
};

class IPAddress : public Printable
{
public:
    IPAddress();
    explicit IPAddress(IPType type);
    IPAddress(uint8_t o1, uint8_t o2, uint8_t o3, uint8_t o4);
    IPAddress(uint8_t o1, uint8_t o2, uint8_t o3, uint8_t o4,
              uint8_t o5, uint8_t o6, uint8_t o7, uint8_t o8,
              uint8_t o9, uint8_t o10, uint8_t o11, uint8_t o12,
              uint8_t o13, uint8_t o14, uint8_t o15, uint8_t o16);
    IPAddress(uint32_t address);
    IPAddress(const uint8_t *address);
    IPAddress(IPType type, const uint8_t *address);
    IPAddress(const char *address);

    bool fromString(const char *address);
    bool fromString(const String &address) { return fromString(address.c_str()); }
    String toString() const;

    operator uint32_t() const
    {
        return _type == IPv4 ? _address.dword[IPADDRESS_V4_DWORD_INDEX] : 0u;
    }

    bool operator==(const IPAddress &other) const;
    bool operator!=(const IPAddress &other) const { return !(*this == other); }
    bool operator==(const uint8_t *address) const;

    uint8_t operator[](int index) const;
    uint8_t &operator[](int index);

    IPAddress &operator=(const uint8_t *address);
    IPAddress &operator=(uint32_t address);
    IPAddress &operator=(const char *address);

    size_t printTo(Print &output) const;
    IPType type() const { return _type; }

private:
    union AddressStorage {
        uint8_t bytes[16];
        uint32_t dword[4];
    } _address;
    IPType _type;

    uint8_t *raw_address()
    {
        return _type == IPv4 ? &_address.bytes[IPADDRESS_V4_BYTES_INDEX]
                             : _address.bytes;
    }

    bool fromString4(const char *address);
    bool fromString6(const char *address);
    String toString4() const;
    String toString6() const;

    friend class Client;
    friend class Server;
    friend class UDP;
    friend class ::EthernetClass;
    friend class ::DhcpClass;
    friend class ::DNSClient;
};

extern const IPAddress IN6ADDR_ANY;
extern const IPAddress INADDR_NONE;

namespace arduino {
using ::IPAddress;
using ::IPType;
using ::IPv4;
using ::IPv6;
using ::IN6ADDR_ANY;
using ::INADDR_NONE;
} // namespace arduino

#endif
