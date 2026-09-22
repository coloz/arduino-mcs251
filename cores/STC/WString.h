#ifndef STCXX_WSTRING_H
#define STCXX_WSTRING_H

#include <stddef.h>
#include <stdint.h>

#include "pgmspace.h"

class StringSumHelper;

class String
{
    typedef void (String::*StringIfHelperType)() const;
    void StringIfHelper() const {}

public:
    String(const char *text = "");
    String(const char *text, unsigned int length);
    String(const uint8_t *text, unsigned int length);
    String(const String &other);
    String(const __FlashStringHelper *text);
    String(String &&other);
    String(StringSumHelper &&other);
    explicit String(char value);
    explicit String(unsigned char value, unsigned char base = 10u);
    explicit String(int value, unsigned char base = 10u);
    explicit String(unsigned int value, unsigned char base = 10u);
    explicit String(long value, unsigned char base = 10u);
    explicit String(unsigned long value, unsigned char base = 10u);
    explicit String(float value, unsigned char decimalPlaces = 2u);
    explicit String(double value, unsigned char decimalPlaces = 2u);
    ~String();

    unsigned char reserve(unsigned int size);
    unsigned int length() const { return _length; }
    unsigned int capacity() const { return _capacity; }
    bool isEmpty() const { return _length == 0u; }
    void clear();

    String &operator=(const String &other);
    String &operator=(String &&other);
    String &operator=(StringSumHelper &&other);
    String &operator=(const char *text);
    String &operator=(const __FlashStringHelper *text);

    unsigned char concat(const String &value);
    unsigned char concat(const char *value);
    unsigned char concat(const char *value, unsigned int length);
    unsigned char concat(const uint8_t *value, unsigned int length)
    {
        return concat(reinterpret_cast<const char *>(value), length);
    }
    unsigned char concat(char value);
    unsigned char concat(unsigned char value);
    unsigned char concat(int value);
    unsigned char concat(unsigned int value);
    unsigned char concat(long value);
    unsigned char concat(unsigned long value);
    unsigned char concat(float value);
    unsigned char concat(double value);
    unsigned char concat(const __FlashStringHelper *value);

    String &operator+=(const String &value) { (void)concat(value); return *this; }
    String &operator+=(const char *value) { (void)concat(value); return *this; }
    String &operator+=(char value) { (void)concat(value); return *this; }
    String &operator+=(unsigned char value) { (void)concat(value); return *this; }
    String &operator+=(int value) { (void)concat(value); return *this; }
    String &operator+=(unsigned int value) { (void)concat(value); return *this; }
    String &operator+=(long value) { (void)concat(value); return *this; }
    String &operator+=(unsigned long value) { (void)concat(value); return *this; }
    String &operator+=(float value) { (void)concat(value); return *this; }
    String &operator+=(double value) { (void)concat(value); return *this; }
    String &operator+=(const __FlashStringHelper *value)
    {
        (void)concat(value);
        return *this;
    }

    /* Match Arduino AVR's safe-bool conversion.  A plain implicit operator
     * bool would make expressions such as String("x") + 1 ambiguous with
     * built-in arithmetic, while this still supports bool valid = value. */
    operator StringIfHelperType() const
    {
        return _buffer != 0 ? &String::StringIfHelper : 0;
    }

    int compareTo(const String &other) const;
    int compareTo(const char *text) const;
    unsigned char equals(const String &other) const;
    unsigned char equals(const char *text) const;
    unsigned char operator==(const String &other) const { return equals(other); }
    unsigned char operator==(const char *text) const { return equals(text); }
    unsigned char operator!=(const String &other) const { return !equals(other); }
    unsigned char operator!=(const char *text) const { return !equals(text); }
    unsigned char operator<(const String &other) const;
    unsigned char operator>(const String &other) const;
    unsigned char operator<=(const String &other) const;
    unsigned char operator>=(const String &other) const;
    unsigned char equalsIgnoreCase(const String &other) const;
    unsigned char startsWith(const String &prefix) const;
    unsigned char startsWith(const String &prefix, unsigned int offset) const;
    unsigned char endsWith(const String &suffix) const;

    char charAt(unsigned int index) const;
    void setCharAt(unsigned int index, char value);
    char operator[](unsigned int index) const;
    char &operator[](unsigned int index);
    void getBytes(unsigned char *destination, unsigned int size,
                  unsigned int index = 0u) const;
    void toCharArray(char *destination, unsigned int size,
                     unsigned int index = 0u) const
    {
        getBytes(reinterpret_cast<unsigned char *>(destination), size, index);
    }
    const char *c_str() const { return _buffer; }
    char *begin() { return _buffer; }
    char *end() { return _buffer != 0 ? _buffer + _length : 0; }
    const char *begin() const { return _buffer; }
    const char *end() const { return _buffer != 0 ? _buffer + _length : 0; }

