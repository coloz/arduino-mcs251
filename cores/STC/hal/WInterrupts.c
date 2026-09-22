#include "Arduino.h"
#include "stc_sfr.h"

#include "WInterrupts_private.h"

static uint8_t stc_interrupt_lock(void)
{
    uint8_t enabled = (uint8_t)(IE & STC_IE_EA);
    IE &= (uint8_t)~STC_IE_EA;
    return enabled;
}

static void stc_interrupt_unlock(uint8_t enabled)
{
    if (enabled != 0u) {
        IE |= STC_IE_EA;
    }
}

static uint8_t stc_interrupt_error;

uint8_t interruptModeSupported(uint8_t interrupt_number, int mode) STC_REENTRANT
{
    if (interrupt_number > 1u ||
        !digitalPinIsValid(interrupt_number == 0u ? P3_2 : P3_3)) {
        return 0u;
    }
    if (mode == FALLING) { return 1u; }
#if STC_CORE_INT01_MODE == 2
    return (mode == CHANGE || mode == RISING) ? 1u : 0u;
#elif STC_CORE_INT01_MODE == 1
    return mode == LOW ? 1u : 0u;
#else
    return 0u;
#endif
}

uint8_t interruptConfigurationError(void)
{
    return stc_interrupt_error;
}

void attachInterrupt(uint8_t interrupt_number, void (*callback)(void),
                     int mode) STC_REENTRANT
{
    (void)attachInterruptChecked(interrupt_number, callback, mode);
}

uint8_t attachInterruptChecked(uint8_t interrupt_number, void (*callback)(void),
                               int mode) STC_REENTRANT
{
    uint8_t interrupt_state;

    if ((interrupt_number > 1u) || (callback == 0) ||
        !digitalPinIsValid(interrupt_number == 0u ? P3_2 : P3_3)) {
        stc_interrupt_error = STC_INTERRUPT_INVALID;
        return stc_interrupt_error;
    }

    if (!interruptModeSupported(interrupt_number, mode)) {
        stc_interrupt_error = STC_INTERRUPT_UNSUPPORTED_MODE;
        return stc_interrupt_error;
    }

    interrupt_state = stc_interrupt_lock();
    stc_external_callbacks[interrupt_number] = callback;
    stc_external_modes[interrupt_number] = (uint8_t)mode;

    if (interrupt_number == 0u) {
        if (mode == FALLING) {
            TCON |= STC_TCON_IT0;
        } else {
            TCON &= (uint8_t)~STC_TCON_IT0;
        }
        TCON &= (uint8_t)~STC_TCON_IE0;
        IE |= STC_IE_EX0;
    } else {
        if (mode == FALLING) {
            TCON |= STC_TCON_IT1;
        } else {
            TCON &= (uint8_t)~STC_TCON_IT1;
        }
        TCON &= (uint8_t)~STC_TCON_IE1;
        IE |= STC_IE_EX1;
    }

    stc_interrupt_unlock(interrupt_state);
    stc_interrupt_error = STC_INTERRUPT_OK;
    return STC_INTERRUPT_OK;
}

void detachInterrupt(uint8_t interrupt_number)
{
    uint8_t interrupt_state;

    if (interrupt_number > 1u) {
        return;
    }

    interrupt_state = stc_interrupt_lock();
    if (interrupt_number == 0u) {
        IE &= (uint8_t)~STC_IE_EX0;
    } else {
        IE &= (uint8_t)~STC_IE_EX1;
    }
    stc_external_callbacks[interrupt_number] = 0;
    stc_interrupt_unlock(interrupt_state);
}
