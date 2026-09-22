#include "Arduino.h"

unsigned long pulseInLong(uint8_t pin, uint8_t state,
                          unsigned long timeout) STC_REENTRANT
{
    unsigned long started;
    unsigned long pulse_started;

    if (digitalPinIsValid(pin) == 0u) {
        return 0UL;
    }

    started = micros();

    while ((uint8_t)digitalRead(pin) == state) {
        if ((micros() - started) >= timeout) {
            return 0UL;
        }
    }
    while ((uint8_t)digitalRead(pin) != state) {
        if ((micros() - started) >= timeout) {
            return 0UL;
        }
    }

    pulse_started = micros();
    while ((uint8_t)digitalRead(pin) == state) {
        if ((micros() - started) >= timeout) {
            return 0UL;
        }
    }

    return micros() - pulse_started;
}

unsigned long pulseIn(uint8_t pin, uint8_t state,
                      unsigned long timeout) STC_REENTRANT
{
    return pulseInLong(pin, state, timeout);
}
