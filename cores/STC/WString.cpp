
#include "WString.h"

#include <limits.h>
#include <stdint.h>

#include "stcxx_allocator.h"
#include "stcxx_libc.h"

namespace {

unsigned int stcxx_text_length(const char *text)
{
    size_t length;
    if (text == 0) {
        return 0u;
    }
    length = strlen(text);
    return length > (size_t)UINT_MAX ? UINT_MAX : (unsigned int)length;
}

unsigned int stcxx_format_unsigned(char *destination, unsigned long value,
                                   unsigned char base)
{
    char reverse[8u * sizeof(unsigned long) + 1u];
    unsigned int length = 0u;
    unsigned int index;

    if (base < 2u || base > 36u) {
        base = 10u;
    }
    do {
        unsigned char digit = (unsigned char)(value % base);
        reverse[length++] = (char)(digit < 10u ? ('0' + digit)
                                               : ('a' + digit - 10u));
        value /= base;
    } while (value != 0u);

    for (index = 0u; index < length; ++index) {
        destination[index] = reverse[length - index - 1u];
    }
    destination[length] = '\0';
    return length;
}

unsigned int stcxx_format_signed(char *destination, long value,
                                 unsigned char base)
{
    if (base == 10u && value < 0L) {
        destination[0] = '-';
        return 1u + stcxx_format_unsigned(destination + 1,
                                          0UL - (unsigned long)value, base);
    }
    return stcxx_format_unsigned(destination, (unsigned long)value, base);
}

unsigned int stcxx_format_double(char *destination, double value,
                                 unsigned char decimals)
{
    unsigned int length = 0u;
    unsigned long integerPart;
    double remainder;
    double rounding = 0.5;
    unsigned char index;

    if (value != value) {
        memcpy(destination, "nan", 4u);
        return 3u;
    }
    if (value > 4294967040.0 || value < -4294967040.0) {
        memcpy(destination, "ovf", 4u);
        return 3u;
    }
    if (value < 0.0) {
        destination[length++] = '-';
        value = -value;
    }
    if (decimals > 9u) {
        decimals = 9u;
    }
    for (index = 0u; index < decimals; ++index) {
        rounding /= 10.0;
    }
    value += rounding;
    integerPart = (unsigned long)value;
    remainder = value - (double)integerPart;
    length += stcxx_format_unsigned(destination + length, integerPart, 10u);
    if (decimals != 0u) {
        destination[length++] = '.';
        while (decimals-- != 0u) {
            unsigned char digit;
            remainder *= 10.0;
            digit = (unsigned char)remainder;
            destination[length++] = (char)('0' + digit);
            remainder -= digit;
        }
        destination[length] = '\0';
    }
    return length;
}

} // namespace

String::String(const char *text)
{
    init();
    if (text != 0) {
        copy(text, stcxx_text_length(text));
    }
}

String::String(const char *text, unsigned int length)
{
    init();
    if (text != 0) {
        copy(text, length);
    }
}

String::String(const uint8_t *text, unsigned int length)
{
    init();
    if (text != 0) {
        copy(reinterpret_cast<const char *>(text), length);
    }
}

String::String(const String &other)
{
    init();
    if (other._buffer != 0) {
        copy(other._buffer, other._length);
    }
}

String::String(const __FlashStringHelper *text)
{
    init();
    if (text != 0) {
#if STCXX_FLASH_STRINGS
        PGM_P source = reinterpret_cast<PGM_P>(text);
        size_t length = stcxx_strlen_P(source);
#else
        const char *source = reinterpret_cast<const char *>(text);
        size_t length = stcxx_text_length(source);
#endif
        if (length <= (size_t)UINT_MAX) {
            copy(text, (unsigned int)length);
        }
    }
}

String::String(String &&other)
{
    init();
    move(other);
}

String::String(StringSumHelper &&other)
{
    init();
    move(other);
}

String::String(char value)
{
    char text[2] = {value, '\0'};
    init();
    copy(text, 1u);
}

