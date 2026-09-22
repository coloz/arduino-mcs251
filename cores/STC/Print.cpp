
#include "Print.h"

#include <limits.h>
#include <math.h>

#include "stcxx_libc.h"

size_t Print::write(const uint8_t *buffer, size_t size)
{
    size_t written = 0u;
    if (buffer == 0) {
        return 0u;
    }
    while (written < size) {
        if (write(buffer[written]) == 0u) {
            setWriteError();
            break;
        }
        ++written;
    }
    return written;
}

size_t Print::write(const char *text)
{
    return text == 0
               ? 0u
               : write(reinterpret_cast<const uint8_t *>(text), strlen(text));
}

size_t Print::print(const __FlashStringHelper *text)
{
#if STCXX_FLASH_STRINGS
    PGM_P current;
    size_t written = 0u;
    if (text == 0) {
        return 0u;
    }
    current = reinterpret_cast<PGM_P>(text);
    for (;;) {
        uint8_t value = (uint8_t)STCXX_FLASH_READ_BYTE(current++);
        if (value == 0u || write(value) == 0u) {
            if (value != 0u) {
                setWriteError();
            }
            return written;
        }
        ++written;
    }
#else
    /* In data-pointer fallback mode external libraries can still expose the
     * canonical Arduino flash-helper type (for example ArduinoJson::f_str()).
     * The pointer names ordinary data here; this preserves the API without
     * claiming any program-memory/RAM saving. */
    return write(reinterpret_cast<const char *>(text));
#endif
}

size_t Print::print(const String &text)
{
    return write(text.c_str(), text.length());
}

size_t Print::print(const char text[])
{
    return write(text);
}

size_t Print::print(char value)
{
    return write((uint8_t)value);
}

size_t Print::print(unsigned char value, int base)
{
    return print((unsigned long)value, base);
}

size_t Print::print(int value, int base)
{
    return print((long)value, base);
}

size_t Print::print(unsigned int value, int base)
{
    return print((unsigned long)value, base);
}

size_t Print::print(long value, int base)
{
    size_t written = 0u;
    if (base == 0) {
        return write((uint8_t)value);
    }
    if (base == DEC && value < 0L) {
        written = write((uint8_t)'-');
        return written + printNumber(0UL - (unsigned long)value, 10u);
    }
    return written + printNumber((unsigned long)value, (uint8_t)base);
}

size_t Print::print(unsigned long value, int base)
{
    return base == 0 ? write((uint8_t)value)
                     : printNumber(value, (uint8_t)base);
}

size_t Print::print(long long value, int base)
{
    size_t written = 0u;
    if (base == 0) {
        return write((uint8_t)value);
    }
    if (base == DEC && value < 0LL) {
        written = write((uint8_t)'-');
        return written +
               printULLNumber(0ULL - (unsigned long long)value, 10u);
    }
    return printULLNumber((unsigned long long)value, (uint8_t)base);
}

size_t Print::print(unsigned long long value, int base)
{
    return base == 0 ? write((uint8_t)value)
                     : printULLNumber(value, (uint8_t)base);
}

size_t Print::print(double value, int digits)
{
    if (digits < 0) {
        digits = 0;
    }
    if (digits > UCHAR_MAX) {
        digits = UCHAR_MAX;
    }
    return printFloat(value, (uint8_t)digits);
}

size_t Print::print(const Printable &value)
{
    return value.printTo(*this);
}

size_t Print::println()
{
    return write("\r\n");
}

#define STCXX_PRINTLN_IMPLEMENTATION(type)                \
    size_t Print::println(type value)                     \
    {                                                     \
        size_t written = print(value);                    \
        return written + println();                       \
    }

STCXX_PRINTLN_IMPLEMENTATION(const __FlashStringHelper *)
STCXX_PRINTLN_IMPLEMENTATION(const String &)
STCXX_PRINTLN_IMPLEMENTATION(char)
STCXX_PRINTLN_IMPLEMENTATION(const Printable &)

#undef STCXX_PRINTLN_IMPLEMENTATION

size_t Print::println(const char text[])
{
    size_t written = print(text);
    return written + println();
}

#define STCXX_PRINTLN_BASE_IMPLEMENTATION(type)                  \
    size_t Print::println(type value, int base)                  \
    {                                                            \
        size_t written = print(value, base);                     \
        return written + println();                              \
    }

STCXX_PRINTLN_BASE_IMPLEMENTATION(unsigned char)
STCXX_PRINTLN_BASE_IMPLEMENTATION(int)
STCXX_PRINTLN_BASE_IMPLEMENTATION(unsigned int)
STCXX_PRINTLN_BASE_IMPLEMENTATION(long)
STCXX_PRINTLN_BASE_IMPLEMENTATION(unsigned long)
STCXX_PRINTLN_BASE_IMPLEMENTATION(long long)
STCXX_PRINTLN_BASE_IMPLEMENTATION(unsigned long long)

#undef STCXX_PRINTLN_BASE_IMPLEMENTATION

size_t Print::println(double value, int digits)
{
    size_t written = print(value, digits);
    return written + println();
}

size_t Print::printNumber(unsigned long value, uint8_t base)
{
    char buffer[8u * sizeof(unsigned long) + 1u];
    char *current = buffer + sizeof(buffer);
    *--current = '\0';
    if (base < 2u || base > 36u) {
        base = 10u;
    }
    do {
        uint8_t digit = (uint8_t)(value % base);
        *--current = (char)(digit < 10u ? ('0' + digit)
                                      : ('A' + digit - 10u));
        value /= base;
    } while (value != 0u);
    return write(current);
}

size_t Print::printULLNumber(unsigned long long value, uint8_t base)
{
    char buffer[8u * sizeof(unsigned long long) + 1u];
    char *current = buffer + sizeof(buffer);
    *--current = '\0';
    if (base < 2u || base > 36u) {
        base = 10u;
    }
    do {
        uint8_t digit = (uint8_t)(value % base);
        *--current = (char)(digit < 10u ? ('0' + digit)
                                      : ('A' + digit - 10u));
        value /= base;
    } while (value != 0u);
    return write(current);
}

size_t Print::printFloat(double value, uint8_t digits)
{
    size_t written = 0u;
    double rounding = 0.5;
    unsigned long integerPart;
    double remainder;
    uint8_t index;

    if (isnan(value)) {
        return print("nan");
    }
    if (isinf(value)) {
        return print("inf");
    }
    if (value > 4294967040.0 || value < -4294967040.0) {
        return print("ovf");
    }
    if (value < 0.0) {
        written += print('-');
        value = -value;
    }
    for (index = 0u; index < digits; ++index) {
        rounding /= 10.0;
    }
    value += rounding;
    integerPart = (unsigned long)value;
    remainder = value - (double)integerPart;
    written += print(integerPart, DEC);
    if (digits != 0u) {
        written += print('.');
    }
    while (digits-- != 0u) {
        unsigned int digit;
        remainder *= 10.0;
        digit = (unsigned int)remainder;
        written += print(digit, DEC);
        remainder -= digit;
    }
    return written;
}
