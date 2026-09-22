#include "Arduino.h"
#include "HardwareSerial_private.h"
#include "stc_sfr.h"
#include "wiring_digital_private.h"
#include "stc_usb_service.h"
#include "HardwareSerial_ports.h"

static uint8_t serial_rx_pin = PIN_SERIAL_RX;
static uint8_t serial_tx_pin = PIN_SERIAL_TX;
static uint8_t serial_route;

bool Serial_setPinsChecked(uint8_t rx, uint8_t tx)
{
    uint8_t route = STC_VARIANT_UART1_ROUTE(rx, tx);
    if (stc_uart1_started || route == 255u) return false;
    serial_rx_pin = rx; serial_tx_pin = tx; serial_route = route;
    return true;
}

/* Timer1 baud generation uses the board-selected clock divider. */
#ifndef STC_SERIAL_TIMER1_CLOCK_DIVIDER
# if STC_CORE_TIMER1_IS_1T
#  define STC_SERIAL_TIMER1_CLOCK_DIVIDER 1UL
#else
#  define STC_SERIAL_TIMER1_CLOCK_DIVIDER 12UL
# endif
#endif

#if (STC_SERIAL_TIMER1_CLOCK_DIVIDER != 1UL) && \
    (STC_SERIAL_TIMER1_CLOCK_DIVIDER != 12UL)
# error "STC_SERIAL_TIMER1_CLOCK_DIVIDER must be 1 or 12"
#endif

#define STC_SERIAL_T1_CLOCK_OUTPUT 0x02u
#define STC_SERIAL_UART1_ROUTE     0xc0u
#define STC_SERIAL_TF1             0x80u

#if STC_CORE_HAS_UART1

/* Supported STC32/AI families use INTCLKO at 0x8f. */
__sfr __at (0x8f) STC_SERIAL_INTCLKO;

/* STC32G and AI8051U: TM1PS at 7EFEA1 divides Timer1's input clock.
 * Baud calculations require no prescaling, regardless of its previous user. */
static uint8_t stc_serial_prescaler(uint8_t value)
{
    uint8_t saved = P_SW2 & STC_P_SW2_EAXFR, previous;
    P_SW2 |= STC_P_SW2_EAXFR;
    previous = STC_XFR8(0x7efea1UL);
    STC_XFR8(0x7efea1UL) = value;
    P_SW2 = (P_SW2 & (uint8_t)~STC_P_SW2_EAXFR) | saved;
    return previous;
}

# if STC_CORE_SERIAL_BUFFERED_RX

/* Timer1 and UART1 routing state temporarily owned by buffered Serial. */
static uint8_t serial_saved_tmod;
static uint8_t serial_saved_th1;
static uint8_t serial_saved_tl1;
static uint8_t serial_saved_tr1;
static uint8_t serial_saved_tf1;
static uint8_t serial_saved_et1;
static uint8_t serial_saved_auxr;
static uint8_t serial_saved_intclko;
static uint8_t serial_saved_p_sw1;
static uint8_t serial_saved_prescaler;

static uint8_t stc_serial_rounded_divisor(unsigned long clock,
                                          unsigned long denominator,
                                          unsigned long *result) __reentrant
{
    unsigned long divisor;
    unsigned long remainder;
    unsigned long round_threshold;

    if (denominator == 0UL) {
        return 0u;
    }

    divisor = clock / denominator;
    /* divisor is floor(clock / denominator), so the product cannot overflow. */
    remainder = clock - divisor * denominator;
    round_threshold = (denominator / 2UL) + (denominator & 1UL);
    if (remainder >= round_threshold) {
        ++divisor;
    }
    *result = divisor;
    return 1u;
}

static uint8_t stc_serial_baud_error_is_acceptable(unsigned long denominator,
                                                    unsigned long divisor) __reentrant
{
    unsigned long generated_ticks = denominator * divisor;
    /* Exact three-percent bounds, evaluated at compile time. Splitting the
     * products keeps the arithmetic within 32 bits at all supported clocks. */
    return generated_ticks >= (F_CPU / 103UL) * 100UL + ((F_CPU % 103UL) * 100UL + 102UL) / 103UL &&
           generated_ticks <= (F_CPU / 97UL) * 100UL + ((F_CPU % 97UL) * 100UL) / 97UL;
}