    int indexOf(char value) const;
    int indexOf(char value, unsigned int fromIndex) const;
    int indexOf(const String &value) const;
    int indexOf(const String &value, unsigned int fromIndex) const;
    int lastIndexOf(char value) const;
    int lastIndexOf(char value, unsigned int fromIndex) const;
    int lastIndexOf(const String &value) const;
    int lastIndexOf(const String &value, unsigned int fromIndex) const;
    String substring(unsigned int beginIndex) const
    {
        return substring(beginIndex, _length);
    }
    String substring(unsigned int beginIndex, unsigned int endIndex) const;

    void replace(char findValue, char replacement);
    void replace(const String &findValue, const String &replacement);
    void remove(unsigned int index);
    void remove(unsigned int index, unsigned int count);
    void toLowerCase();
    void toUpperCase();
    void trim();

    long toInt() const;
    float toFloat() const;
    double toDouble() const;

protected:
    char *_buffer;
    unsigned int _capacity;
    unsigned int _length;

    void init();
    void invalidate();
    unsigned char changeBuffer(unsigned int maximumLength);
    String &copy(const char *value, unsigned int length);
    String &copy(const __FlashStringHelper *value, unsigned int length);
    void move(String &other);

private:
    friend class StringSumHelper;
    friend StringSumHelper &operator+(const StringSumHelper &, const String &);
    friend StringSumHelper &operator+(const StringSumHelper &, const char *);
    friend StringSumHelper &operator+(const StringSumHelper &, char);
    friend StringSumHelper &operator+(const StringSumHelper &, unsigned char);
    friend StringSumHelper &operator+(const StringSumHelper &, int);
    friend StringSumHelper &operator+(const StringSumHelper &, unsigned int);
    friend StringSumHelper &operator+(const StringSumHelper &, long);
    friend StringSumHelper &operator+(const StringSumHelper &, unsigned long);
    friend StringSumHelper &operator+(const StringSumHelper &, float);
    friend StringSumHelper &operator+(const StringSumHelper &, double);
    friend StringSumHelper &operator+(const StringSumHelper &,
                                      const __FlashStringHelper *);
};

inline unsigned char operator==(const char *left, const String &right)
{
    return right.equals(left);
}

inline unsigned char operator!=(const char *left, const String &right)
{
    return !right.equals(left);
}

inline bool operator<(const char *left, const String &right)
{
    return right.compareTo(left) > 0;
}
inline bool operator>(const char *left, const String &right)
{
    return right.compareTo(left) < 0;
}
inline bool operator<=(const char *left, const String &right)
{
    return right.compareTo(left) >= 0;
}
inline bool operator>=(const char *left, const String &right)
{
    return right.compareTo(left) <= 0;
}

class StringSumHelper : public String
{
public:
    StringSumHelper(const String &value) : String(value) {}
    StringSumHelper(const char *value) : String(value) {}
    StringSumHelper(char value) : String(value) {}
    StringSumHelper(unsigned char value) : String(value) {}
    StringSumHelper(int value) : String(value) {}
    StringSumHelper(unsigned int value) : String(value) {}
    StringSumHelper(long value) : String(value) {}
    StringSumHelper(unsigned long value) : String(value) {}
    StringSumHelper(float value) : String(value) {}
    StringSumHelper(double value) : String(value) {}
    StringSumHelper(const __FlashStringHelper *value) : String(value) {}
};

StringSumHelper &operator+(const StringSumHelper &left, const String &right);
StringSumHelper &operator+(const StringSumHelper &left, const char *right);
StringSumHelper &operator+(const StringSumHelper &left, char right);
StringSumHelper &operator+(const StringSumHelper &left, unsigned char right);
StringSumHelper &operator+(const StringSumHelper &left, int right);
StringSumHelper &operator+(const StringSumHelper &left, unsigned int right);
StringSumHelper &operator+(const StringSumHelper &left, long right);
StringSumHelper &operator+(const StringSumHelper &left, unsigned long right);
StringSumHelper &operator+(const StringSumHelper &left, float right);
StringSumHelper &operator+(const StringSumHelper &left, double right);
StringSumHelper &operator+(const StringSumHelper &left,
                           const __FlashStringHelper *right);

namespace arduino {
using ::String;
using ::StringSumHelper;
using ::__FlashStringHelper;
}

#endif
