#ifndef STCXX_PRINT_H
#define STCXX_PRINT_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "Printable.h"
#include "WString.h"

#ifndef DEC
# define DEC 10
#endif
#ifndef HEX
# define HEX 16
#endif
#ifndef OCT
# define OCT 8
#endif
#ifdef BIN
# undef BIN
#endif
#define BIN 2

class Print
{
public:
    Print() : _writeError(0) {}

    int getWriteError() const { return _writeError; }
    void clearWriteError() { setWriteError(0); }

    virtual size_t write(uint8_t value) = 0;
    virtual size_t write(const uint8_t *buffer, size_t size);
    size_t write(const char *text);
    size_t write(const char *buffer, size_t size)
    {
        return write(reinterpret_cast<const uint8_t *>(buffer), size);
    }

    virtual int availableForWrite() { return 0; }
    virtual void flush() {}

    size_t print(const __FlashStringHelper *text);
    size_t print(const String &text);
    size_t print(const char text[]);
    size_t print(char value);
    size_t print(unsigned char value, int base = DEC);
    size_t print(int value, int base = DEC);
    size_t print(unsigned int value, int base = DEC);
    size_t print(long value, int base = DEC);
    size_t print(unsigned long value, int base = DEC);
    size_t print(long long value, int base = DEC);
    size_t print(unsigned long long value, int base = DEC);
    size_t print(double value, int digits = 2);
    size_t print(const Printable &value);

    size_t println(const __FlashStringHelper *text);
    size_t println(const String &text);
    size_t println(const char text[]);
    size_t println(char value);
    size_t println(unsigned char value, int base = DEC);
    size_t println(int value, int base = DEC);
    size_t println(unsigned int value, int base = DEC);
    size_t println(long value, int base = DEC);
    size_t println(unsigned long value, int base = DEC);
    size_t println(long long value, int base = DEC);
    size_t println(unsigned long long value, int base = DEC);
    size_t println(double value, int digits = 2);
    size_t println(const Printable &value);
    size_t println();

protected:
    void setWriteError(int error = 1) { _writeError = error; }

private:
    int _writeError;
    size_t writeFormatted(const char *buffer, size_t size);
    size_t printNumber(unsigned long value, uint8_t base);
    size_t printULLNumber(unsigned long long value, uint8_t base);
    size_t printFloat(double value, uint8_t digits);
};

namespace arduino {
using ::Print;
}

#endif
