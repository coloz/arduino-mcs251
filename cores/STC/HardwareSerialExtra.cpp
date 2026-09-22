/* C++ callbacks retain the bridge ABI at the native C boundary. */
#include "HardwareSerial.h"
#include "HardwareSerial_ports.h"
static bool beginPort(uint8_t p, unsigned long b) { return SerialPort_begin(p,b); }
static void endPort(uint8_t p) { SerialPort_end(p); }
static bool pinsPort(uint8_t p,uint8_t rx,uint8_t tx) { return SerialPort_setPinsChecked(p,rx,tx); }
static int availablePort(uint8_t p) { return SerialPort_available(p); }
static int peekPort(uint8_t p) { return SerialPort_peek(p); }
static int readPort(uint8_t p) { return SerialPort_read(p); }
static int writablePort(uint8_t p) { return SerialPort_availableForWrite(p); }
static void flushPort(uint8_t p) { SerialPort_flush(p); }
static size_t writePort(uint8_t p,uint8_t v) { return SerialPort_write(p,v); }
static bool overflowPort(uint8_t p) { return SerialPort_overflow(p); }
static const HardwareSerialBackend backend = {
    beginPort,endPort,pinsPort,availablePort,peekPort,readPort,writablePort,flushPort,writePort,overflowPort
};
HardwareSerial::HardwareSerial(uint8_t port) : _port(port), _backend(&backend),
    _configurationError(!(port >= 2u && port <= STC_CORE_UART_COUNT &&
                           (STC_CORE_UART_AVAILABLE_MASK & (1u << (port - 1u))))) {}
#if STC_CORE_UART_AVAILABLE_MASK & 2
HardwareSerial Serial2(2);
#endif
HardwareSerial Serial3(3);
HardwareSerial Serial4(4);
#if STC_CORE_UART_COUNT > 4
HardwareSerial Serial5(5);
HardwareSerial Serial6(6);
HardwareSerial Serial7(7);
HardwareSerial Serial8(8);
#endif
