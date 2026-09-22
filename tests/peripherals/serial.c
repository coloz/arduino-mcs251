#include "Arduino.h"
#define STC_SERIAL_HOST_TEST
static uint8_t xfr[4096];
#define STC_XFR8(addr) xfr[(addr) & 4095u]
static uint8_t IE, IE2, P_SW2, AUXR, SCON, INTCLKO;
static uint8_t S2CON, S2BUF, S3CON, S3BUF, S4CON, S4BUF;
static uint8_t T4T3M, T2H, T2L, T3H, T3L, T4H, T4L;
static uint8_t modes[256], levels[256];
void pinMode(uint8_t pin, uint8_t mode) { modes[pin] = mode; }
void digitalWrite(uint8_t pin, uint8_t value) { levels[pin] = value; }
void stc_serial_test_transmit(uint8_t port, uint8_t value);
#include "../../cores/STC/hal/HardwareSerial_ports.c"
STC_IRQ_DATA stc_uart_service_t stc_uart_extra_service;
void stc_serial_test_transmit(uint8_t port, uint8_t value)
{
    (void)value;
    control_write(port, control_read(port) | 2u);
}
#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (0)
static void inject(uint8_t port, uint8_t value)
{
    if (port == 2) S2BUF = value;
    else if (port == 3) S3BUF = value;
    else if (port == 4) S4BUF = value;
#if STC_CORE_UART_COUNT > 4
    else STC_XFR8(0x7ef741UL + (port - 5u) * 0x30UL) = value;
#endif
    control_write(port, control_read(port) | 1u);
    stc_uart_extra_service(port);
}
int run_tests(void)
{
    uint8_t p, i;
    IE = 0x82u; P_SW2 = 0x30u;
    CHECK(!SerialPort_begin(0,9600));
    CHECK(!SerialPort_begin(3,0));
    CHECK(!SerialPort_begin(3,0xffffffffUL));
    CHECK(!SerialPort_begin(3,1)); // timer period overflows
    CHECK(!SerialPort_begin(3,2000000)); // nearest period exceeds 3% error
    CHECK(!SerialPort_setPinsChecked(3,P3_0,P3_1));
#if STC_CORE_UART_AVAILABLE_MASK & 2
    AUXR = 0x10u; /* tone or another user already owns T2 */
    CHECK(!SerialPort_begin(2,9600)); CHECK(AUXR == 0x10u);
    AUXR = 0;
    CHECK(SerialPort_begin(2,9600));
    CHECK((AUXR & 0x1cu) == 0x14u);
    CHECK((((unsigned int)T2H << 8) | T2L) == 65536u - 313u);
#else
    CHECK(!SerialPort_begin(2,9600));
#endif
    CHECK(SerialPort_begin(3,19200)); CHECK(SerialPort_begin(4,38400));
    CHECK((T4T3M & 0xaau) == 0xaau); CHECK(P_SW2 == 0x30u);
    CHECK(!SerialPort_setPinsChecked(3,P0_0,P0_1));
    inject(3,0x31); inject(4,0x42);
    CHECK(SerialPort_available(3) == 1 && SerialPort_available(4) == 1);
    CHECK(SerialPort_peek(3) == 0x31); CHECK(SerialPort_read(3) == 0x31);
    CHECK(SerialPort_read(3) == -1 && SerialPort_read(4) == 0x42);
    for(i=0;i<32;i++) inject(3,i);
    CHECK(SerialPort_available(3) == SERIAL_RX_BUFFER_SIZE-1);
    CHECK(SerialPort_overflow(3)); CHECK(!SerialPort_overflow(3));
    for(i=0;i<SERIAL_RX_BUFFER_SIZE-1;i++) CHECK(SerialPort_read(3) == i);
    inject(3,99); CHECK(SerialPort_read(3) == 99); /* ring wraps */
    IE = 2; CHECK(SerialPort_write(3,0x5a) == 1); CHECK(S3BUF == 0x5a && IE == 2);
    SerialPort_end(3); CHECK((T4T3M & 0xf0u) == 0xa0u); CHECK((IE2 & 16u) != 0);
    CHECK(SerialPort_write(3,0x5a) == 0); CHECK(P_SW2 == 0x30u);
    SerialPort_end(4);
#if STC_CORE_UART_COUNT > 4
    STC_XFR8(0x7efdd0UL) = 0xffu; STC_XFR8(0x7efdd1UL) = 0xffu;
    CHECK(SerialPort_setPinsChecked(3,P8_6,P8_7));
    CHECK(SerialPort_begin(3,19200));
    CHECK((STC_XFR8(0x7efd69UL) & 0x40u) != 0);
    CHECK(STC_XFR8(0x7efdb8UL) == 1u);
    CHECK(STC_XFR8(0x7efdd0UL) == 0 && STC_XFR8(0x7efdd1UL) == 0);
    SerialPort_end(3); CHECK(STC_XFR8(0x7efd69UL) == 0);
    for(p=5;p<=8;p++) CHECK(SerialPort_begin(p,9600));
    CHECK(STC_XFR8(0x7efbb3UL) == 0xaau && STC_XFR8(0x7efbbbUL) == 0xaau);
    for(p=5;p<=8;p++) {
        CHECK(STC_XFR8(0x7ef76dUL+(p-5u)*0x30UL) == 1u);
        inject(p,p+0x40); CHECK(SerialPort_read(p) == p+0x40);
        CHECK(SerialPort_write(p,p) == 1);
    }
    SerialPort_end(5); CHECK(STC_XFR8(0x7efbb3UL) == 0xa0u);
    for(p=6;p<=8;p++) SerialPort_end(p);
#endif
    SerialPort_end(2);
    CHECK(T4T3M == 0 && AUXR == 0 && P_SW2 == 0x30u);
    return 0;
}
