#ifndef STC_HARDWARE_SERIAL_BACKEND_H
#define STC_HARDWARE_SERIAL_BACKEND_H

/* Internal UART backend used by HardwareSerial and the C runtime. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef STC_CORE_SERIAL_BUFFERED_RX
# define STC_CORE_SERIAL_BUFFERED_RX 1
#endif

/*
 * UART1 uses Timer1 while Serial is active and transmit is synchronous.
 * Receive uses the board-selected interrupt buffer or polling backend.
 */
void Serial_begin(unsigned long baud);
void Serial_end(void);
bool Serial_active(void);
int Serial_available(void);
int Serial_availableForWrite(void);
int Serial_peek(void);
int Serial_read(void);
size_t Serial_write(uint8_t value);
void Serial_flush(void);

/* Returns the sticky RX-overflow state and clears it. */
bool Serial_overflow(void);

#endif