String::String(unsigned char value, unsigned char base)
{
    char text[8u * sizeof(unsigned long) + 1u];
    unsigned int length;
    init();
    length = stcxx_format_unsigned(text, value, base);
    copy(text, length);
}

String::String(int value, unsigned char base)
{
    char text[8u * sizeof(unsigned long) + 2u];
    unsigned int length;
    init();
    length = stcxx_format_signed(text, value, base);
    copy(text, length);
}

String::String(unsigned int value, unsigned char base)
{
    char text[8u * sizeof(unsigned long) + 1u];
    unsigned int length;
    init();
    length = stcxx_format_unsigned(text, value, base);
    copy(text, length);
}

String::String(long value, unsigned char base)
{
    char text[8u * sizeof(unsigned long) + 2u];
    unsigned int length;
    init();
    length = stcxx_format_signed(text, value, base);
    copy(text, length);
}

String::String(unsigned long value, unsigned char base)
{
    char text[8u * sizeof(unsigned long) + 1u];
    unsigned int length;
    init();
    length = stcxx_format_unsigned(text, value, base);
    copy(text, length);
}

String::String(float value, unsigned char decimals)
{
    char text[32];
    unsigned int length;
    init();
    length = stcxx_format_double(text, value, decimals);
    copy(text, length);
}

String::String(double value, unsigned char decimals)
{
    char text[32];
    unsigned int length;
    init();
    length = stcxx_format_double(text, value, decimals);
    copy(text, length);
}

String::~String()
{
    if (_buffer != 0) {
        stcxx_free(_buffer);
    }
}

void String::init()
{
    _buffer = 0;
    _capacity = 0u;
    _length = 0u;
}

void String::invalidate()
{
    if (_buffer != 0) {
        stcxx_free(_buffer);
    }
    init();
}

unsigned char String::changeBuffer(unsigned int maximumLength)
{
    char *replacement;
    if (maximumLength == UINT_MAX) {
        return 0u;
    }
    replacement = static_cast<char *>(
        stcxx_realloc(_buffer, (size_t)maximumLength + 1u));
    if (replacement == 0) {
        return 0u;
    }
    _buffer = replacement;
    _capacity = maximumLength;
    return 1u;
}

unsigned char String::reserve(unsigned int size)
{
    if (_buffer != 0 && _capacity >= size) {
        return 1u;
    }
    if (changeBuffer(size) == 0u) {
        return 0u;
    }
    if (_length == 0u) {
        _buffer[0] = '\0';
    }
    return 1u;
}

void String::clear()
{
    _length = 0u;
    if (_buffer != 0) {
        _buffer[0] = '\0';
    }
}

String &String::copy(const char *value, unsigned int length)
{
    if (value == 0 || reserve(length) == 0u) {
        invalidate();
        return *this;
    }
    memmove(_buffer, value, length);
    _buffer[length] = '\0';
    _length = length;
    return *this;
}

String &String::copy(const __FlashStringHelper *value, unsigned int length)
{
#if STCXX_FLASH_STRINGS
    unsigned int index;
    PGM_P source;
    if (value == 0 || reserve(length) == 0u) {
        invalidate();
        return *this;
    }
    source = reinterpret_cast<PGM_P>(value);
    for (index = 0u; index < length; ++index) {
        _buffer[index] = (char)STCXX_FLASH_READ_BYTE(source + index);
    }
    _buffer[length] = '\0';
    _length = length;
    return *this;
#else
    return copy(reinterpret_cast<const char *>(value), length);
#endif
}

void String::move(String &other)
{
    if (this == &other) {
        return;
    }
    if (_buffer != 0) {
        stcxx_free(_buffer);
    }
    _buffer = other._buffer;
    _capacity = other._capacity;
    _length = other._length;
    other.init();
}

String &String::operator=(const String &other)
{
    if (this != &other) {
        if (other._buffer == 0) {
            invalidate();
        } else {
            copy(other._buffer, other._length);
        }
    }
    return *this;
}

String &String::operator=(String &&other)
{
    move(other);
    return *this;
}

String &String::operator=(StringSumHelper &&other)
{
    move(other);
    return *this;
}

