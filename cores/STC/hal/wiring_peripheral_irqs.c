#include "stc_peripheral_irqs.h"
#include "stc_isr_context.h"
#include "stc_sfr.h"

/* Each hardware vector owns its callback. Do not infer the source by reading
 * T2IF: some STC timer interrupt flags are write-only. Callback registration
 * changes are made while interrupts are off. */
STC_IRQ_DATA stc_peripheral_service_t stc_tone_service;
STC_IRQ_DATA stc_peripheral_service_t stc_wire_slave_service;
void stc_timer2_isr(void) __interrupt (12)
{
    STC_ISR_CONTEXT_ENTER();
    AUXINTIF &= (uint8_t)~1u;
    if (stc_tone_service) stc_tone_service();
    STC_ISR_CONTEXT_LEAVE();
}
void stc_i2c_isr(void) __interrupt (24)
{
    STC_ISR_CONTEXT_ENTER();
    if (stc_wire_slave_service) stc_wire_slave_service();
    STC_ISR_CONTEXT_LEAVE();
}