static uint8_t stc_serial_calculate_reload(unsigned long baud,
                                           uint16_t *reload,
                                           uint8_t *double_baud) __reentrant
{
    unsigned long denominator;
    unsigned long divisor;

    if (baud == 0UL) {
        return 0u;
    }

    /* Official STC SDK formula: baud = F_CPU / (4 * Timer1 period). */
    if (baud > (0xffffffffUL / 4UL)) {
        return 0u;
    }
    denominator = baud * 4UL;
    if (stc_serial_rounded_divisor((unsigned long)F_CPU,
                                   denominator, &divisor) == 0u) {
        return 0u;
    }
    if ((divisor == 0UL) || (divisor >= 65536UL) ||
        (stc_serial_baud_error_is_acceptable(denominator, divisor) == 0u)) {
        return 0u;
    }
    *reload = (uint16_t)(65536UL - divisor);
    *double_baud = 0u;
    return 1u;
}

static void stc_serial_reset_rx(void)
{
    stc_uart1_rx_head = 0u;
    stc_uart1_rx_tail = 0u;
    stc_uart1_rx_overflow = 0u;
}

static void stc_serial_wait_for_tx(void)
{
    while (stc_uart1_tx_complete == 0u) {
        /* Also service TI by polling so writes work with EA disabled. */
        if ((SCON & STC_SCON_TI) != 0u) {
            SCON &= (uint8_t)~STC_SCON_TI;
            stc_uart1_tx_complete = 1u;
        }
    }
}

#else

/*
 * The <=2 KiB profile accepts the common rates below. Every reload expression
 * is a compile-time constant, avoiding the 32-bit divide runtime that would
 * otherwise consume a material fraction of a 2 KiB device.
 */
#   define STC_SERIAL_COMPACT_DIVISOR(rate) \
        (((unsigned long)F_CPU + ((unsigned long)(rate) * 2UL)) / \
         ((unsigned long)(rate) * 4UL))
#   define STC_SERIAL_COMPACT_GENERATED(rate) \
        ((unsigned long)(rate) * 4UL * STC_SERIAL_COMPACT_DIVISOR(rate))
#   define STC_SERIAL_COMPACT_TOLERANCE(generated)                    \
        ((((generated) / 100UL) * 3UL) +                              \
         (((((generated) % 100UL) * 3UL) + 99UL) / 100UL))
#   define STC_SERIAL_COMPACT_ERROR_OK(generated)                     \
        (((unsigned long)F_CPU >= (generated))                        \
             ? (((unsigned long)F_CPU - (generated)) <=               \
                STC_SERIAL_COMPACT_TOLERANCE(generated))              \
             : (((generated) - (unsigned long)F_CPU) <=               \
                STC_SERIAL_COMPACT_TOLERANCE(generated)))
#   define STC_SERIAL_COMPACT_CASE(rate)                              \
        case (rate):                                                  \
            divisor = STC_SERIAL_COMPACT_DIVISOR(rate);               \
            if (!STC_SERIAL_COMPACT_ERROR_OK(                         \
                    STC_SERIAL_COMPACT_GENERATED(rate))) {            \
                return 0u;                                            \
            }                                                         \
            break

static uint8_t stc_serial_calculate_reload(unsigned long baud,
                                           uint16_t *reload,
                                           uint8_t *double_baud) __reentrant
{
    unsigned long divisor;

    switch (baud) {
    STC_SERIAL_COMPACT_CASE(4800UL);
    STC_SERIAL_COMPACT_CASE(9600UL);
    STC_SERIAL_COMPACT_CASE(19200UL);
    STC_SERIAL_COMPACT_CASE(38400UL);
    STC_SERIAL_COMPACT_CASE(57600UL);
    STC_SERIAL_COMPACT_CASE(115200UL);
    default:
        return 0u;
    }

    *double_baud = 0u;
    if ((divisor == 0UL) || (divisor >= 65536UL)) {
        return 0u;
    }
    *reload = (uint16_t)(65536UL - divisor);
    return 1u;
}

#  undef STC_SERIAL_COMPACT_CASE
#   undef STC_SERIAL_COMPACT_ERROR_OK
#   undef STC_SERIAL_COMPACT_TOLERANCE
#   undef STC_SERIAL_COMPACT_GENERATED
#   undef STC_SERIAL_COMPACT_DIVISOR