String &String::operator=(const char *text)
{
    if (text == 0) {
        invalidate();
    } else {
        copy(text, stcxx_text_length(text));
    }
    return *this;
}

String &String::operator=(const __FlashStringHelper *text)
{
    if (text == 0) {
        invalidate();
    } else {
#if STCXX_FLASH_STRINGS
        size_t length = stcxx_strlen_P(reinterpret_cast<PGM_P>(text));
#else
        size_t length =
            stcxx_text_length(reinterpret_cast<const char *>(text));
#endif
        if (length > (size_t)UINT_MAX) {
            invalidate();
        } else {
            copy(text, (unsigned int)length);
        }
    }
    return *this;
}

unsigned char String::concat(const char *value, unsigned int length)
{
    unsigned int newLength;
    unsigned int sourceOffset = 0u;
    unsigned char aliases = 0u;

    if (value == 0) {
        return 0u;
    }
    if (length == 0u) {
        return 1u;
    }
    if (_length > UINT_MAX - length) {
        return 0u;
    }
    if (_buffer != 0) {
        for (;;) {
            if (value == _buffer + sourceOffset) {
                if (length > _length - sourceOffset) {
                    return 0u;
                }
                aliases = 1u;
                break;
            }
            if (sourceOffset == _length) {
                break;
            }
            ++sourceOffset;
        }
    }
    newLength = _length + length;
    if (reserve(newLength) == 0u) {
        return 0u;
    }
    if (aliases != 0u) {
        value = _buffer + sourceOffset;
    }
    memmove(_buffer + _length, value, length);
    _length = newLength;
    _buffer[_length] = '\0';
    return 1u;
}

unsigned char String::concat(const String &value)
{
    return value._buffer == 0 ? 0u : concat(value._buffer, value._length);
}

unsigned char String::concat(const char *value)
{
    return value == 0 ? 0u : concat(value, stcxx_text_length(value));
}

unsigned char String::concat(char value)
{
    return concat(&value, 1u);
}

unsigned char String::concat(unsigned char value)
{
    char text[8u * sizeof(unsigned long) + 1u];
    unsigned int length = stcxx_format_unsigned(text, value, 10u);
    return concat(text, length);
}

unsigned char String::concat(int value)
{
    char text[8u * sizeof(unsigned long) + 2u];
    unsigned int length = stcxx_format_signed(text, value, 10u);
    return concat(text, length);
}

unsigned char String::concat(unsigned int value)
{
    char text[8u * sizeof(unsigned long) + 1u];
    unsigned int length = stcxx_format_unsigned(text, value, 10u);
    return concat(text, length);
}

unsigned char String::concat(long value)
{
    char text[8u * sizeof(unsigned long) + 2u];
    unsigned int length = stcxx_format_signed(text, value, 10u);
    return concat(text, length);
}

unsigned char String::concat(unsigned long value)
{
    char text[8u * sizeof(unsigned long) + 1u];
    unsigned int length = stcxx_format_unsigned(text, value, 10u);
    return concat(text, length);
}

unsigned char String::concat(float value)
{
    char text[32];
    unsigned int length = stcxx_format_double(text, value, 2u);
    return concat(text, length);
}

unsigned char String::concat(double value)
{
    char text[32];
    unsigned int length = stcxx_format_double(text, value, 2u);
    return concat(text, length);
}

unsigned char String::concat(const __FlashStringHelper *value)
{
#if !STCXX_FLASH_STRINGS
    return concat(reinterpret_cast<const char *>(value));
#else
    size_t flashLength;
    unsigned int newLength;
    unsigned int index;
    PGM_P source;

    if (value == 0) {
        return 0u;
    }
    source = reinterpret_cast<PGM_P>(value);
    flashLength = stcxx_strlen_P(source);
    if (flashLength > (size_t)UINT_MAX ||
        _length > UINT_MAX - (unsigned int)flashLength) {
        return 0u;
    }
    newLength = _length + (unsigned int)flashLength;
    if (reserve(newLength) == 0u) {
        return 0u;
    }
    for (index = 0u; index < (unsigned int)flashLength; ++index) {
        _buffer[_length + index] =
            (char)STCXX_FLASH_READ_BYTE(source + index);
    }
    _length = newLength;
    _buffer[_length] = '\0';
    return 1u;
#endif
}

