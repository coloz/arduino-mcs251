/* Exercise the production C++ dispatch against a counting C HAL. */
#include "../../cores/STC/HardwareSerial.cpp"
#include "../../cores/STC/HardwareSerial1.cpp"
#include "../../cores/STC/HardwareSerialExtra.cpp"
#include "../../libraries/Wire/src/WireClass.cpp"

static unsigned wire_calls[2], serial_calls;
static unsigned last_serial_port;
size_t Print::write(const uint8_t *buffer, size_t size)
{
    size_t written = 0;
    while (written < size && write(buffer[written])) ++written;
    return written;
}
extern "C" void __cxa_pure_virtual() { __builtin_trap(); }
extern "C" {
void Serial_begin(unsigned long) { ++serial_calls; last_serial_port = 1; }
bool Serial_active(void) { return true; }
void Serial_end(void) {}
bool Serial_setPinsChecked(uint8_t, uint8_t) { return true; }
int Serial_available(void) { return 0; }
int Serial_availableForWrite(void) { return 1; }
int Serial_peek(void) { return -1; }
int Serial_read(void) { return -1; }
size_t Serial_write(uint8_t) { return 1; }
void Serial_flush(void) {}
bool Serial_overflow(void) { return false; }
bool SerialPort_begin(uint8_t port, unsigned long)
{
    if (port < 2 || port > STC_CORE_UART_COUNT ||
        !(STC_CORE_UART_AVAILABLE_MASK & (1u << (port - 1u)))) return false;
    ++serial_calls; last_serial_port = port; return true;
}
void SerialPort_end(uint8_t) {}
bool SerialPort_setPinsChecked(uint8_t, uint8_t, uint8_t) { return false; }
int SerialPort_available(uint8_t) { return 0; }
int SerialPort_peek(uint8_t) { return -1; }
int SerialPort_read(uint8_t) { return -1; }
size_t SerialPort_write(uint8_t, uint8_t) { return 0; }
int SerialPort_availableForWrite(uint8_t) { return 0; }
void SerialPort_flush(uint8_t) {}
bool SerialPort_overflow(uint8_t) { return false; }
#define WIRE_HAL(bus, name) \
void name##_begin(void) { ++wire_calls[bus]; } \
void name##_beginSlave(uint8_t) { ++wire_calls[bus]; } \
void name##_onReceive(void (*)(int)) { ++wire_calls[bus]; } \
void name##_onRequest(void (*)(void)) { ++wire_calls[bus]; } \
void name##_end(void) { ++wire_calls[bus]; } \
void name##_setPins(uint8_t, uint8_t) { ++wire_calls[bus]; } \
uint8_t name##_setPinsChecked(uint8_t, uint8_t) { ++wire_calls[bus]; return 0; } \
uint8_t name##_configurationError(void) { ++wire_calls[bus]; return 0; } \
uint8_t name##_lastError(void) { ++wire_calls[bus]; return 0; } \
void name##_setClock(unsigned long) { ++wire_calls[bus]; } \
void name##_setWireTimeout(uint32_t, uint8_t) { ++wire_calls[bus]; } \
uint8_t name##_getWireTimeoutFlag(void) { ++wire_calls[bus]; return 0; } \
void name##_clearWireTimeoutFlag(void) { ++wire_calls[bus]; } \
void name##_beginTransmission(uint8_t) { ++wire_calls[bus]; } \
uint8_t name##_endTransmissionStop(uint8_t) { ++wire_calls[bus]; return 0; } \
uint8_t name##_requestFromStop(uint8_t, uint8_t, uint8_t) { ++wire_calls[bus]; return 0; } \
uint8_t name##_requestFromInternal(uint8_t, uint8_t, uint32_t, uint8_t, uint8_t) { ++wire_calls[bus]; return 0; } \
size_t name##_write(uint8_t) { ++wire_calls[bus]; return 1; } \
int name##_available(void) { ++wire_calls[bus]; return 0; } \
int name##_read(void) { ++wire_calls[bus]; return -1; } \
int name##_peek(void) { ++wire_calls[bus]; return -1; }
WIRE_HAL(0, Wire)
#if STC_CORE_I2C_COUNT > 1
WIRE_HAL(1, Wire1)
#endif
}
#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (0)
static void received(int) {}
static void requested() {}
extern "C" int run_tests(void)
{
    HardwareSerial uart1;
    CHECK(!uart1.configurationError());
    uart1.begin(9600);
    CHECK(last_serial_port == 1 && serial_calls == 1);
    HardwareSerial numbered1(1), invalid0(0), invalid255(255);
    CHECK(numbered1.configurationError() && !numbered1);
    CHECK(invalid0.configurationError() && invalid255.configurationError());
    numbered1.begin(9600); invalid0.begin(9600); invalid255.begin(9600);
    CHECK(serial_calls == 1);
    HardwareSerial uart3(3);
    CHECK(!uart3.configurationError());
    uart3.begin(9600); CHECK(last_serial_port == 3 && serial_calls == 2);
    HardwareSerial uart2(2);
    CHECK(uart2.configurationError() == !(STC_CORE_UART_AVAILABLE_MASK & 2));

    TwoWire bus0;
    bus0.begin(); CHECK(wire_calls[0] == 1 && wire_calls[1] == 0);
#if STC_CORE_I2C_COUNT > 1
    TwoWire bus1(1);
    bus1.begin(); CHECK(wire_calls[0] == 1 && wire_calls[1] == 1);
#endif
    wire_calls[0] = wire_calls[1] = 0;
    for (unsigned n = STC_CORE_I2C_COUNT; n <= 255; ++n) {
        TwoWire invalid((uint8_t)n);
        uint8_t buffer[] = {1, 2};
        CHECK(invalid.configurationError() == WIRE_STATUS_OTHER_ERROR);
        CHECK(invalid.lastError() == WIRE_STATUS_OTHER_ERROR);
        invalid.begin(); invalid.begin(0x20); invalid.end();
        invalid.onReceive(received); invalid.onRequest(requested);
        invalid.setPins(0, 1); invalid.setClock(100000);
        CHECK(invalid.setPinsChecked(0, 1) == WIRE_STATUS_OTHER_ERROR);
        invalid.setWireTimeout(1000, true); invalid.clearWireTimeoutFlag();
        CHECK(!invalid.getWireTimeoutFlag());
        invalid.beginTransmission(0x20);
        CHECK(invalid.endTransmission() == WIRE_STATUS_OTHER_ERROR);
        CHECK(invalid.requestFrom(0x20, 1) == 0);
        CHECK(invalid.requestFrom(0x20, 1, 2, 1, 1) == 0);
        CHECK(invalid.write((uint8_t)1) == 0 && invalid.write(buffer, 2) == 0);
        CHECK(invalid.getWriteError());
        CHECK(invalid.available() == 0 && invalid.read() == -1 && invalid.peek() == -1);
    }
    CHECK(wire_calls[0] == 0 && wire_calls[1] == 0);
    return 0;
}
