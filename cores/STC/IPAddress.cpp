
#include "IPAddress.h"

#include "Print.h"
#include "stcxx_libc.h"

IPAddress::IPAddress() : _type(IPv4)
{
    memset(_address.bytes, 0, sizeof(_address.bytes));
}

IPAddress::IPAddress(IPType type) : _type(type)
{
    memset(_address.bytes, 0, sizeof(_address.bytes));
}

IPAddress::IPAddress(uint8_t o1, uint8_t o2, uint8_t o3, uint8_t o4)
    : _type(IPv4)
{
    memset(_address.bytes, 0, sizeof(_address.bytes));
    _address.bytes[12] = o1;
    _address.bytes[13] = o2;
    _address.bytes[14] = o3;
    _address.bytes[15] = o4;
}

IPAddress::IPAddress(uint8_t o1, uint8_t o2, uint8_t o3, uint8_t o4,
                     uint8_t o5, uint8_t o6, uint8_t o7, uint8_t o8,
                     uint8_t o9, uint8_t o10, uint8_t o11, uint8_t o12,
                     uint8_t o13, uint8_t o14, uint8_t o15, uint8_t o16)
    : _type(IPv6)
{
    const uint8_t octets[16] = {o1, o2, o3, o4, o5, o6, o7, o8,
                                o9, o10, o11, o12, o13, o14, o15, o16};
    memcpy(_address.bytes, octets, sizeof(octets));
}

IPAddress::IPAddress(uint32_t address) : _type(IPv4)
{
    memset(_address.bytes, 0, sizeof(_address.bytes));
    _address.dword[IPADDRESS_V4_DWORD_INDEX] = address;
}

IPAddress::IPAddress(const uint8_t *address) : _type(IPv4)
{
    memset(_address.bytes, 0, sizeof(_address.bytes));
    if (address != 0) {
        memcpy(&_address.bytes[IPADDRESS_V4_BYTES_INDEX], address, 4u);
    }
}

IPAddress::IPAddress(IPType type, const uint8_t *address) : _type(type)
{
    memset(_address.bytes, 0, sizeof(_address.bytes));
    if (address != 0) {
        if (type == IPv4) {
            memcpy(&_address.bytes[IPADDRESS_V4_BYTES_INDEX], address, 4u);
        } else {
            memcpy(_address.bytes, address, 16u);
        }
    }
}

IPAddress::IPAddress(const char *address) : _type(IPv4)
{
    memset(_address.bytes, 0, sizeof(_address.bytes));
    (void)fromString(address);
}

bool IPAddress::fromString(const char *address)
{
    if (address == 0) {
        return false;
    }
    return fromString4(address) || fromString6(address);
}

bool IPAddress::fromString4(const char *address)
{
    uint8_t parsed[4] = {0u, 0u, 0u, 0u};
    int accumulator = -1;
    uint8_t dots = 0u;
    const char *current = address;

    if (current == 0 || *current == '\0') {
        return false;
    }
    while (*current != '\0') {
        char value = *current++;
        if (value >= '0' && value <= '9') {
            accumulator = accumulator < 0 ? value - '0'
                                           : accumulator * 10 + value - '0';
            if (accumulator > 255) {
                return false;
            }
        } else if (value == '.') {
            if (dots >= 3u || accumulator < 0) {
                return false;
            }
            parsed[dots++] = (uint8_t)accumulator;
            accumulator = -1;
        } else {
            return false;
        }
    }
    if (dots != 3u || accumulator < 0) {
        return false;
    }
    parsed[3] = (uint8_t)accumulator;
    memset(_address.bytes, 0, sizeof(_address.bytes));
    memcpy(&_address.bytes[IPADDRESS_V4_BYTES_INDEX], parsed, sizeof(parsed));
    _type = IPv4;
    return true;
}

