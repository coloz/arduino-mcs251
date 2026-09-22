#include "Arduino.h"

static unsigned long stc_random_state = 1UL;

void randomSeed(unsigned long seed)
{
    if (seed != 0UL) {
        stc_random_state = seed;
    }
}

static unsigned long stc_random_next(void)
{
    stc_random_state = stc_random_state * 1103515245UL + 12345UL;
    return stc_random_state;
}

long random(long upper_bound)
{
    if (upper_bound <= 0L) {
        return 0L;
    }
    return (long)(stc_random_next() % (unsigned long)upper_bound);
}

long random_minmax(long lower_bound, long upper_bound) STC_REENTRANT
{
    if (lower_bound >= upper_bound) {
        return lower_bound;
    }
    return lower_bound + random(upper_bound - lower_bound);
}

long map(long value, long from_low, long from_high,
         long to_low, long to_high) STC_REENTRANT
{
    if (from_high == from_low) {
        return to_low;
    }
    return (value - from_low) * (to_high - to_low) /
           (from_high - from_low) + to_low;
}
