#ifndef STCXX_C_HAL_H
#define STCXX_C_HAL_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#if defined(__SDCC)
# define STCXX_HAL_REENTRANT __reentrant
#else
# define STCXX_HAL_REENTRANT
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Wiring services used by Stream. */
unsigned long millis(void);
void yield(void);

/* Stable C UART boundary implemented by cores/STC/HardwareSerial*.c. */
void Serial_begin(unsigned long baud);
bool Serial_active(void);
void Serial_end(void);
int Serial_available(void);
int Serial_availableForWrite(void);
int Serial_peek(void);
int Serial_read(void);
size_t Serial_write(uint8_t value);
void Serial_flush(void);
bool Serial_overflow(void);

/* Stable C SPI boundary implemented by libraries/SPI/src/SPI.c. */
void SPI_begin(void) STCXX_HAL_REENTRANT;
uint8_t SPI_configurationError(void) STCXX_HAL_REENTRANT;
uint8_t SPI_setPinsChecked(uint8_t mosi_pin, uint8_t miso_pin, uint8_t sck_pin,
                           uint8_t ss_pin) STCXX_HAL_REENTRANT;
uint8_t SPI_beginTransactionChecked(unsigned long clock_hz, uint8_t bit_order,
                                     uint8_t data_mode) STCXX_HAL_REENTRANT;
void SPI_setPins(uint8_t mosi_pin, uint8_t miso_pin, uint8_t sck_pin,
                 uint8_t ss_pin) STCXX_HAL_REENTRANT;
void SPI_beginTransaction(unsigned long clock_hz, uint8_t bit_order,
                           uint8_t data_mode) STCXX_HAL_REENTRANT;
void SPI_setSettings(unsigned long clock_hz, uint8_t bit_order,
                     uint8_t data_mode) STCXX_HAL_REENTRANT;
void SPI_usingInterrupt(uint8_t interrupt_number) STCXX_HAL_REENTRANT;
void SPI_notUsingInterrupt(uint8_t interrupt_number) STCXX_HAL_REENTRANT;
uint8_t SPI_transfer(uint8_t value) STCXX_HAL_REENTRANT;
void SPI_transferBuffer(uint8_t *buffer, size_t length) STCXX_HAL_REENTRANT;
void SPI_endTransaction(void) STCXX_HAL_REENTRANT;
void SPI_end(void) STCXX_HAL_REENTRANT;

/* Stable C I2C-master boundary implemented by libraries/Wire/src/Wire.c. */
void Wire_begin(void) STCXX_HAL_REENTRANT;
void Wire_beginSlave(uint8_t address) STCXX_HAL_REENTRANT;
void Wire_onReceive(void (*callback)(int) STCXX_HAL_REENTRANT) STCXX_HAL_REENTRANT;
void Wire_onRequest(void (*callback)(void) STCXX_HAL_REENTRANT) STCXX_HAL_REENTRANT;
void Wire_end(void) STCXX_HAL_REENTRANT;
void Wire_setPins(uint8_t sda_pin, uint8_t scl_pin) STCXX_HAL_REENTRANT;
uint8_t Wire_setPinsChecked(uint8_t sda_pin, uint8_t scl_pin) STCXX_HAL_REENTRANT;
uint8_t Wire_configurationError(void) STCXX_HAL_REENTRANT;
uint8_t Wire_lastError(void) STCXX_HAL_REENTRANT;
void Wire_setClock(unsigned long clock_hz) STCXX_HAL_REENTRANT;
void Wire_setClockStretchTimeout(unsigned long timeout_us)
    STCXX_HAL_REENTRANT;
void Wire_setWireTimeout(uint32_t timeout_us, uint8_t reset_with_timeout)
    STCXX_HAL_REENTRANT;
uint8_t Wire_getWireTimeoutFlag(void) STCXX_HAL_REENTRANT;
void Wire_clearWireTimeoutFlag(void) STCXX_HAL_REENTRANT;
void Wire_beginTransmission(uint8_t address) STCXX_HAL_REENTRANT;
size_t Wire_write(uint8_t value) STCXX_HAL_REENTRANT;
uint8_t Wire_endTransmission(void) STCXX_HAL_REENTRANT;
uint8_t Wire_endTransmissionStop(uint8_t send_stop) STCXX_HAL_REENTRANT;
uint8_t Wire_requestFrom(uint8_t address, uint8_t quantity)
    STCXX_HAL_REENTRANT;
uint8_t Wire_requestFromStop(uint8_t address, uint8_t quantity,
                             uint8_t send_stop) STCXX_HAL_REENTRANT;
uint8_t Wire_requestFromInternal(uint8_t address, uint8_t quantity,
                                 uint32_t internal_address,
                                 uint8_t internal_address_size,
                                 uint8_t send_stop) STCXX_HAL_REENTRANT;
int Wire_available(void) STCXX_HAL_REENTRANT;
int Wire_peek(void) STCXX_HAL_REENTRANT;
int Wire_read(void) STCXX_HAL_REENTRANT;

#ifdef __cplusplus
}
#endif

#endif