int String::compareTo(const String &other) const
{
    return compareTo(other._buffer);
}

int String::compareTo(const char *text) const
{
    if (_buffer == 0) {
        return text == 0 ? 0 : -(unsigned char)text[0];
    }
    if (text == 0) {
        return _length == 0u ? 0 : (unsigned char)_buffer[0];
    }
    return strcmp(_buffer, text);
}

unsigned char String::equals(const String &other) const
{
    return (_length == other._length && compareTo(other) == 0) ? 1u : 0u;
}

unsigned char String::equals(const char *text) const
{
    if (_length == 0u) {
        return (text == 0 || text[0] == '\0') ? 1u : 0u;
    }
    return (_buffer != 0 && text != 0 && strcmp(_buffer, text) == 0) ? 1u : 0u;
}

unsigned char String::operator<(const String &other) const
{
    return compareTo(other) < 0 ? 1u : 0u;
}

unsigned char String::operator>(const String &other) const
{
    return compareTo(other) > 0 ? 1u : 0u;
}

unsigned char String::operator<=(const String &other) const
{
    return compareTo(other) <= 0 ? 1u : 0u;
}

unsigned char String::operator>=(const String &other) const
{
    return compareTo(other) >= 0 ? 1u : 0u;
}

unsigned char String::equalsIgnoreCase(const String &other) const
{
    unsigned int index;
    if (_length != other._length) {
        return 0u;
    }
    for (index = 0u; index < _length; ++index) {
        if (tolower((unsigned char)_buffer[index]) !=
            tolower((unsigned char)other._buffer[index])) {
            return 0u;
        }
    }
    return 1u;
}

unsigned char String::startsWith(const String &prefix) const
{
    return startsWith(prefix, 0u);
}

unsigned char String::startsWith(const String &prefix, unsigned int offset) const
{
    if (_buffer == 0 || prefix._buffer == 0 || offset > _length ||
        prefix._length > _length - offset) {
        return 0u;
    }
    return memcmp(_buffer + offset, prefix._buffer, prefix._length) == 0 ? 1u : 0u;
}

unsigned char String::endsWith(const String &suffix) const
{
    if (_buffer == 0 || suffix._buffer == 0 || suffix._length > _length) {
        return 0u;
    }
    return memcmp(_buffer + _length - suffix._length,
                  suffix._buffer, suffix._length) == 0 ? 1u : 0u;
}

char String::charAt(unsigned int index) const
{
    return (*this)[index];
}

void String::setCharAt(unsigned int index, char value)
{
    if (_buffer != 0 && index < _length) {
        _buffer[index] = value;
    }
}

char String::operator[](unsigned int index) const
{
    return _buffer != 0 && index < _length ? _buffer[index] : '\0';
}

char &String::operator[](unsigned int index)
{
    static char dummy;
    if (_buffer == 0 || index >= _length) {
        dummy = '\0';
        return dummy;
    }
    return _buffer[index];
}

void String::getBytes(unsigned char *destination, unsigned int size,
                      unsigned int index) const
{
    unsigned int count;
    if (destination == 0 || size == 0u) {
        return;
    }
    if (_buffer == 0 || index >= _length) {
        destination[0] = 0u;
        return;
    }
    count = size - 1u;
    if (count > _length - index) {
        count = _length - index;
    }
    memcpy(destination, _buffer + index, count);
    destination[count] = 0u;
}

int String::indexOf(char value) const
{
    return indexOf(value, 0u);
}

int String::indexOf(char value, unsigned int fromIndex) const
{
    if (_buffer == 0 || fromIndex >= _length) {
        return -1;
    }
    for (unsigned int index = fromIndex; index < _length; ++index) {
        if (_buffer[index] == value) {
            return (int)index;
        }
    }
    return -1;
}

