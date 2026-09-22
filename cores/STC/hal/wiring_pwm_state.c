#include "wiring_pwm_private.h"
#include "stc_sfr.h"
#if STC_CORE_PWM_LAYOUT
volatile uint8_t stc_pwm_owner[8];
volatile uint8_t stc_pwm_active;

void stc_pwm_detach(uint8_t pin) STC_REENTRANT
{
    uint8_t channel, enabled, window, owner = (uint8_t)(pin + 1u);
    uint8_t alias = (uint8_t)(STC_VARIANT_PHYSICAL_ALIAS(pin) + 1u);
    if (!stc_pwm_active) return;
    enabled = IE & STC_IE_EA;
    IE &= (uint8_t)~STC_IE_EA;
    window = P_SW2;
    P_SW2 |= STC_P_SW2_EAXFR;
    for (channel = 0; channel != 8u; ++channel) {
        if (stc_pwm_owner[channel] == owner ||
            (alias && stc_pwm_owner[channel] == alias)) {
            STC_PWM_REG(channel < 4u ? 0xb1u : 0xb5u) &=
                (uint8_t)~(1u << ((channel & 3u) * 2u));
            stc_pwm_owner[channel] = 0;
            stc_pwm_active &= (uint8_t)~(1u << channel);
        }
    }
    P_SW2 = window;
    if (enabled) IE |= STC_IE_EA;
}
#endif
