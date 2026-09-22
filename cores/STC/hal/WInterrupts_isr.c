#include "Arduino.h"
#include "stc_isr_context.h"
#include "stc_sfr.h"

#include "WInterrupts_private.h"

/*
 * Keep the ISR bodies and their callback state in a dedicated archive member.
 * main.c always references the available interrupt vectors, but sketches that
 * do not use attachInterrupt() must not also pull the configuration API.
 */
void (* volatile stc_external_callbacks[2])(void);
volatile uint8_t stc_external_modes[2];

#if STC_CORE_HAS_INT0
void stc_external0_isr(void) __interrupt (0)
{
    void (*callback)(void);

    STC_ISR_CONTEXT_ENTER();
    callback = stc_external_callbacks[0];
    if (callback != 0
#if STC_CORE_INT01_MODE == 2
        && (stc_external_modes[0] != RISING || (P3 & 0x04u) != 0u)
#endif
    ) {
        callback();
    }
    STC_ISR_CONTEXT_LEAVE();
}
#endif

#if STC_CORE_HAS_INT1
void stc_external1_isr(void) __interrupt (2)
{
    void (*callback)(void);

    STC_ISR_CONTEXT_ENTER();
    callback = stc_external_callbacks[1];
    if (callback != 0
#if STC_CORE_INT01_MODE == 2
        && (stc_external_modes[1] != RISING || (P3 & 0x08u) != 0u)
#endif
    ) {
        callback();
    }
    STC_ISR_CONTEXT_LEAVE();
}
#endif
