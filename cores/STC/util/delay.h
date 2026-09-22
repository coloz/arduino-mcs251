#ifndef STC_CORE_UTIL_DELAY_H
#define STC_CORE_UTIL_DELAY_H

#include <Arduino.h>

/* Source-compatible convenience spellings, NOT AVR cycle-counted loops.
 * Timer0 must have been initialized. Negative/NaN durations do not wait.
 * Chunking avoids truncating a duration to STC's 16-bit unsigned int. */
static inline void _delay_us(double microseconds)
{
    if (!(microseconds > 0.0)) return;
    while (microseconds > 60000.0) {
        delayMicroseconds(60000u);
        microseconds -= 60000.0;
    }
    if (microseconds > 0.0) {
        unsigned int whole = (unsigned int)microseconds;
        delayMicroseconds(whole + (microseconds > (double)whole ? 1u : 0u));
    }
}

static inline void _delay_ms(double milliseconds)
{
    _delay_us(milliseconds * 1000.0);
}

#endif
