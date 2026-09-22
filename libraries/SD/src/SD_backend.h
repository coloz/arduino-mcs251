/* SPDX-License-Identifier: MIT */
/* Internal C hardware backend for the Arduino C++ library. */
#ifndef STC_SD_BACKEND_H
#define STC_SD_BACKEND_H

#include <Arduino.h>
#include <SPI_backend.h>
#include "SD_file.h"

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(STC_XDATA_BYTES) || (STC_XDATA_BYTES < 1024UL)
# error "The STC SD library requires at least 1024 bytes of XDATA"
#endif

/* Budgeted hot-state placement on measured 64 KiB+ targets. Override with a
 * platform-wide -DSTC_SD_STATE_IN_DATA=0 when application DATA is scarce;
 * a sketch-only definition does not configure this C translation unit. */
#ifndef STC_SD_STATE_IN_DATA
# if defined(STC32G12K128) || defined(STC32G144K246) || defined(AI8051U_34K64) || \
     defined(STC32G12K64) || defined(STC32G8K64) || defined(STC32CL8K64)
#  define STC_SD_STATE_IN_DATA 1
# else
#  define STC_SD_STATE_IN_DATA 0
# endif
#endif
#if (STC_SD_STATE_IN_DATA != 0) && (STC_SD_STATE_IN_DATA != 1)
# error "STC_SD_STATE_IN_DATA must be 0 or 1"
#endif

#if defined(__SDCC)
# define STC_SD_REENTRANT __reentrant
#else
# define STC_SD_REENTRANT
#endif

#ifndef FILE_READ
# define FILE_READ 0x01u
#endif
#ifndef FILE_WRITE
# define FILE_WRITE 0x17u
#endif

#ifndef SD_DEFAULT_MOSI_PIN
# define SD_DEFAULT_MOSI_PIN SPI_DEFAULT_MOSI_PIN
#endif
#ifndef SD_DEFAULT_MISO_PIN
# define SD_DEFAULT_MISO_PIN SPI_DEFAULT_MISO_PIN
#endif
#ifndef SD_DEFAULT_SCK_PIN
# define SD_DEFAULT_SCK_PIN SPI_DEFAULT_SCK_PIN
#endif
#ifndef SD_DEFAULT_CS_PIN
# define SD_DEFAULT_CS_PIN SPI_DEFAULT_SS_PIN
#endif

#define SD_CARD_NONE 0u
#define SD_CARD_SD1  1u
#define SD_CARD_SD2  2u
#define SD_CARD_SDHC 3u

#define SD_FAT_NONE 0u
#define SD_FAT16    16u
#define SD_FAT32    32u

/* Sticky result from the most recent operation. */
#define SD_ERROR_NONE               0u
#define SD_ERROR_INVALID_PIN        1u
#define SD_ERROR_PIN_CONFLICT       2u
#define SD_ERROR_CARD_TIMEOUT       3u
#define SD_ERROR_CARD_RESPONSE      4u
#define SD_ERROR_CARD_UNSUPPORTED   5u
#define SD_ERROR_ADDRESS_OVERFLOW   6u
#define SD_ERROR_READ_TOKEN         7u
#define SD_ERROR_WRITE_REJECTED     8u
#define SD_ERROR_WRITE_STATUS       9u
#define SD_ERROR_INVALID_ARGUMENT  10u
#define SD_ERROR_NOT_INITIALIZED   11u
#define SD_ERROR_BAD_VOLUME        12u
#define SD_ERROR_FAT12_UNSUPPORTED 13u
#define SD_ERROR_NOT_FOUND         14u
#define SD_ERROR_NOT_A_FILE        15u
#define SD_ERROR_READ_ONLY         16u
#define SD_ERROR_BAD_CLUSTER       17u
#define SD_ERROR_NO_SPACE          18u
#define SD_ERROR_DIRECTORY_FULL    19u
#define SD_ERROR_UNSUPPORTED       20u

/*
 * Select all four software-SPI pins. A failed call preserves the current
 * configuration. No two logical names may resolve to the same package pad.
 */
uint8_t SD_setPins(uint8_t mosi_pin, uint8_t miso_pin, uint8_t sck_pin,
                   uint8_t cs_pin) STC_SD_REENTRANT;

/* begin() retains the configured MOSI/MISO/SCK pins and selects cs_pin. */
uint8_t SD_begin(uint8_t cs_pin) STC_SD_REENTRANT;
uint8_t SD_beginClock(unsigned long clock_hz, uint8_t cs_pin) STC_SD_REENTRANT;

/* Uses the current pin tuple, which initially contains the SPI defaults. */
uint8_t SD_beginDefault(void) STC_SD_REENTRANT;
void SD_end(void) STC_SD_REENTRANT;

uint8_t SD_cardType(void) STC_SD_REENTRANT;
uint8_t SD_fatType(void) STC_SD_REENTRANT;
uint8_t SD_error(void) STC_SD_REENTRANT;

/* Raw 512-byte sectors. writeBlock() can corrupt a mounted filesystem. */
uint8_t SD_readBlock(unsigned long sector, uint8_t *buffer)
                     STC_SD_REENTRANT;
uint8_t SD_writeBlock(unsigned long sector, const uint8_t *buffer)
                      STC_SD_REENTRANT;

/* FAT16/FAT32 paths containing ASCII short 8.3 components. */
uint8_t SD_exists(const char *name) STC_SD_REENTRANT;
uint8_t SD_open(const char *name, uint8_t mode) STC_SD_REENTRANT;
size_t SD_write(uint8_t value) STC_SD_REENTRANT;
size_t SD_writeBytes(const uint8_t *buffer, size_t length)
                     STC_SD_REENTRANT;
int SD_read(void) STC_SD_REENTRANT;
size_t SD_readBytes(uint8_t *buffer, size_t length) STC_SD_REENTRANT;
int SD_peek(void) STC_SD_REENTRANT;
unsigned long SD_available(void) STC_SD_REENTRANT;
uint8_t SD_seek(unsigned long position) STC_SD_REENTRANT;
unsigned long SD_position(void) STC_SD_REENTRANT;
unsigned long SD_size(void) STC_SD_REENTRANT;
uint8_t SD_flush(void) STC_SD_REENTRANT;
void SD_close(void) STC_SD_REENTRANT;
uint8_t SD_remove(const char *name) STC_SD_REENTRANT;
uint8_t SD_mkdir(const char *name) STC_SD_REENTRANT;
uint8_t SD_rmdir(const char *name) STC_SD_REENTRANT;

#if defined(STC_SD_HOST_TEST) && STC_SD_HOST_TEST
/* Test-only mount entry point. Sector I/O is supplied by the host fixture. */
uint8_t SD_testMount(void) STC_SD_REENTRANT;
void SD_testReset(void) STC_SD_REENTRANT;
#endif

#ifdef __cplusplus
}
#endif

#endif
