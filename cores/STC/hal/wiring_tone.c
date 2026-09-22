/* SPDX-License-Identifier: MIT
 * One nonblocking tone on Timer2. Timer0 timekeeping and UART1/Timer1 continue.
 * Timer2 runs at F_CPU/12; no interrupt is generated before all state is ready.
 */
#include "Arduino.h"
#include "stc_peripheral_irqs.h"
#ifndef STC_TONE_HOST_TEST
#include "stc_sfr.h"
#endif
static uint8_t tone_pin, tone_active, tone_level, tone_error;
static uint8_t tone_saved_auxr, tone_saved_high, tone_saved_low;
static uint8_t tone_saved_output;
#if defined(STC_CORE_FAMILY_AI8051U) || STC_CORE_MEMORY_TIMING_LAYOUT == 2
static uint8_t tone_saved_prescaler;
#define STC_TONE_TM2PS STC_XFR8(0x7efea2UL)
static uint8_t tone_prescaler(uint8_t value)
{
    uint8_t saved = P_SW2, previous;
    P_SW2 |= 0x80u;
    previous = STC_TONE_TM2PS; STC_TONE_TM2PS = value;
    P_SW2 = saved;
    return previous;
}
#endif
static unsigned long tone_started, tone_duration;

static void tone_stop(void) STC_REENTRANT
{
    IE2 &= (uint8_t)~4u;
    AUXR &= (uint8_t)~0x10u;
    stc_tone_service = 0;
    AUXINTIF &= (uint8_t)~1u;
    T2H = tone_saved_high; T2L = tone_saved_low;
    INTCLKO = (INTCLKO & (uint8_t)~4u) | tone_saved_output;
#if defined(STC_CORE_FAMILY_AI8051U) || STC_CORE_MEMORY_TIMING_LAYOUT == 2
    (void)tone_prescaler(tone_saved_prescaler);
#endif
    AUXR = (AUXR & (uint8_t)~0x1cu) | tone_saved_auxr;
    digitalWrite(tone_pin, LOW);
    tone_active = 0u;
}
static void tone_tick(void) STC_REENTRANT
{
    if (tone_duration && (unsigned long)(millis() - tone_started) >= tone_duration) {
        tone_stop();
    } else {
        tone_level ^= 1u;
        digitalWrite(tone_pin, tone_level);
    }
}
uint8_t toneConfigurationError(void) { return tone_error; }
uint8_t toneChecked(uint8_t pin, unsigned int frequency, unsigned long duration) STC_REENTRANT
{
    uint8_t enabled;
    unsigned long ticks;
    unsigned int reload;
    if (!digitalPinIsValid(pin)) return tone_error = STC_TONE_INVALID_PIN;
    if (!frequency) { noTone(pin); return tone_error = STC_TONE_OK; }
    if (frequency < STC_TONE_MIN_FREQUENCY || frequency > STC_TONE_MAX_FREQUENCY)
        return tone_error = STC_TONE_INVALID_FREQUENCY;
    enabled = IE & 0x80u; IE &= (uint8_t)~0x80u;
    if ((tone_active && tone_pin != pin) ||
        (!tone_active && ((AUXR & 0x10u) || (IE2 & 4u) || (AUXR & 1u)))) {
        IE |= enabled; return tone_error = STC_TONE_TIMER_BUSY;
    }
    if (!tone_active) {
        tone_saved_auxr = AUXR & 0x1cu;
        tone_saved_high = T2H; tone_saved_low = T2L;
        tone_saved_output = INTCLKO & 4u;
#if defined(STC_CORE_FAMILY_AI8051U) || STC_CORE_MEMORY_TIMING_LAYOUT == 2
        tone_saved_prescaler = tone_prescaler(0u);
#endif
    }
    AUXR &= (uint8_t)~0x1cu;
    IE2 &= (uint8_t)~4u;
    INTCLKO &= (uint8_t)~4u;
    ticks = (F_CPU / 12UL + (unsigned long)frequency) / (2UL * frequency);
    reload = (unsigned int)(65536UL - ticks);
    pinMode(pin, OUTPUT); digitalWrite(pin, LOW);
    tone_pin = pin; tone_level = 0u; tone_active = 1u;
    tone_started = millis(); tone_duration = duration;
    T2H = (uint8_t)(reload >> 8); T2L = (uint8_t)reload;
    AUXINTIF &= (uint8_t)~1u;
    stc_tone_service = tone_tick;
    IE2 |= 4u; AUXR |= 0x10u;
    IE |= enabled;
    return tone_error = STC_TONE_OK;
}
void tone(uint8_t pin, unsigned int frequency, unsigned long duration) STC_REENTRANT
{
    (void)toneChecked(pin, frequency, duration);
}
void noTone(uint8_t pin) STC_REENTRANT
{
    uint8_t enabled = IE & 0x80u;
    IE &= (uint8_t)~0x80u;
    if (tone_active && tone_pin == pin) tone_stop();
    IE |= enabled;
}
