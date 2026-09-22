#ifndef STC_CORE_ARDUINO_H
#define STC_CORE_ARDUINO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "binary.h"

/* Match the common Arduino.h convenience surface before Arduino's function
 * macros are introduced.  The freestanding target headers contain honest
 * declarations for the SDCC libc.  Host conformance skips stdlib.h here
 * because glibc's POSIX random(void) has C linkage and cannot coexist with
 * Arduino's random(long); host probes that need allocation include it before
 * Arduino.h under an isolated adapter name. */
#if defined(__STC_CLANG_IR_ONLY__) || defined(__SDCC)
# include <stdlib.h>
# include <string.h>
# include <math.h>
#else
/* string.h has no POSIX random collision and remains useful to native tests. */
# include <string.h>
#endif

#include "stc_family.h"

#ifndef F_CPU
# error "F_CPU must be provided by the selected board"
#endif

#define LOW  0x0u
#define HIGH 0x1u

/* Arduino modes plus explicit STC output modes. */
#define INPUT             0x0u
#define OUTPUT            0x1u
#define INPUT_PULLUP      0x2u
#define OUTPUT_OPEN_DRAIN 0x3u
#define OUTPUT_QUASI      0x4u
#define QUASI             OUTPUT_QUASI
#define OUTPUT_PP         OUTPUT
#define OUTPUT_OD         OUTPUT_OPEN_DRAIN

#define LSBFIRST 0u
#define MSBFIRST 1u

/* Portable SPI consumers use BitOrder on non-AVR architectures. The STC C
 * HAL deliberately keeps the same one-byte bit-order representation. */
typedef uint8_t BitOrder;

#define CHANGE  1u
#define FALLING 2u
#define RISING  3u

#define DEFAULT  1u
#define EXTERNAL 0u
#define INTERNAL 3u

#ifndef DEC
# define DEC 10u
#endif
#ifndef HEX
# define HEX 16u
#endif
#ifndef OCT
# define OCT 8u
#endif
#ifndef BIN
# define BIN 2u
#endif

#ifndef PI
# define PI 3.14159265358979323846
#endif
#ifndef HALF_PI
# define HALF_PI 1.57079632679489661923
#endif
#ifndef TWO_PI
# define TWO_PI 6.28318530717958647692
#endif
#ifndef DEG_TO_RAD
# define DEG_TO_RAD 0.01745329251994329577
#endif
#ifndef RAD_TO_DEG
# define RAD_TO_DEG 57.2957795130823208768
#endif
#ifndef EULER
# define EULER 2.71828182845904523536
#endif

#define STC_PIN(port, bit) ((((uint8_t)(port)) << 4) | ((uint8_t)(bit)))

typedef uint8_t byte;
typedef uint8_t pin_size_t;
typedef bool boolean;
typedef unsigned int word;
typedef void (*voidFuncPtr)(void);
typedef void (*voidFuncPtrParam)(void *);

/* Indirect calls and separately archived modules cannot safely share SDCC's
 * fixed overlay parameter slots. Public multi-argument APIs therefore use
 * the stack-based calling convention, which also preserves scarce DATA RAM. */
#if defined(__SDCC)
# define STC_REENTRANT __reentrant
#else
# define STC_REENTRANT
#endif

#define bit(bit_number) (1UL << (bit_number))
#define bitRead(value, bit) (((value) >> (bit)) & 0x01u)
#define bitSet(value, bit) ((value) |= (1UL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
#define bitToggle(value, bit) ((value) ^= (1UL << (bit)))
#define bitWrite(value, bit, state) ((state) ? bitSet((value), (bit)) : bitClear((value), (bit)))
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
#define abs(value) ((value) >= 0 ? (value) : -(value))
#define constrain(value, low, high) ((value) < (low) ? (low) : ((value) > (high) ? (high) : (value)))
#define round(value) ((value) >= 0 ? (long)((value) + 0.5) : (long)((value) - 0.5))
#define radians(degrees_value) ((degrees_value) * DEG_TO_RAD)
#define degrees(radians_value) ((radians_value) * RAD_TO_DEG)
#define sq(value) ((value) * (value))
#define lowByte(value) ((uint8_t)((value) & 0xffu))
#define highByte(value) ((uint8_t)(((value) >> 8) & 0xffu))

#define clockCyclesPerMicrosecond() (F_CPU / 1000000UL)
#define clockCyclesToMicroseconds(cycles) ((cycles) / clockCyclesPerMicrosecond())
#define microsecondsToClockCycles(us) ((us) * clockCyclesPerMicrosecond())

/* The active variant supplies physical pin availability and board aliases. */
#include "pins_arduino.h"

#ifndef NOT_A_PIN
# define NOT_A_PIN 0xffu
#endif
#ifndef NOT_AN_INTERRUPT
# define NOT_AN_INTERRUPT 0xffu
#endif
#ifndef digitalPinToPort
# define digitalPinToPort(pin) ((uint8_t)(pin) >> 4)
#endif
#ifndef digitalPinToBitMask
# define digitalPinToBitMask(pin) ((uint8_t)(1u << ((uint8_t)(pin) & 0x07u)))
#endif
#ifndef digitalPinToInterrupt
# define digitalPinToInterrupt(pin) \
    (((uint8_t)(pin) == (uint8_t)P3_2) ? 0u : \
     (((uint8_t)(pin) == (uint8_t)P3_3) ? 1u : NOT_AN_INTERRUPT))
