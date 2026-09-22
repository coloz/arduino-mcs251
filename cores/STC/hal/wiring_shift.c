#include "Arduino.h"

uint8_t shiftIn(uint8_t data_pin, uint8_t clock_pin,
                uint8_t bit_order) STC_REENTRANT
{
    uint8_t bit_index;
    uint8_t value = 0u;

    for (bit_index = 0u; bit_index < 8u; ++bit_index) {
        digitalWrite(clock_pin, HIGH);
        if (digitalRead(data_pin) != LOW) {
            if (bit_order == LSBFIRST) {
                value |= (uint8_t)(1u << bit_index);
            } else {
                value |= (uint8_t)(1u << (7u - bit_index));
            }
        }
        digitalWrite(clock_pin, LOW);
    }

    return value;
}

void shiftOut(uint8_t data_pin, uint8_t clock_pin, uint8_t bit_order,
              uint8_t value) STC_REENTRANT
{
    uint8_t bit_index;

    for (bit_index = 0u; bit_index < 8u; ++bit_index) {
        uint8_t selected_bit = (bit_order == LSBFIRST)
            ? bit_index : (uint8_t)(7u - bit_index);
        digitalWrite(data_pin,
                     ((value & (uint8_t)(1u << selected_bit)) != 0u)
                         ? HIGH : LOW);
        digitalWrite(clock_pin, HIGH);
        digitalWrite(clock_pin, LOW);
    }
}