# endif /* STC_CORE_SERIAL_BUFFERED_RX */
#endif /* STC_CORE_HAS_UART1 */

#if STC_CORE_HAS_UART1
static void stc_serial_configure_uart1_pins(void)
{
    uint8_t saved_ea = (uint8_t)(IE & STC_IE_EA);

    if (serial_route != 0u) {
        digitalWrite(serial_tx_pin, HIGH);
        pinMode(serial_tx_pin, OUTPUT);
        pinMode(serial_rx_pin, INPUT_PULLUP);
        return;
    }

    IE &= (uint8_t)~STC_IE_EA;

    /* Match pinMode(P3.0, INPUT_PULLUP) without linking all digital APIs. */
    P3 |= 0x01u;
# if STC_CORE_HAS_PORT_MODE
#  if STC_CORE_HAS_SEPARATE_PULLUP
    P3M1 = (uint8_t)((P3M1 & (uint8_t)~0x03u) | 0x01u);
    P3PU = (uint8_t)((P3PU & (uint8_t)~0x03u) | 0x01u);
#else
    P3M1 &= (uint8_t)~0x03u;
#  endif
    /* Match pinMode(P3.1, OUTPUT), preserving the existing TX latch. */
    P3M0 = (uint8_t)((P3M0 & (uint8_t)~0x03u) | 0x02u);
# endif
    __stc_digital_input_pins[3] =
        (uint8_t)((__stc_digital_input_pins[3] | 0x01u) &
                  (uint8_t)~0x02u);

    if (saved_ea != 0u) {
        IE |= STC_IE_EA;
    }
}
#endif

void Serial_begin(unsigned long baud)
{
#if STC_CORE_HAS_UART1
    uint16_t reload;
    uint8_t double_baud;

#if STC_CORE_USB_LAYOUT
    if (stc_usb_active && serial_route == 0u) return;
#endif

    if (stc_serial_calculate_reload(baud, &reload, &double_baud) == 0u) {
        return;
    }

    if (stc_uart1_started != 0u) {
        Serial_end();
    }

# if STC_CORE_SERIAL_BUFFERED_RX
    serial_saved_tmod = TMOD;
    serial_saved_th1 = TH1;
    serial_saved_tl1 = TL1;
    serial_saved_tr1 = (uint8_t)(TCON & STC_TCON_TR1);
    serial_saved_tf1 = (uint8_t)(TCON & STC_SERIAL_TF1);
    serial_saved_et1 = (uint8_t)(IE & STC_IE_ET1);
    serial_saved_auxr = AUXR;
    serial_saved_intclko = STC_SERIAL_INTCLKO;
    serial_saved_p_sw1 = P_SW1;
# endif

    IE &= (uint8_t)~STC_IE_ES;
    TCON &= (uint8_t)~(STC_TCON_TR1 | STC_SERIAL_TF1);
    IE &= (uint8_t)~STC_IE_ET1;
# if STC_CORE_SERIAL_BUFFERED_RX
    serial_saved_prescaler = stc_serial_prescaler(0u);
# else
    (void)stc_serial_prescaler(0u);
# endif

    /* Select Timer1 rather than Timer2 and disable Timer1 clock output. */
    AUXR &= (uint8_t)~STC_AUXR_S1_BRT_T2;
#  if STC_CORE_TIMER1_IS_1T
    AUXR |= STC_AUXR_T1_1T;
#else
    AUXR &= (uint8_t)~STC_AUXR_T1_1T;
#  endif
    STC_SERIAL_INTCLKO &= (uint8_t)~STC_SERIAL_T1_CLOCK_OUTPUT;

    /* UART1 route is controlled by P_SW1. */
    P_SW1 = (P_SW1 & (uint8_t)~STC_SERIAL_UART1_ROUTE) | (serial_route << 6);
    TMOD &= 0x0fu;             /* Timer1, 16-bit auto reload, not gated. */
    TH1 = (uint8_t)(reload >> 8);
    TL1 = (uint8_t)reload;

# if STC_CORE_SERIAL_BUFFERED_RX
    stc_serial_configure_uart1_pins();
    stc_serial_reset_rx();
    stc_uart1_tx_complete = 1u;
#else
    stc_serial_configure_uart1_pins();
    stc_uart1_peek_valid = 0u;
# endif
    SCON = STC_SCON_MODE1 | STC_SCON_REN;
    stc_uart1_started = 1u;
#if STC_CORE_USB_LAYOUT
    stc_usb_uart1_active = serial_route == 0u;
#endif
# if STC_CORE_SERIAL_BUFFERED_RX
    IE |= STC_IE_ES;
# endif
    TCON |= STC_TCON_TR1;
#else
    (void)baud;
#endif
}