int String::indexOf(const String &value) const
{
    return indexOf(value, 0u);
}

int String::indexOf(const String &value, unsigned int fromIndex) const
{
    if (_buffer == 0 || value._buffer == 0 || fromIndex > _length) {
        return -1;
    }
    if (value._length == 0u) {
        return (int)fromIndex;
    }
    if (fromIndex == _length || value._length > _length - fromIndex) {
        return -1;
    }
    for (unsigned int index = fromIndex;
         index <= _length - value._length; ++index) {
        if (memcmp(_buffer + index, value._buffer, value._length) == 0) {
            return (int)index;
        }
    }
    return -1;
}

int String::lastIndexOf(char value) const
{
    return _length == 0u ? -1 : lastIndexOf(value, _length - 1u);
}

int String::lastIndexOf(char value, unsigned int fromIndex) const
{
    unsigned int index;
    if (_buffer == 0 || _length == 0u) {
        return -1;
    }
    if (fromIndex >= _length) {
        fromIndex = _length - 1u;
    }
    index = fromIndex + 1u;
    while (index-- != 0u) {
        if (_buffer[index] == value) {
            return (int)index;
        }
    }
    return -1;
}

int String::lastIndexOf(const String &value) const
{
    if (value._length == 0u || value._length > _length) {
        return -1;
    }
    return lastIndexOf(value, _length - value._length);
}

int String::lastIndexOf(const String &value, unsigned int fromIndex) const
{
    unsigned int index;
    if (_buffer == 0 || value._buffer == 0 || value._length == 0u ||
        value._length > _length) {
        return -1;
    }
    if (fromIndex > _length - value._length) {
        fromIndex = _length - value._length;
    }
    index = fromIndex + 1u;
    while (index-- != 0u) {
        if (memcmp(_buffer + index, value._buffer, value._length) == 0) {
            return (int)index;
        }
    }
    return -1;
}

String String::substring(unsigned int beginIndex, unsigned int endIndex) const
{
    String result;
    if (beginIndex > endIndex) {
        unsigned int temporary = beginIndex;
        beginIndex = endIndex;
        endIndex = temporary;
    }
    if (_buffer == 0 || beginIndex >= _length) {
        return result;
    }
    if (endIndex > _length) {
        endIndex = _length;
    }
    result.copy(_buffer + beginIndex, endIndex - beginIndex);
    return result;
}

void String::replace(char findValue, char replacement)
{
    unsigned int index;
    for (index = 0u; index < _length; ++index) {
        if (_buffer[index] == findValue) {
            _buffer[index] = replacement;
        }
    }
}

void String::replace(const String &findValue, const String &replacement)
{
    unsigned int occurrences = 0u;
    unsigned int searchIndex = 0u;
    unsigned int newLength;
    unsigned int sourceIndex = 0u;
    unsigned int destinationIndex = 0u;
    char *replacementBuffer;

    if (_buffer == 0 || findValue._buffer == 0 || findValue._length == 0u ||
        replacement._buffer == 0) {
        return;
    }
    while (findValue._length <= _length - searchIndex) {
        if (memcmp(_buffer + searchIndex, findValue._buffer,
                   findValue._length) == 0) {
            ++occurrences;
            searchIndex += findValue._length;
        } else {
            ++searchIndex;
        }
    }
    if (occurrences == 0u) {
        return;
    }
    /* AVR String performs equal-size and shrinking replacements in place.
     * Apart from avoiding needless heap pressure, this lets those operations
     * remain useful when the heap is already exhausted. */
    if (replacement._length <= findValue._length) {
        sourceIndex = 0u;
        destinationIndex = 0u;
        while (sourceIndex < _length) {
            if (findValue._length <= _length - sourceIndex &&
                memcmp(_buffer + sourceIndex, findValue._buffer,
                       findValue._length) == 0) {
                memmove(_buffer + destinationIndex, replacement._buffer,
                        replacement._length);
                destinationIndex += replacement._length;
                sourceIndex += findValue._length;
            } else {
                _buffer[destinationIndex++] = _buffer[sourceIndex++];
            }
        }
        _buffer[destinationIndex] = '\0';
        _length = destinationIndex;
        return;
    }
    if (replacement._length >= findValue._length) {
        unsigned int growth = replacement._length - findValue._length;
        if (growth != 0u && occurrences > (UINT_MAX - _length) / growth) {
            return;
        }
        newLength = _length + occurrences * growth;
    } else {
        newLength = _length -
                    occurrences * (findValue._length - replacement._length);
    }
    if (newLength == UINT_MAX) {
        return;
    }
    replacementBuffer = static_cast<char *>(stcxx_malloc((size_t)newLength + 1u));
    if (replacementBuffer == 0) {
        return;
    }
    while (sourceIndex < _length) {
        if (findValue._length <= _length - sourceIndex &&
            memcmp(_buffer + sourceIndex, findValue._buffer,
                   findValue._length) == 0) {
            memcpy(replacementBuffer + destinationIndex, replacement._buffer,
                   replacement._length);
            destinationIndex += replacement._length;
            sourceIndex += findValue._length;
        } else {
            replacementBuffer[destinationIndex++] = _buffer[sourceIndex++];
        }
    }
    replacementBuffer[newLength] = '\0';
    stcxx_free(_buffer);
    _buffer = replacementBuffer;
    _capacity = newLength;
    _length = newLength;
}

