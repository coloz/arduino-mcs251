// Internal integer formatting shared by String and Print; no heap allocation.
#ifndef STCXX_NUMBER_FORMAT_H
#define STCXX_NUMBER_FORMAT_H
#include <stdint.h>

namespace stc_detail {
template <typename T>
char *formatInteger(char *end, T value, uint8_t base, char alphabet)
{
    if (base < 2u || base > 36u) base = 10u;
    if ((base & (base - 1u)) == 0u) {
        // BIN/OCT/HEX (and bases 4/32) never need software division.
        const uint8_t shift = base == 2u ? 1u : base == 4u ? 2u :
                              base == 8u ? 3u : base == 16u ? 4u : 5u;
        const uint8_t mask = base - 1u;
        do {
            const uint8_t digit = (uint8_t)value & mask;
            *--end = (char)(digit < 10u ? '0' + digit : alphabet + digit - 10u);
            value >>= shift;
        } while (value != 0u);
    } else {
        do {
            const uint8_t digit = (uint8_t)(value % base);
            *--end = (char)(digit < 10u ? '0' + digit : alphabet + digit - 10u);
            value /= base;
        } while (value != 0u);
    }
    return end;
}
}
#endif