void Serial_end(void)
{
#if STC_CORE_HAS_UART1
    if (stc_uart1_started == 0u) {
        return;
    }

# if STC_CORE_SERIAL_BUFFERED_RX
    stc_serial_wait_for_tx();
# endif
    IE &= (uint8_t)~STC_IE_ES;
    TCON &= (uint8_t)~(STC_TCON_TR1 | STC_SERIAL_TF1);
    SCON &= (uint8_t)~(STC_SCON_REN | STC_SCON_RI | STC_SCON_TI);
    stc_uart1_started = 0u;
#if STC_CORE_USB_LAYOUT
    stc_usb_uart1_active = 0u;
#endif

# if STC_CORE_SERIAL_BUFFERED_RX
    stc_serial_reset_rx();
    stc_uart1_tx_complete = 1u;

    TMOD = (uint8_t)((TMOD & 0x0fu) | (serial_saved_tmod & 0xf0u));
    TH1 = serial_saved_th1;
    TL1 = serial_saved_tl1;
    AUXR = (uint8_t)((AUXR &
                      (uint8_t)~(STC_AUXR_T1_1T | STC_AUXR_S1_BRT_T2)) |
                     (serial_saved_auxr &
                      (STC_AUXR_T1_1T | STC_AUXR_S1_BRT_T2)));
    STC_SERIAL_INTCLKO =
        (uint8_t)((STC_SERIAL_INTCLKO &
                   (uint8_t)~STC_SERIAL_T1_CLOCK_OUTPUT) |
                  (serial_saved_intclko & STC_SERIAL_T1_CLOCK_OUTPUT));
    P_SW1 = (uint8_t)((P_SW1 & (uint8_t)~STC_SERIAL_UART1_ROUTE) |
                      (serial_saved_p_sw1 & STC_SERIAL_UART1_ROUTE));
    IE = (uint8_t)((IE & (uint8_t)~STC_IE_ET1) | serial_saved_et1);
    (void)stc_serial_prescaler(serial_saved_prescaler);
    TCON = (uint8_t)((TCON &
                      (uint8_t)~(STC_TCON_TR1 | STC_SERIAL_TF1)) |
                     serial_saved_tr1 | serial_saved_tf1);
#else
    stc_uart1_peek_valid = 0u;
# endif
#endif
}

int Serial_availableForWrite(void)
{
#if STC_CORE_HAS_UART1
# if STC_CORE_SERIAL_BUFFERED_RX
    return ((stc_uart1_started != 0u) && (stc_uart1_tx_complete != 0u)) ? 1 : 0;
#else
    return (stc_uart1_started != 0u) ? 1 : 0;
# endif
#else
    return 0;
#endif
}

size_t Serial_write(uint8_t value)
{
#if STC_CORE_HAS_UART1
    if (stc_uart1_started == 0u) {
        return 0u;
    }
# if STC_CORE_SERIAL_BUFFERED_RX
    stc_serial_wait_for_tx();
    stc_uart1_tx_complete = 0u;
    SCON &= (uint8_t)~STC_SCON_TI;
    SBUF = value;
    stc_serial_wait_for_tx();
#else
    SCON &= (uint8_t)~STC_SCON_TI;
    SBUF = value;
    while ((SCON & STC_SCON_TI) == 0u) {
    }
    SCON &= (uint8_t)~STC_SCON_TI;
# endif
    return 1u;
#else
    (void)value;
    return 0u;
#endif
}

bool Serial_active(void)
{
#if STC_CORE_HAS_UART1
    return stc_uart1_started != 0u;
#else
    return false;
#endif
}

void Serial_flush(void)
{
#if STC_CORE_HAS_UART1 && STC_CORE_SERIAL_BUFFERED_RX
    if (stc_uart1_started != 0u) {
        stc_serial_wait_for_tx();
    }
#endif
}
