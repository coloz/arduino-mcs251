#include "Arduino.h"

uint8_t shiftIn(uint8_t data_pin, uint8_t clock_pin,
                uint8_t bit_order) STC_REENTRANT
{
    uint8_t bit_index;
    uint8_t value = 0u;
    uint8_t mask = bit_order == LSBFIRST ? 1u : 0x80u;

    for (bit_index = 0u; bit_index < 8u; ++bit_index) {
        digitalWrite(clock_pin, HIGH);
        if (digitalRead(data_pin) != LOW) {
            value |= mask;
        }
        digitalWrite(clock_pin, LOW);
        mask = bit_order == LSBFIRST ? (uint8_t)(mask << 1) : (uint8_t)(mask >> 1);
    }

    return value;
}

void shiftOut(uint8_t data_pin, uint8_t clock_pin, uint8_t bit_order,
              uint8_t value) STC_REENTRANT
{
    uint8_t bit_index;
    uint8_t mask = bit_order == LSBFIRST ? 1u : 0x80u;

    for (bit_index = 0u; bit_index < 8u; ++bit_index) {
        digitalWrite(data_pin,
                     (value & mask) != 0u ? HIGH : LOW);
        digitalWrite(clock_pin, HIGH);
        digitalWrite(clock_pin, LOW);
        mask = bit_order == LSBFIRST ? (uint8_t)(mask << 1) : (uint8_t)(mask >> 1);
    }
}
