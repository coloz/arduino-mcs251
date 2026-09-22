#include "Arduino.h"
#include "stc_sfr.h"

#ifndef STC_TIMER0_CLOCK_DIVIDER
# if STC_CORE_TIMER1_IS_1T
#  define STC_TIMER0_CLOCK_DIVIDER 1UL
# else
#  define STC_TIMER0_CLOCK_DIVIDER 12UL
# endif
#endif

#define STC_TIMER0_TICKS_PER_MS \
    ((F_CPU + (STC_TIMER0_CLOCK_DIVIDER * 500UL)) / \
     (STC_TIMER0_CLOCK_DIVIDER * 1000UL))
#define STC_TIMER0_RELOAD (65536UL - STC_TIMER0_TICKS_PER_MS)

extern volatile unsigned long stc_timer0_millis_count;

static uint16_t stc_timer0_read(void)
{
    uint8_t high_before;
    uint8_t high_after;
    uint8_t low;

    do {
        high_before = TH0;
        low = TL0;
        high_after = TH0;
    } while (high_before != high_after);

    return ((uint16_t)high_after << 8) | low;
}

unsigned long micros(void)
{
    uint8_t enabled = IE & STC_IE_EA;
    uint8_t overflow;
    uint16_t counter;
    uint32_t elapsed_ticks;
    unsigned long milliseconds;

    IE &= (uint8_t)~STC_IE_EA;
    milliseconds = stc_timer0_millis_count;
    counter = stc_timer0_read();
    overflow = TCON & STC_TCON_TF0;

    if (overflow != 0u) {
        /* Re-read after observing TF0.  The first sample may have straddled
         * the overflow and therefore still belong to the preceding tick. */
        counter = stc_timer0_read();
        ++milliseconds;
#if STC_CORE_HAS_TIMER01_16BIT_AUTO_RELOAD
        elapsed_ticks = (uint16_t)(counter - (uint16_t)STC_TIMER0_RELOAD);
#else
        /* A classic mode-1 counter wraps to zero until the ISR reloads it. */
        elapsed_ticks = counter;
#endif
    } else {
        elapsed_ticks = (uint16_t)(counter - (uint16_t)STC_TIMER0_RELOAD);
    }

    if (enabled != 0u) {
        IE |= STC_IE_EA;
    }

#if (STC_TIMER0_TICKS_PER_MS % 1000UL) == 0UL
    /* Whole timer ticks per microsecond: cancel the common factor before
     * generating code. This avoids a 32-bit multiply and division at the
     * supported integer-MHz clocks without changing truncation semantics. */
    return milliseconds * 1000UL +
        (uint16_t)elapsed_ticks / (uint16_t)(STC_TIMER0_TICKS_PER_MS / 1000UL);
#else
    return milliseconds * 1000UL +
        (elapsed_ticks * 1000UL) / STC_TIMER0_TICKS_PER_MS;
#endif
}

#if STC_CORE_HAS_TIMER01_16BIT_AUTO_RELOAD && \
    (STC_TIMER0_TICKS_PER_MS % 1000UL) == 0UL && STC_TIMER0_TICKS_PER_MS <= 65535UL
static void stc_delay_short_ticks(uint16_t remaining)
{
    uint16_t previous = stc_timer0_read();
    uint16_t current;
    uint16_t elapsed;
    while (remaining != 0u) {
        current = stc_timer0_read();
        elapsed = current >= previous ? current - previous :
            (uint16_t)(STC_TIMER0_TICKS_PER_MS - (uint16_t)(previous - current));
        if (elapsed >= remaining) break;
        remaining -= elapsed;
        previous = current;
    }
}
#endif

void delayMicroseconds(unsigned int microseconds)
{
#if STC_CORE_HAS_TIMER01_16BIT_AUTO_RELOAD
    uint32_t remaining;
    uint16_t previous;
    uint16_t current;
    uint16_t elapsed;

    if (microseconds == 0u) {
        return;
    }
#if (STC_TIMER0_TICKS_PER_MS % 1000UL) == 0UL && STC_TIMER0_TICKS_PER_MS <= 65535UL
    /* Up to one timer period fits in 16 bits. Keep the common short-delay
     * loop out of the 32-bit accounting path, with the same wrap semantics. */
    if (microseconds <= 1000u) {
        stc_delay_short_ticks(microseconds * (uint16_t)(STC_TIMER0_TICKS_PER_MS / 1000UL));
        return;
    }
#endif
    /* micros() can account for only one pending TF0. During a transaction
     * that masks interrupts, delays longer than one Timer0 period must not
     * depend on the ISR's millisecond count. Accumulate coherent counter
     * deltas instead. Timer0 must be running (as after init()). A preempting
     * ISR can lengthen this busy wait, but cannot make it return early.
     * Round up so fractional timer ticks do not shorten the requested wait.
     */
#if (STC_TIMER0_TICKS_PER_MS % 1000UL) == 0UL
    remaining = (uint32_t)microseconds * (STC_TIMER0_TICKS_PER_MS / 1000UL);
#else
    remaining = ((uint32_t)microseconds * STC_TIMER0_TICKS_PER_MS + 999UL) /
        1000UL;
#endif
    previous = stc_timer0_read();
    while (remaining != 0UL) {
        current = stc_timer0_read();
        if (current >= previous) {
            elapsed = current - previous;
        } else {
            elapsed = (uint16_t)(STC_TIMER0_TICKS_PER_MS -
                (uint16_t)(previous - current));
        }
        if ((uint32_t)elapsed >= remaining) {
            break;
        }
        remaining -= elapsed;
        previous = current;
    }
#else
    unsigned long start;

    if (microseconds == 0u) {
        return;
    }
    start = micros();
    while ((unsigned long)(micros() - start) < (unsigned long)microseconds) {
    }
#endif
}
