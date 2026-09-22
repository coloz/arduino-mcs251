/* SPDX-License-Identifier: MIT */
/* Internal C hardware backend for the Arduino C++ library. */
#ifndef STC_WIRE_BACKEND_H
#define STC_WIRE_BACKEND_H

#include <Arduino.h>

/* Eight hot bytes; RX/TX buffers and slave callbacks stay in XDATA. */
#ifndef STC_WIRE_STATE_IN_DATA
# if defined(STC32G12K128) || defined(STC32G144K246) || defined(AI8051U_34K64) || \
     defined(STC32G12K64) || defined(STC32G8K64) || defined(STC32CL8K64)
#  define STC_WIRE_STATE_IN_DATA 1
# else
#  define STC_WIRE_STATE_IN_DATA 0
# endif
#endif
#if STC_WIRE_STATE_IN_DATA != 0 && STC_WIRE_STATE_IN_DATA != 1
# error "STC_WIRE_STATE_IN_DATA must be 0 or 1"
#endif
#if defined(__SDCC) && STC_WIRE_STATE_IN_DATA
# define STC_WIRE_HOT __data
#else
# define STC_WIRE_HOT
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* SDCC indirect calls cannot use callee-specific overlay parameter slots. */
#if defined(__SDCC)
# define STC_WIRE_REENTRANT __reentrant
#else
# define STC_WIRE_REENTRANT
#endif

#ifndef WIRE_BUFFER_LENGTH
# define WIRE_BUFFER_LENGTH 32u
#endif
#if (WIRE_BUFFER_LENGTH < 1u) || (WIRE_BUFFER_LENGTH > 255u)
# error "WIRE_BUFFER_LENGTH must be between 1 and 255 bytes"
#endif
#ifndef BUFFER_LENGTH
# define BUFFER_LENGTH WIRE_BUFFER_LENGTH
#endif
#ifndef WIRE_HAS_TIMEOUT
# define WIRE_HAS_TIMEOUT 1
#endif
#ifndef WIRE_HAS_SLAVE
# define WIRE_HAS_SLAVE 1
#endif

#ifndef WIRE_DEFAULT_CLOCK_HZ
# define WIRE_DEFAULT_CLOCK_HZ 100000UL
#endif

#ifndef WIRE_DEFAULT_SDA_PIN
# define WIRE_DEFAULT_SDA_PIN PIN_WIRE_SDA
#endif

#ifndef WIRE_DEFAULT_SCL_PIN
# define WIRE_DEFAULT_SCL_PIN PIN_WIRE_SCL
#endif

#ifndef WIRE_DEFAULT_STRETCH_TIMEOUT_US
# define WIRE_DEFAULT_STRETCH_TIMEOUT_US 25000UL
#endif

#define WIRE_STATUS_SUCCESS        0u
#define WIRE_STATUS_DATA_TOO_LONG  1u
#define WIRE_STATUS_ADDRESS_NACK   2u
#define WIRE_STATUS_DATA_NACK      3u
#define WIRE_STATUS_OTHER_ERROR    4u
#define WIRE_STATUS_TIMEOUT        5u

void Wire_begin(void) STC_WIRE_REENTRANT;
void Wire_beginSlave(uint8_t address) STC_WIRE_REENTRANT;
void Wire_onReceive(void (*callback)(int) STC_WIRE_REENTRANT) STC_WIRE_REENTRANT;
void Wire_onRequest(void (*callback)(void) STC_WIRE_REENTRANT) STC_WIRE_REENTRANT;
void Wire_end(void) STC_WIRE_REENTRANT;
void Wire_setPins(uint8_t sda_pin, uint8_t scl_pin) STC_WIRE_REENTRANT;
uint8_t Wire_setPinsChecked(uint8_t sda_pin, uint8_t scl_pin) STC_WIRE_REENTRANT;
uint8_t Wire_configurationError(void) STC_WIRE_REENTRANT;
uint8_t Wire_lastError(void) STC_WIRE_REENTRANT;
void Wire_setClock(unsigned long clock_hz) STC_WIRE_REENTRANT;
void Wire_setClockStretchTimeout(unsigned long timeout_us) STC_WIRE_REENTRANT;
void Wire_setWireTimeout(uint32_t timeout_us, uint8_t reset_with_timeout)
    STC_WIRE_REENTRANT;
uint8_t Wire_getWireTimeoutFlag(void) STC_WIRE_REENTRANT;
void Wire_clearWireTimeoutFlag(void) STC_WIRE_REENTRANT;
void Wire_beginTransmission(uint8_t address) STC_WIRE_REENTRANT;
size_t Wire_write(uint8_t value) STC_WIRE_REENTRANT;
uint8_t Wire_endTransmission(void) STC_WIRE_REENTRANT;
uint8_t Wire_endTransmissionStop(uint8_t send_stop) STC_WIRE_REENTRANT;
uint8_t Wire_requestFrom(uint8_t address, uint8_t quantity) STC_WIRE_REENTRANT;
uint8_t Wire_requestFromStop(uint8_t address, uint8_t quantity,
                             uint8_t send_stop) STC_WIRE_REENTRANT;
uint8_t Wire_requestFromInternal(uint8_t address, uint8_t quantity,
                                 uint32_t internal_address,
                                 uint8_t internal_address_size,
                                 uint8_t send_stop) STC_WIRE_REENTRANT;
int Wire_available(void) STC_WIRE_REENTRANT;
int Wire_peek(void) STC_WIRE_REENTRANT;
int Wire_read(void) STC_WIRE_REENTRANT;

#ifdef __cplusplus
}
#endif

#endif
