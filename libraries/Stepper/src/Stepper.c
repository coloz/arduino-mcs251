/*
 * SPDX-License-Identifier: MIT
 *
 * Blocking, software-timed stepper motor control for the STC C++ library.
 */
#include "Stepper_backend.h"

#define STEPPER_MICROSECONDS_PER_MINUTE 60000000UL
#define STEPPER_MAX_STEPS_PER_REVOLUTION 65535UL
#define STEPPER_WAIT_CHUNK_US 60000u

static STCStepperState *stepper_current;
#define stepper_pins (stepper_current->pins)
#define stepper_pin_count (stepper_current->pin_count)
#define stepper_phase (stepper_current->phase)
#define stepper_steps_per_revolution (stepper_current->steps_per_revolution)
#define stepper_step_delay_us (stepper_current->step_delay_us)
#define stepper_last_step_time (stepper_current->last_step_time)

STCStepperState *Stepper_selectContext(STCStepperState *context)
    STC_STEPPER_REENTRANT
{
    STCStepperState *previous = stepper_current;
    stepper_current = context;
    return previous;
}

static uint8_t stepper_pins_conflict(uint8_t left, uint8_t right)
{
    if (left == right) {
        return 1u;
    }
    return (digitalPinsSharePhysicalPad(left, right) != 0) ? 1u : 0u;
}

static uint8_t stepper_validate_configuration(
    unsigned long steps_per_revolution, uint8_t pin_count,
    uint8_t pin1, uint8_t pin2, uint8_t pin3, uint8_t pin4, uint8_t pin5)
    STC_STEPPER_REENTRANT
{
    uint8_t pins[5];
    uint8_t index;
    uint8_t other;

    if ((steps_per_revolution == 0UL) ||
        (steps_per_revolution > STEPPER_MAX_STEPS_PER_REVOLUTION)) {
        return STEPPER_STATUS_INVALID_STEPS;
    }

    pins[0] = pin1;
    pins[1] = pin2;
    pins[2] = pin3;
    pins[3] = pin4;
    pins[4] = pin5;
    /* Check every pin before conflicts, preserving error precedence. */
    for (index = 0u; index < pin_count; ++index) {
        if (digitalPinIsValid(pins[index]) == 0u) {
            return STEPPER_STATUS_INVALID_PIN;
        }
    }
    for (index = 1u; index < pin_count; ++index) {
        for (other = 0u; other < index; ++other) {
            if (stepper_pins_conflict(pins[index], pins[other]) != 0u) {
                return STEPPER_STATUS_PIN_CONFLICT;
            }
        }
    }

    return STEPPER_STATUS_SUCCESS;
}

static void stepper_detach_current_pins(void)
{
    STCStepperState *state = stepper_current;
    uint8_t index;

    if (state->pin_count == STEPPER_PIN_COUNT_NONE) {
        return;
    }
    Stepper_release();
    for (index = 0u; index < state->pin_count; ++index) {
        pinMode(state->pins[index], INPUT);
    }
}

static uint8_t stepper_configure(
    unsigned long steps_per_revolution, uint8_t pin_count,
    uint8_t pin1, uint8_t pin2, uint8_t pin3, uint8_t pin4, uint8_t pin5)
    STC_STEPPER_REENTRANT
{
    /* No yield/callback boundary occurs in configuration or GPIO writes. */
    STCStepperState *state = stepper_current;
    uint8_t index;
    uint8_t status;

    status = stepper_validate_configuration(steps_per_revolution, pin_count,
                                             pin1, pin2, pin3, pin4, pin5);
    if (status != STEPPER_STATUS_SUCCESS) {
        return status;
    }

    stepper_detach_current_pins();

    state->pins[0] = pin1;
    state->pins[1] = pin2;
    state->pins[2] = pin3;
    state->pins[3] = pin4;
    state->pins[4] = pin5;
    state->pin_count = pin_count;
    state->phase = 0u;
    state->steps_per_revolution = (unsigned int)steps_per_revolution;

    /* Motion stays disabled until the sketch explicitly selects a speed. */
    state->step_delay_us = 0UL;

    for (index = 0u; index < state->pin_count; ++index) {
        /* Preload LOW through the released/open-drain state before enabling
         * push-pull output, so a reset-high latch cannot create a drive pulse. */
        pinMode(state->pins[index], OUTPUT_OPEN_DRAIN);
        digitalWrite(state->pins[index], LOW);
        pinMode(state->pins[index], OUTPUT);
    }
    state->last_step_time = micros();
    return STEPPER_STATUS_SUCCESS;
}

static uint8_t stepper_phase_count(void)
{
    return (stepper_pin_count == STEPPER_PIN_COUNT_5) ? 10u : 4u;
}

