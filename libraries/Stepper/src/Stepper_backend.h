/* SPDX-License-Identifier: MIT */
/* Internal C hardware backend for the Arduino C++ library. */
#ifndef STC_STEPPER_BACKEND_H
#define STC_STEPPER_BACKEND_H

#include <Arduino.h>

typedef struct {
    uint8_t pins[5];
    uint8_t pin_count, phase;
    unsigned int steps_per_revolution;
    unsigned long step_delay_us, last_step_time;
} STCStepperState;

#ifdef __cplusplus
extern "C" {
#endif

/* SDCC indirect calls cannot use callee-specific overlay parameter slots. */
#if defined(__SDCC)
# define STC_STEPPER_REENTRANT __reentrant
# define STC_STEPPER_CODE __code
#else
# define STC_STEPPER_REENTRANT
# define STC_STEPPER_CODE
#endif

#define STEPPER_STATUS_SUCCESS          0u
#define STEPPER_STATUS_INVALID_STEPS    1u
#define STEPPER_STATUS_INVALID_PIN      2u
#define STEPPER_STATUS_PIN_CONFLICT     3u
#define STEPPER_STATUS_NOT_CONFIGURED   4u
#define STEPPER_STATUS_INVALID_SPEED    5u

#define STEPPER_PIN_COUNT_NONE 0u
#define STEPPER_PIN_COUNT_2    2u
#define STEPPER_PIN_COUNT_4    4u
#define STEPPER_PIN_COUNT_5    5u

STCStepperState *Stepper_selectContext(STCStepperState *context)
    STC_STEPPER_REENTRANT;

/*
 * steps_per_revolution must be in the range 1..65535.  Every pin must exist
 * on the selected variant, and no two pins may name the same physical pad.
 * A failed call leaves the previous configuration unchanged.
 */
uint8_t Stepper_setPins2(unsigned long steps_per_revolution,
                         uint8_t pin1, uint8_t pin2)
                         STC_STEPPER_REENTRANT;
uint8_t Stepper_setPins4(unsigned long steps_per_revolution,
                         uint8_t pin1, uint8_t pin2, uint8_t pin3,
                         uint8_t pin4) STC_STEPPER_REENTRANT;
uint8_t Stepper_setPins5(unsigned long steps_per_revolution,
                         uint8_t pin1, uint8_t pin2, uint8_t pin3,
                         uint8_t pin4, uint8_t pin5)
                         STC_STEPPER_REENTRANT;

/*
 * rpm must be positive.  Zero or a negative value disables motion; delays
 * shorter than one microsecond are clamped to one microsecond.
 */
uint8_t Stepper_setSpeed(long rpm) STC_STEPPER_REENTRANT;

/* Positive and negative values select opposite directions. */
void Stepper_step(long steps_to_move) STC_STEPPER_REENTRANT;

/* De-energizes every configured output without discarding the configuration. */
void Stepper_release(void) STC_STEPPER_REENTRANT;


#ifdef __cplusplus
}
#endif

#endif
