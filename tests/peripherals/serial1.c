/* Production UART1 initialization with emulated SFRs and an audited XFR gate. */
#include "Arduino.h"
#define __SDCC 1
#define __sfr uint8_t
#define __at(address)
#define __reentrant
#include "stc_sfr.h"
#undef STC_XFR8
static uint8_t xfr[4096], gate_error;
static uint8_t *xfr_byte(unsigned long address)
{
    if (address == 0x7efea1UL && !(P_SW2 & 0x80u)) gate_error = 1;
    return &xfr[address & 4095u];
}
#define STC_XFR8(address) (*xfr_byte(address))
uint8_t __stc_digital_input_pins[12];
uint8_t stc_usb_active, stc_usb_uart1_active;
volatile uint8_t stc_uart1_started, stc_uart1_rx_head, stc_uart1_rx_tail;
volatile uint8_t stc_uart1_rx_overflow, stc_uart1_tx_complete;
void pinMode(uint8_t pin, uint8_t mode) { (void)pin; (void)mode; }
void digitalWrite(uint8_t pin, uint8_t value) { (void)pin; (void)value; }
void Serial_end(void);
#include "../../cores/STC/hal/HardwareSerial.c"
#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (0)
int run_tests(void)
{
    uint8_t gate, enabled;
    CHECK(Serial_setPinsChecked(P3_6, P3_7));
    /* Test both EAXFR states and both global-interrupt states. */
    for (gate = 0; gate < 2; ++gate) for (enabled = 0; enabled < 2; ++enabled) {
        P_SW2 = 0x35u | (gate ? 0x80u : 0u);
        IE = 0x0au | (enabled ? 0x80u : 0u);
        TMOD = 0x12u; TH1 = 0x12u; TL1 = 0x34u; TCON = 0xc0u;
        xfr[0xea1] = 7u;
        Serial_begin(0); /* Rejected configuration must not change TM1PS. */
        CHECK(!Serial_active() && xfr[0xea1] == 7u);
        Serial_begin(9600);
        CHECK(Serial_active() && xfr[0xea1] == 0u);
        CHECK((((unsigned int)TH1 << 8) | TL1) == 65536u - 313u);
        CHECK(P_SW2 == (0x35u | (gate ? 0x80u : 0u)));
        CHECK((IE & 0x80u) == (enabled ? 0x80u : 0u));
        Serial_begin(19200); /* Restart still preserves the original owner. */
        CHECK(xfr[0xea1] == 0u);
        Serial_end();
        CHECK(!Serial_active() && xfr[0xea1] == 7u);
        CHECK(TMOD == 0x12u && TH1 == 0x12u && TL1 == 0x34u && TCON == 0xc0u);
        CHECK(P_SW2 == (0x35u | (gate ? 0x80u : 0u)));
        CHECK(IE == (0x0au | (enabled ? 0x80u : 0u)));
    }
    CHECK(!gate_error);
    return 0;
}
