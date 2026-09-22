/* Minimal host adapter; production variant headers and C drivers are used. */
#ifndef TEST_ARDUINO_H
#define TEST_ARDUINO_H
#define STC_CORE_ARDUINO_H
#define __xdata
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#define STC_REENTRANT
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define OUTPUT_OPEN_DRAIN 3
#define HIGH 1
#define LOW 0
#include "pins_arduino.h"
void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t value);
int digitalRead(uint8_t pin);
int digitalPinIsValid(uint8_t pin);
void delayMicroseconds(unsigned int delay);
unsigned long micros(void);
#endif
