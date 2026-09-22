/* LLVM may introduce copysignf while optimizing binary32 arithmetic (for
 * example truncf). Keep this fallback in native C so LLVM cannot rewrite
 * its bit operations into a recursive call to the same missing libcall. */
#include <stdint.h>

typedef char stcxx_copysign_requires_binary32[(sizeof(float) == 4) ? 1 : -1];

float copysignf(float magnitude, float sign)
{
    union { float value; uint32_t bits; } x, y;
    x.value = magnitude;
    y.value = sign;
    x.bits = (x.bits & UINT32_C(0x7fffffff)) |
             (y.bits & UINT32_C(0x80000000));
    return x.value;
}
