#ifndef STC_SERIAL_PORTS_H
#define STC_SERIAL_PORTS_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
bool Serial_setPinsChecked(uint8_t rx, uint8_t tx);
bool SerialPort_begin(uint8_t port, unsigned long baud);
void SerialPort_end(uint8_t port);
bool SerialPort_setPinsChecked(uint8_t port, uint8_t rx, uint8_t tx);
int SerialPort_available(uint8_t port);
int SerialPort_peek(uint8_t port);
int SerialPort_read(uint8_t port);
size_t SerialPort_write(uint8_t port, uint8_t value);
int SerialPort_availableForWrite(uint8_t port);
void SerialPort_flush(uint8_t port);
bool SerialPort_overflow(uint8_t port);
#ifdef __cplusplus
}
#endif
#endif
