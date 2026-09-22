/* SPDX-License-Identifier: MIT */
/* Internal C hardware backend for the Arduino C++ library. */
#ifndef STC_SPI_BACKEND_H
#define STC_SPI_BACKEND_H

#include <Arduino.h>

/* Eight bytes of frequently accessed bus state. Buffers and less frequent
 * transaction bookkeeping retain the default memory model. */
#ifndef STC_SPI_STATE_IN_DATA
# if defined(STC32G12K128) || defined(STC32G144K246) || defined(AI8051U_34K64) || \
     defined(STC32G12K64) || defined(STC32G8K64) || defined(STC32CL8K64)
#  define STC_SPI_STATE_IN_DATA 1
# else
#  define STC_SPI_STATE_IN_DATA 0
# endif
#endif
#if STC_SPI_STATE_IN_DATA != 0 && STC_SPI_STATE_IN_DATA != 1
# error "STC_SPI_STATE_IN_DATA must be 0 or 1"
#endif
#if defined(__SDCC) && STC_SPI_STATE_IN_DATA
# define STC_SPI_HOT __data
#else
# define STC_SPI_HOT
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* SDCC indirect calls cannot use callee-specific overlay parameter slots. */
#if defined(__SDCC)
# define STC_SPI_REENTRANT __reentrant
#else
# define STC_SPI_REENTRANT
#endif

#ifndef SPI_MODE0
# define SPI_MODE0 0x00u
#endif
#ifndef SPI_MODE1
# define SPI_MODE1 0x01u
#endif
#ifndef SPI_MODE2
# define SPI_MODE2 0x02u
#endif
#ifndef SPI_MODE3
# define SPI_MODE3 0x03u
#endif

#ifndef SPI_DEFAULT_CLOCK_HZ
# define SPI_DEFAULT_CLOCK_HZ 100000UL
#endif

#ifndef SPI_DEFAULT_MOSI_PIN
# define SPI_DEFAULT_MOSI_PIN PIN_SPI_MOSI
#endif
#ifndef SPI_DEFAULT_MISO_PIN
# define SPI_DEFAULT_MISO_PIN PIN_SPI_MISO
#endif
#ifndef SPI_DEFAULT_SCK_PIN
# if defined(PIN_SPI_SCK)
#  define SPI_DEFAULT_SCK_PIN PIN_SPI_SCK
# elif defined(PIN_VALID_MASK_P3) && ((PIN_VALID_MASK_P3 & 0x30U) == 0x30U)
#  define SPI_DEFAULT_SCK_PIN P3_4
# else
/* STC8G1K08A exposes only P3.0--P3.3; P5.4 is its portable fallback. */
#  define SPI_DEFAULT_SCK_PIN P5_4
# endif
#endif
#ifndef SPI_DEFAULT_SS_PIN
# if defined(PIN_SPI_SS)
#  define SPI_DEFAULT_SS_PIN PIN_SPI_SS
# elif defined(PIN_VALID_MASK_P3) && ((PIN_VALID_MASK_P3 & 0x30U) == 0x30U)
#  define SPI_DEFAULT_SS_PIN P3_5
# else
/* Keep SS adjacent to the fallback clock pin on STC8G1K08A. */
#  define SPI_DEFAULT_SS_PIN P5_5
# endif
#endif

void SPI_begin(void) STC_SPI_REENTRANT;
#define STC_SPI_OK 0u
#define STC_SPI_INVALID 1u
#define STC_SPI_BUSY 2u
#define STC_SPI_TIMEOUT 3u
uint8_t SPI_configurationError(void) STC_SPI_REENTRANT;
uint8_t SPI_setPinsChecked(uint8_t mosi_pin, uint8_t miso_pin, uint8_t sck_pin,
                           uint8_t ss_pin) STC_SPI_REENTRANT;
uint8_t SPI_beginTransactionChecked(unsigned long clock_hz, uint8_t bit_order,
                                     uint8_t data_mode) STC_SPI_REENTRANT;
void SPI_setPins(uint8_t mosi_pin, uint8_t miso_pin, uint8_t sck_pin,
                 uint8_t ss_pin) STC_SPI_REENTRANT;
void SPI_beginTransaction(unsigned long clock_hz, uint8_t bit_order,
                           uint8_t data_mode) STC_SPI_REENTRANT;
void SPI_setSettings(unsigned long clock_hz, uint8_t bit_order,
                     uint8_t data_mode) STC_SPI_REENTRANT;
void SPI_usingInterrupt(uint8_t interrupt_number) STC_SPI_REENTRANT;
void SPI_notUsingInterrupt(uint8_t interrupt_number) STC_SPI_REENTRANT;
uint8_t SPI_transfer(uint8_t value) STC_SPI_REENTRANT;
void SPI_transferBuffer(uint8_t *buffer, size_t length) STC_SPI_REENTRANT;
void SPI_endTransaction(void) STC_SPI_REENTRANT;
void SPI_end(void) STC_SPI_REENTRANT;

#ifdef __cplusplus
}
#endif

#endif
