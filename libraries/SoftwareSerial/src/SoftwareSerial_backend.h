/* SPDX-License-Identifier: MIT */
/* Internal C hardware backend for the Arduino C++ library. */
#ifndef STC_SOFTWARESERIAL_BACKEND_H
#define STC_SOFTWARESERIAL_BACKEND_H

#include <Arduino.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__SDCC)
# define STC_SOFTWARE_SERIAL_REENTRANT __reentrant
#else
# define STC_SOFTWARE_SERIAL_REENTRANT
#endif

#ifndef SOFTWARE_SERIAL_RX_BUFFER_SIZE
# if defined(STC_XDATA_BYTES) && STC_XDATA_BYTES >= 2048
#  define SOFTWARE_SERIAL_RX_BUFFER_SIZE 64u
# else
#  define SOFTWARE_SERIAL_RX_BUFFER_SIZE 16u
# endif
#endif

/* Callers can distinguish a polled port from an interrupt-backed UART. */
#define SOFTWARE_SERIAL_RX_BACKGROUND 0
#define SOFTWARE_SERIAL_RX_POLL_ON_READ 1

#if (SOFTWARE_SERIAL_RX_BUFFER_SIZE < 2) || \
    (SOFTWARE_SERIAL_RX_BUFFER_SIZE > 255)
# error "SOFTWARE_SERIAL_RX_BUFFER_SIZE must be between 2 and 255 bytes"
#endif

/*
 * These conservative request limits keep each bit shorter than one Timer0
 * millisecond and leave time for portable C GPIO on slower targets.  A custom
 * value must be supplied as a build flag so it also reaches SoftwareSerial.c;
 * changing it in only the sketch translation unit has no effect.  Every
 * clock/board/rate combination still needs measurement on real hardware.
 */
#ifndef SOFTWARE_SERIAL_MIN_BIT_PERIOD_US
# define SOFTWARE_SERIAL_MIN_BIT_PERIOD_US 104u
#endif
#ifndef SOFTWARE_SERIAL_MAX_BIT_PERIOD_US
# define SOFTWARE_SERIAL_MAX_BIT_PERIOD_US 834u
#endif

/* Validate the complete C++ object configuration before replacing the
 * current single active backend.  A rejected request leaves it untouched. */
bool SoftwareSerial_beginOnPins(uint8_t receive_pin, uint8_t transmit_pin,
                                bool inverse_logic, unsigned long baud)
                                STC_SOFTWARE_SERIAL_REENTRANT;
void SoftwareSerial_end(void);
bool SoftwareSerial_listen(void);
bool SoftwareSerial_stopListening(void);
bool SoftwareSerial_isListening(void);

/* Poll once for a start bit and, when found, synchronously sample one frame. */
size_t SoftwareSerial_poll(void);
/* available/peek/read service one arriving frame before inspecting the buffer.
 * Frequent calls are still necessary; this is not background reception. */
int SoftwareSerial_available(void);
int SoftwareSerial_availableForWrite(void);
int SoftwareSerial_peek(void);
int SoftwareSerial_read(void);
size_t SoftwareSerial_write(uint8_t value);
void SoftwareSerial_flush(void);

/* Sticky receive status; each query returns and clears its flag. */
bool SoftwareSerial_overflow(void);
bool SoftwareSerial_framingError(void);
bool SoftwareSerial_timingError(void);


#ifdef __cplusplus
}
#endif

#endif
