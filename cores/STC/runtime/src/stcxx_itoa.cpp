/* Arduino's nonstandard integer formatting API. No allocation is required. */
#include <stddef.h>

extern "C" char *ultoa(unsigned long value, char *buffer, int radix)
{
    if (buffer == NULL) return buffer;
    char *end = buffer;
    if (radix < 2 || radix > 36) {
        *end = '\0';
        return buffer;
    }
    do {
        const unsigned char digit = (unsigned char)(value % (unsigned long)radix);
        *end++ = (char)(digit < 10 ? '0' + digit : 'a' + digit - 10);
        value /= (unsigned long)radix;
    } while (value != 0);
    *end = '\0';
    for (char *left = buffer, *right = end - 1; left < right; ++left, --right) {
        const char temporary = *left;
        *left = *right;
        *right = temporary;
    }
    return buffer;
}

extern "C" char *ltoa(long value, char *buffer, int radix)
{
    if (buffer == NULL) return buffer;
    unsigned long magnitude = (unsigned long)value;
    if (value < 0 && radix == 10) {
        *buffer = '-';
        // Unsigned subtraction also handles LONG_MIN without signed overflow.
        ultoa(0UL - magnitude, buffer + 1, radix);
    } else {
        ultoa(magnitude, buffer, radix);
    }
    return buffer;
}

extern "C" char *utoa(unsigned int value, char *buffer, int radix)
{
    return ultoa((unsigned long)value, buffer, radix);
}

extern "C" char *itoa(int value, char *buffer, int radix)
{
    return radix == 10 ? ltoa((long)value, buffer, radix)
                       : utoa((unsigned int)value, buffer, radix);
}