#endif
#ifndef digitalPinHasPWM
# define digitalPinHasPWM(pin) (analogWriteSupported((uint8_t)(pin)) != 0u)
#endif

#ifdef __cplusplus
extern "C" {
#endif

void init(void);
void initVariant(void);
void yield(void);
void interrupts(void);
void noInterrupts(void);
void pinMode(uint8_t pin, uint8_t mode) STC_REENTRANT;
void digitalWrite(uint8_t pin, uint8_t value) STC_REENTRANT;
int digitalRead(uint8_t pin);
uint8_t digitalPinIsValid(uint8_t pin);
unsigned long millis(void);
unsigned long micros(void);
void delay(unsigned long milliseconds);
void delayMicroseconds(unsigned int microseconds);

int analogRead(uint8_t pin);
void analogReference(uint8_t mode);
void analogReadResolution(uint8_t bits);
void analogWrite(uint8_t pin, int value) STC_REENTRANT;
#define STC_PWM_OK 0u
#define STC_PWM_INVALID 1u
#define STC_PWM_UNSUPPORTED 2u
#define STC_PWM_BUSY 3u
uint8_t analogWriteSupported(uint8_t pin) STC_REENTRANT;
/* Checked requests never substitute a digital threshold for PWM. */
uint8_t analogWriteChecked(uint8_t pin, int value) STC_REENTRANT;
uint8_t analogWriteConfigurationError(void);

void attachInterrupt(uint8_t interrupt_number, void (*callback)(void),
                     int mode) STC_REENTRANT;
#define STC_INTERRUPT_OK 0u
#define STC_INTERRUPT_INVALID 1u
#define STC_INTERRUPT_UNSUPPORTED_MODE 2u
/* RISING on dual-edge INT0/1 is filtered by the pin level in the ISR.
 * Pulses must remain high until the ISR samples them. Use CHANGE/FALLING
 * for direct hardware edge selection.
 */
uint8_t interruptModeSupported(uint8_t interrupt_number, int mode) STC_REENTRANT;
uint8_t attachInterruptChecked(uint8_t interrupt_number, void (*callback)(void),
                               int mode) STC_REENTRANT;
uint8_t interruptConfigurationError(void);
void detachInterrupt(uint8_t interrupt_number);

uint8_t shiftIn(uint8_t data_pin, uint8_t clock_pin,
                uint8_t bit_order) STC_REENTRANT;
void shiftOut(uint8_t data_pin, uint8_t clock_pin, uint8_t bit_order,
              uint8_t value) STC_REENTRANT;
#ifdef __cplusplus
unsigned long pulseIn(uint8_t pin, uint8_t state,
                      unsigned long timeout = 1000000UL) STC_REENTRANT;
unsigned long pulseInLong(uint8_t pin, uint8_t state,
                          unsigned long timeout = 1000000UL) STC_REENTRANT;
#else
unsigned long pulseIn(uint8_t pin, uint8_t state,
                      unsigned long timeout) STC_REENTRANT;
unsigned long pulseInLong(uint8_t pin, uint8_t state,
                          unsigned long timeout) STC_REENTRANT;
#endif

void randomSeed(unsigned long seed);
#define STC_HAS_TONE 1
#define STC_TONE_MIN_FREQUENCY 31u
#define STC_TONE_MAX_FREQUENCY (F_CPU / 4096UL)
#define STC_TONE_OK 0u
#define STC_TONE_INVALID_PIN 1u
#define STC_TONE_INVALID_FREQUENCY 2u
#define STC_TONE_TIMER_BUSY 3u
#ifdef __cplusplus
void tone(uint8_t pin, unsigned int frequency, unsigned long duration = 0) STC_REENTRANT;
#else
void tone(uint8_t pin, unsigned int frequency, unsigned long duration) STC_REENTRANT;
#endif
void noTone(uint8_t pin) STC_REENTRANT;
uint8_t toneChecked(uint8_t pin, unsigned int frequency, unsigned long duration) STC_REENTRANT;
uint8_t toneConfigurationError(void);
long random(long upper_bound);
long random_minmax(long lower_bound, long upper_bound) STC_REENTRANT;
long map(long value, long from_low, long from_high,
         long to_low, long to_high) STC_REENTRANT;

void setup(void);
void loop(void);

#ifdef __cplusplus
} /* extern "C" */

# include "WCharacter.h"

/* Preserve the conventional Arduino C++ overloads while keeping the retained
 * implementation symbols in the stable C HAL. */
inline long random(long lower_bound, long upper_bound)
{
    return random_minmax(lower_bound, upper_bound);
}

inline word makeWord(word value)
{
    return value;
}

inline word makeWord(byte high, byte low)
{
    return (word)((((word)high) << 8) | (word)low);
}

# define word(...) makeWord(__VA_ARGS__)
#else
# define makeWord(high, low) \
    ((word)((((word)(uint8_t)(high)) << 8) | (word)(uint8_t)(low)))
#endif

/* C++ Arduino API and internal C hardware declarations. */
#if defined(__cplusplus)
#include "stcxx_config.h"
#include "pgmspace.h"
#include "WString.h"
#include "Printable.h"
#include "Print.h"
#include "Stream.h"
#include "HardwareSerial.h"
#include "USBCDC.h"
#include "IPAddress.h"
#include "Client.h"
#include "Server.h"
#include "Udp.h"

#define STCXX_CPP_CORE_AVAILABLE 1
#ifndef ARDUINO_API_VERSION
# define ARDUINO_API_VERSION 10502
#endif
#else
# include "HardwareSerial_backend.h"
#endif

#endif