bool IPAddress::fromString6(const char *address)
{
    uint16_t fields[8];
    uint8_t fieldCount = 0u;
    int gapIndex = -1;
    const char *current = address;

    memset(fields, 0, sizeof(fields));
    if (current == 0 || *current == '\0') {
        return false;
    }
    if (*current == ':') {
        if (current[1] != ':') {
            return false;
        }
        gapIndex = 0;
        current += 2;
        if (*current == '\0') {
            memset(_address.bytes, 0, sizeof(_address.bytes));
            _type = IPv6;
            return true;
        }
    }
    while (*current != '\0') {
        uint32_t accumulator = 0u;
        uint8_t digits = 0u;
        if (fieldCount >= 8u) {
            return false;
        }
        while (*current != '\0' && *current != ':') {
            unsigned char value = (unsigned char)*current++;
            uint8_t digit;
            if (value >= '0' && value <= '9') {
                digit = (uint8_t)(value - '0');
            } else if (value >= 'a' && value <= 'f') {
                digit = (uint8_t)(value - 'a' + 10u);
            } else if (value >= 'A' && value <= 'F') {
                digit = (uint8_t)(value - 'A' + 10u);
            } else {
                return false;
            }
            if (++digits > 4u) {
                return false;
            }
            accumulator = accumulator * 16u + digit;
        }
        if (digits == 0u) {
            return false;
        }
        fields[fieldCount++] = (uint16_t)accumulator;
        if (*current == '\0') {
            break;
        }
        ++current;
        if (*current == ':') {
            if (gapIndex >= 0) {
                return false;
            }
            gapIndex = (int)fieldCount;
            ++current;
            if (*current == '\0') {
                break;
            }
        } else if (*current == '\0') {
            return false;
        }
    }
    if (gapIndex < 0) {
        if (fieldCount != 8u) {
            return false;
        }
    } else {
        uint8_t zeros;
        int index;
        if (fieldCount >= 8u) {
            return false;
        }
        zeros = (uint8_t)(8u - fieldCount);
        for (index = (int)fieldCount - 1; index >= gapIndex; --index) {
            fields[index + zeros] = fields[index];
        }
        for (index = gapIndex; index < gapIndex + zeros; ++index) {
            fields[index] = 0u;
        }
    }
    for (uint8_t index = 0u; index < 8u; ++index) {
        _address.bytes[index * 2u] = (uint8_t)(fields[index] >> 8);
        _address.bytes[index * 2u + 1u] = (uint8_t)fields[index];
    }
    _type = IPv6;
    return true;
}

IPAddress &IPAddress::operator=(const uint8_t *address)
{
    if (address != 0) {
        memset(_address.bytes, 0, sizeof(_address.bytes));
        memcpy(&_address.bytes[IPADDRESS_V4_BYTES_INDEX], address, 4u);
        _type = IPv4;
    }
    return *this;
}

IPAddress &IPAddress::operator=(uint32_t address)
{
    memset(_address.bytes, 0, sizeof(_address.bytes));
    _address.dword[IPADDRESS_V4_DWORD_INDEX] = address;
    _type = IPv4;
    return *this;
}

IPAddress &IPAddress::operator=(const char *address)
{
    (void)fromString(address);
    return *this;
}

bool IPAddress::operator==(const IPAddress &other) const
{
    return _type == other._type &&
           memcmp(_address.bytes, other._address.bytes,
                  sizeof(_address.bytes)) == 0;
}

bool IPAddress::operator==(const uint8_t *address) const
{
    return _type == IPv4 && address != 0 &&
           memcmp(&_address.bytes[IPADDRESS_V4_BYTES_INDEX], address, 4u) == 0;
}

uint8_t IPAddress::operator[](int index) const
{
    return _type == IPv4
               ? _address.bytes[IPADDRESS_V4_BYTES_INDEX + index]
               : _address.bytes[index];
}

uint8_t &IPAddress::operator[](int index)
{
    return _type == IPv4
               ? _address.bytes[IPADDRESS_V4_BYTES_INDEX + index]
               : _address.bytes[index];
}

String IPAddress::toString4() const
{
    String result(_address.bytes[12]);
    result += '.';
    result += _address.bytes[13];
    result += '.';
    result += _address.bytes[14];
    result += '.';
    result += _address.bytes[15];
    return result;
}

String IPAddress::toString6() const
{
    String result;
    int longestStart = -1;
    int longestLength = 1;
    int currentStart = -1;
    int currentLength = 0;

    for (int field = 0; field < 8; ++field) {
        if (_address.bytes[field * 2] == 0u &&
            _address.bytes[field * 2 + 1] == 0u) {
            if (currentStart < 0) {
                currentStart = field;
                currentLength = 1;
            } else {
                ++currentLength;
            }
            if (currentLength > longestLength) {
                longestStart = currentStart;
                longestLength = currentLength;
            }
        } else {
            currentStart = -1;
            currentLength = 0;
        }
    }
    for (int field = 0; field < 8; ++field) {
        if (field == longestStart) {
            if (field == 0) {
                result += ':';
            }
            result += ':';
            field += longestLength - 1;
            continue;
        }
        uint16_t value = (uint16_t)(((uint16_t)_address.bytes[field * 2] << 8) |
                                    _address.bytes[field * 2 + 1]);
        String digits((unsigned int)value, HEX);
        digits.toLowerCase();
        result += digits;
        if (field != 7) {
            result += ':';
        }
    }
    return result;
}

String IPAddress::toString() const
{
    return _type == IPv4 ? toString4() : toString6();
}

size_t IPAddress::printTo(Print &output) const
{
    if (_type == IPv4) {
        size_t written = 0u;
        for (int index = 12; index < 15; ++index) {
            written += output.print(_address.bytes[index], DEC);
            written += output.print('.');
        }
        written += output.print(_address.bytes[15], DEC);
        return written;
    }

    String text = toString6();
    return output.write(text.c_str(), text.length());
}

const IPAddress IN6ADDR_ANY(IPv6);
const IPAddress INADDR_NONE(0u, 0u, 0u, 0u);
