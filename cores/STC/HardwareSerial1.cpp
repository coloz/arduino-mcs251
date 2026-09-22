/* Link UART1 only when its object/default constructor is used. */
#include "HardwareSerial.h"
#include "HardwareSerial_ports.h"
#include "hal/stc_c_hal.h"
static bool begin1(uint8_t, unsigned long baud) { Serial_end(); Serial_begin(baud); return Serial_active(); }
static void end1(uint8_t) { Serial_end(); }
static bool pins1(uint8_t, uint8_t rx, uint8_t tx) { return Serial_setPinsChecked(rx, tx); }
static int available1(uint8_t) { return Serial_available(); }
static int peek1(uint8_t) { return Serial_peek(); }
static int read1(uint8_t) { return Serial_read(); }
static int writable1(uint8_t) { return Serial_availableForWrite(); }
static void flush1(uint8_t) { Serial_flush(); }
static size_t write1(uint8_t, uint8_t value) { return Serial_write(value); }
static bool overflow1(uint8_t) { return Serial_overflow(); }
static const HardwareSerialBackend backend1 = {
    begin1, end1, pins1, available1, peek1, read1, writable1, flush1, write1, overflow1
};
HardwareSerial::HardwareSerial() : _port(1), _backend(&backend1), _configurationError(false) {}
HardwareSerial Serial1;