static uint8_t stepper_phase_mask(uint8_t phase)
{
    static STC_STEPPER_CODE const uint8_t phases2[4] = {
        0x02u, 0x03u, 0x01u, 0x00u
    };
    static STC_STEPPER_CODE const uint8_t phases4[4] = {
        0x05u, 0x06u, 0x0au, 0x09u
    };
    /* Ten half-phases for a five-phase motor, one bit per output. */
    static STC_STEPPER_CODE const uint8_t phases5[10] = {
        0x16u, 0x12u, 0x1au, 0x0au, 0x0bu,
        0x09u, 0x0du, 0x05u, 0x15u, 0x14u
    };
    if (stepper_pin_count == STEPPER_PIN_COUNT_2) {
        return phases2[phase < 3u ? phase : 3u];
    }

    if (stepper_pin_count == STEPPER_PIN_COUNT_4) {
        return phases4[phase < 3u ? phase : 3u];
    }

    return phases5[phase < 9u ? phase : 9u];
}

static void stepper_write_phase(uint8_t phase)
{
    STCStepperState *state = stepper_current;
    uint8_t index;
    uint8_t mask = stepper_phase_mask(phase);

    for (index = 0u; index < state->pin_count; ++index) {
        digitalWrite(state->pins[index],
                     ((mask & (uint8_t)(1u << index)) != 0u) ? HIGH : LOW);
    }
}

static void stepper_wait_for_next_step(void)
{
    unsigned long now;
    unsigned long elapsed;
    unsigned long remaining;
    unsigned int chunk;

    for (;;) {
        now = micros();
        elapsed = (unsigned long)(now - stepper_last_step_time);
        if (elapsed >= stepper_step_delay_us) {
            break;
        }

        remaining = stepper_step_delay_us - elapsed;
        chunk = (remaining > (unsigned long)STEPPER_WAIT_CHUNK_US) ?
            STEPPER_WAIT_CHUNK_US : (unsigned int)remaining;
        delayMicroseconds(chunk);
        yield();
    }
    stepper_last_step_time = now;
}

uint8_t Stepper_setPins2(unsigned long steps_per_revolution,
                         uint8_t pin1, uint8_t pin2)
                         STC_STEPPER_REENTRANT
{
    return stepper_configure(steps_per_revolution, STEPPER_PIN_COUNT_2,
                             pin1, pin2, NOT_A_PIN, NOT_A_PIN, NOT_A_PIN);
}

uint8_t Stepper_setPins4(unsigned long steps_per_revolution,
                         uint8_t pin1, uint8_t pin2, uint8_t pin3,
                         uint8_t pin4) STC_STEPPER_REENTRANT
{
    return stepper_configure(steps_per_revolution, STEPPER_PIN_COUNT_4,
                             pin1, pin2, pin3, pin4, NOT_A_PIN);
}

uint8_t Stepper_setPins5(unsigned long steps_per_revolution,
                         uint8_t pin1, uint8_t pin2, uint8_t pin3,
                         uint8_t pin4, uint8_t pin5)
                         STC_STEPPER_REENTRANT
{
    return stepper_configure(steps_per_revolution, STEPPER_PIN_COUNT_5,
                             pin1, pin2, pin3, pin4, pin5);
}

uint8_t Stepper_setSpeed(long rpm) STC_STEPPER_REENTRANT
{
    STCStepperState *state = stepper_current;
    unsigned long delay_us;

    if (rpm <= 0L) {
        state->step_delay_us = 0UL;
        return STEPPER_STATUS_INVALID_SPEED;
    }
    if (state->pin_count == STEPPER_PIN_COUNT_NONE) {
        return STEPPER_STATUS_NOT_CONFIGURED;
    }

    /* Divide in two stages so steps * rpm can never overflow. */
    delay_us = STEPPER_MICROSECONDS_PER_MINUTE /
        (unsigned long)state->steps_per_revolution;
    delay_us /= (unsigned long)rpm;
    state->step_delay_us = (delay_us == 0UL) ? 1UL : delay_us;
    state->last_step_time = micros();
    return STEPPER_STATUS_SUCCESS;
}

void Stepper_step(long steps_to_move) STC_STEPPER_REENTRANT
{
    unsigned long remaining_steps;
    uint8_t phase_count;
    uint8_t reverse;

    if ((stepper_pin_count == STEPPER_PIN_COUNT_NONE) ||
        (stepper_step_delay_us == 0UL) ||
        (steps_to_move == 0L)) {
        return;
    }

    if (steps_to_move < 0L) {
        /* This form is defined even for the most-negative signed long. */
        remaining_steps = (unsigned long)(-(steps_to_move + 1L)) + 1UL;
        reverse = 1u;
    } else {
        remaining_steps = (unsigned long)steps_to_move;
        reverse = 0u;
    }
    phase_count = stepper_phase_count();

    while (remaining_steps != 0UL) {
        stepper_wait_for_next_step();
        if (reverse == 0u) {
            ++stepper_phase;
            if (stepper_phase >= phase_count) {
                stepper_phase = 0u;
            }
        } else if (stepper_phase == 0u) {
            stepper_phase = (uint8_t)(phase_count - 1u);
        } else {
            --stepper_phase;
        }

        stepper_write_phase(stepper_phase);
        --remaining_steps;
        yield();
    }
}

void Stepper_release(void) STC_STEPPER_REENTRANT
{
    STCStepperState *state = stepper_current;
    uint8_t index;

    for (index = 0u; index < state->pin_count; ++index) {
        digitalWrite(state->pins[index], LOW);
    }
}
