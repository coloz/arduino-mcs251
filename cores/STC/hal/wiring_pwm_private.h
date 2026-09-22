#ifndef STC_WIRING_PWM_PRIVATE_H
#define STC_WIRING_PWM_PRIVATE_H
#include "Arduino.h"
#if STC_CORE_PWM_LAYOUT
#  define STC_PWM_XFR_BASE 0x7efe00UL
# define STC_PWM_REG(offset) (*(volatile __xdata uint8_t *)(STC_PWM_XFR_BASE + (offset)))
/* Zero means unowned; the pin encoding plus one cannot wrap for valid pins. */
extern volatile uint8_t stc_pwm_owner[8];
extern volatile uint8_t stc_pwm_active;
void stc_pwm_detach(uint8_t pin) STC_REENTRANT;
#else
# define stc_pwm_detach(pin) ((void)0)
#endif
#endif