void String::remove(unsigned int index)
{
    remove(index, UINT_MAX);
}

void String::remove(unsigned int index, unsigned int count)
{
    if (_buffer == 0 || index >= _length || count == 0u) {
        return;
    }
    if (count > _length - index) {
        count = _length - index;
    }
    memmove(_buffer + index, _buffer + index + count,
            _length - index - count + 1u);
    _length -= count;
}

void String::toLowerCase()
{
    unsigned int index;
    for (index = 0u; index < _length; ++index) {
        _buffer[index] = (char)tolower((unsigned char)_buffer[index]);
    }
}

void String::toUpperCase()
{
    unsigned int index;
    for (index = 0u; index < _length; ++index) {
        _buffer[index] = (char)toupper((unsigned char)_buffer[index]);
    }
}

void String::trim()
{
    unsigned int first = 0u;
    unsigned int last = _length;
    if (_buffer == 0) {
        return;
    }
    while (first < _length && isspace((unsigned char)_buffer[first]) != 0) {
        ++first;
    }
    while (last > first && isspace((unsigned char)_buffer[last - 1u]) != 0) {
        --last;
    }
    if (first != 0u) {
        memmove(_buffer, _buffer + first, last - first);
    }
    _length = last - first;
    _buffer[_length] = '\0';
}

long String::toInt() const
{
    return _buffer == 0 ? 0L : strtol(_buffer, 0, 10);
}

float String::toFloat() const
{
    return (float)toDouble();
}

double String::toDouble() const
{
    return _buffer == 0 ? 0.0 : atof(_buffer);
}

#define STCXX_STRING_SUM_OPERATOR(type)                                    \
    StringSumHelper &operator+(const StringSumHelper &left, type right)    \
    {                                                                      \
        StringSumHelper &result = const_cast<StringSumHelper &>(left);     \
        if (result.concat(right) == 0u) {                                  \
            result.invalidate();                                           \
        }                                                                  \
        return result;                                                      \
    }

STCXX_STRING_SUM_OPERATOR(const String &)
STCXX_STRING_SUM_OPERATOR(const char *)
STCXX_STRING_SUM_OPERATOR(char)
STCXX_STRING_SUM_OPERATOR(unsigned char)
STCXX_STRING_SUM_OPERATOR(int)
STCXX_STRING_SUM_OPERATOR(unsigned int)
STCXX_STRING_SUM_OPERATOR(long)
STCXX_STRING_SUM_OPERATOR(unsigned long)
STCXX_STRING_SUM_OPERATOR(float)
STCXX_STRING_SUM_OPERATOR(double)
STCXX_STRING_SUM_OPERATOR(const __FlashStringHelper *)

#undef STCXX_STRING_SUM_OPERATOR
