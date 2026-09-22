
#include <stdint.h>

static_assert(sizeof(float) == 4u,
              "the STC math runtime requires a 32-bit float");
static_assert(__FLT_RADIX__ == 2 && __FLT_MANT_DIG__ == 24 &&
                  __FLT_MAX_EXP__ == 128,
              "the STC math runtime requires IEEE-754 binary32");

namespace {

union StcxxFloatBits {
    float value;
    uint32_t bits;
};

static uint32_t stcxx_float_bits(float value)
{
    StcxxFloatBits representation;
    representation.value = value;
    return representation.bits;
}

static float stcxx_float_from_bits(uint32_t bits)
{
    StcxxFloatBits representation;
    representation.bits = bits;
    return representation.value;
}

static float stcxx_quiet_nan(void)
{
    return stcxx_float_from_bits(UINT32_C(0x7fc00000));
}

static void stcxx_normalize_finite(uint32_t magnitude,
                                   uint32_t *significand,
                                   int *exponent)
{
    uint32_t exponent_field = magnitude >> 23;
    uint32_t value = magnitude & UINT32_C(0x007fffff);

    if (exponent_field != 0u) {
        *significand = value | UINT32_C(0x00800000);
        *exponent = (int)exponent_field - 127;
        return;
    }

    *exponent = -126;
    while ((value & UINT32_C(0x00800000)) == 0u) {
        value <<= 1;
        --*exponent;
    }
    *significand = value;
}

} // namespace

extern "C" float fminf(float x, float y)
{
    const uint32_t xb = stcxx_float_bits(x);
    const uint32_t yb = stcxx_float_bits(y);
    if ((xb & UINT32_C(0x7fffffff)) > UINT32_C(0x7f800000)) return y;
    if ((yb & UINT32_C(0x7fffffff)) > UINT32_C(0x7f800000)) return x;
    if (((xb | yb) & UINT32_C(0x7fffffff)) == 0u)
        return stcxx_float_from_bits(xb | yb); // min(+0,-0) is -0
    return x < y ? x : y;
}

extern "C" float fmaxf(float x, float y)
{
    const uint32_t xb = stcxx_float_bits(x);
    const uint32_t yb = stcxx_float_bits(y);
    if ((xb & UINT32_C(0x7fffffff)) > UINT32_C(0x7f800000)) return y;
    if ((yb & UINT32_C(0x7fffffff)) > UINT32_C(0x7f800000)) return x;
    if (((xb | yb) & UINT32_C(0x7fffffff)) == 0u)
        return stcxx_float_from_bits(xb & yb); // max(+0,-0) is +0
    return x > y ? x : y;
}

extern "C" float fmodf(float numerator, float denominator)
{
    const uint32_t sign = stcxx_float_bits(numerator) &
                          UINT32_C(0x80000000);
    const uint32_t numerator_magnitude = stcxx_float_bits(numerator) &
                                         UINT32_C(0x7fffffff);
    const uint32_t denominator_magnitude = stcxx_float_bits(denominator) &
                                           UINT32_C(0x7fffffff);
    uint32_t numerator_significand;
    uint32_t denominator_significand;
    int numerator_exponent;
    int denominator_exponent;

    /* A zero divisor, NaN divisor, NaN numerator, or infinite numerator has
     * no defined finite remainder.  This freestanding runtime has no floating
     * exception environment, but it still returns a quiet NaN. */
    if (denominator_magnitude == 0u ||
        denominator_magnitude > UINT32_C(0x7f800000) ||
        numerator_magnitude >= UINT32_C(0x7f800000)) {
        return stcxx_quiet_nan();
    }

    if (numerator_magnitude < denominator_magnitude) {
        return numerator;
    }
    if (numerator_magnitude == denominator_magnitude) {
        return stcxx_float_from_bits(sign);
    }

    stcxx_normalize_finite(numerator_magnitude,
                           &numerator_significand,
                           &numerator_exponent);
    stcxx_normalize_finite(denominator_magnitude,
                           &denominator_significand,
                           &denominator_exponent);

    /* Binary long division keeps the remainder in a 25-bit integer.  Unlike
     * x - y * trunc(x / y), it never forms a quotient that can overflow. */
    while (numerator_exponent > denominator_exponent) {
        if (numerator_significand >= denominator_significand) {
            numerator_significand -= denominator_significand;
            if (numerator_significand == 0u) {
                return stcxx_float_from_bits(sign);
            }
        }
        numerator_significand <<= 1;
        --numerator_exponent;
    }

    if (numerator_significand >= denominator_significand) {
        numerator_significand -= denominator_significand;
        if (numerator_significand == 0u) {
            return stcxx_float_from_bits(sign);
        }
    }

    while ((numerator_significand & UINT32_C(0x00800000)) == 0u) {
        numerator_significand <<= 1;
        --numerator_exponent;
    }

    if (numerator_exponent >= -126) {
        const uint32_t exponent_field =
            (uint32_t)(numerator_exponent + 127) << 23;
        return stcxx_float_from_bits(
            sign | exponent_field |
            (numerator_significand & UINT32_C(0x007fffff)));
    }

    return stcxx_float_from_bits(
        sign |
        (numerator_significand >> (uint32_t)(-126 - numerator_exponent)));
}

extern "C" float roundf(float value)
{
    const uint32_t bits = stcxx_float_bits(value);
    const uint32_t sign = bits & UINT32_C(0x80000000);
    uint32_t magnitude = bits & UINT32_C(0x7fffffff);

    if (magnitude >= UINT32_C(0x7f800000)) {
        return value;
    }
    if (magnitude < UINT32_C(0x3f000000)) {
        return stcxx_float_from_bits(sign);
    }
    if (magnitude < UINT32_C(0x3f800000)) {
        return stcxx_float_from_bits(sign | UINT32_C(0x3f800000));
    }
    if (magnitude >= UINT32_C(0x4b000000)) {
        return value;
    }

    const uint32_t exponent = (magnitude >> 23) - 127u;
    const uint32_t fractional_bits = 23u - exponent;
    const uint32_t fractional_mask =
        (UINT32_C(1) << fractional_bits) - UINT32_C(1);
    if ((magnitude & fractional_mask) == 0u) {
        return value;
    }

    magnitude += UINT32_C(1) << (fractional_bits - 1u);
    magnitude &= ~fractional_mask;
    return stcxx_float_from_bits(sign | magnitude);
}

extern "C" float truncf(float value)
{
    const uint32_t bits = stcxx_float_bits(value);
    const uint32_t sign = bits & UINT32_C(0x80000000);
    uint32_t magnitude = bits & UINT32_C(0x7fffffff);

    if (magnitude >= UINT32_C(0x7f800000) ||
        magnitude >= UINT32_C(0x4b000000)) {
        return value;
    }
    if (magnitude < UINT32_C(0x3f800000)) {
        return stcxx_float_from_bits(sign);
    }

    const uint32_t exponent = (magnitude >> 23) - 127u;
    const uint32_t fractional_mask =
        (UINT32_C(1) << (23u - exponent)) - UINT32_C(1);
    magnitude &= ~fractional_mask;
    return stcxx_float_from_bits(sign | magnitude);
}
