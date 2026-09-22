/*
 * SPDX-License-Identifier: MIT
 *
 * Clean-room polling software UART for STC targets.  Receive is intentionally
 * explicit: without a portable pin-change interrupt across the supported STC
 * families, poll()/available()/read() must observe the start bit in time.
 */
#include "SoftwareSerial_backend.h"
#include "stc_sfr.h"

#if defined(STC_XDATA_BYTES) && (STC_XDATA_BYTES > 0)
static __xdata uint8_t software_serial_rx_buffer[SOFTWARE_SERIAL_RX_BUFFER_SIZE];
#else
static uint8_t software_serial_rx_buffer[SOFTWARE_SERIAL_RX_BUFFER_SIZE];
#endif

static uint8_t software_serial_rx_pin = NOT_A_PIN;
static uint8_t software_serial_tx_pin = NOT_A_PIN;
static uint8_t software_serial_rx_port;
static uint8_t software_serial_tx_port;
static uint8_t software_serial_rx_mask;
static uint8_t software_serial_tx_mask;
static unsigned int software_serial_bit_period_us;
static unsigned int software_serial_half_period_us;
static uint8_t software_serial_inverse;
static uint8_t software_serial_started;
static uint8_t software_serial_listening;
static uint8_t software_serial_rx_head;
static uint8_t software_serial_rx_tail;
static uint8_t software_serial_rx_overflow;
static uint8_t software_serial_framing_error;
static uint8_t software_serial_timing_error;
static uint8_t software_serial_waiting_for_idle;

#define SOFTWARE_SERIAL_WAIT_SPIN_LIMIT 4096u

static uint8_t software_serial_next_index(uint8_t index)
{
    ++index;
    if (index >= (uint8_t)SOFTWARE_SERIAL_RX_BUFFER_SIZE) {
        index = 0u;
    }
    return index;
}

static uint8_t software_serial_port_read(uint8_t port)
{
    switch (port) {
    case 0u: return P0;
    case 1u: return P1;
    case 2u: return P2;
    case 3u: return P3;
#if STC_CORE_HAS_PORT4
    case 4u: return P4;
#endif
#if STC_CORE_HAS_PORT5
    case 5u: return P5;
#endif
#if STC_CORE_HAS_PORT6
    case 6u: return P6;
#endif
#if STC_CORE_HAS_PORT7
    case 7u: return P7;
#endif
#if STC_CORE_HAS_PORT8
    case 8u: return P8IN;
#endif
#if STC_CORE_HAS_PORT9
    case 9u: return P9IN;
#endif
#if STC_CORE_HAS_PORTA
    case 10u: return PAIN;
#endif
#if STC_CORE_HAS_PORTB
    case 11u: return PBIN;
#endif
    default: return 0xffu;
    }
}

static void software_serial_port_write(uint8_t port, uint8_t mask,
                                       uint8_t high)
{
    uint8_t saved_ea = (uint8_t)(IE & STC_IE_EA);

    IE &= (uint8_t)~STC_IE_EA;
    switch (port) {
    case 0u:
        if (high != 0u) { P0 |= mask; } else { P0 &= (uint8_t)~mask; }
        break;
    case 1u:
        if (high != 0u) { P1 |= mask; } else { P1 &= (uint8_t)~mask; }
        break;
    case 2u:
        if (high != 0u) { P2 |= mask; } else { P2 &= (uint8_t)~mask; }
        break;
    case 3u:
        if (high != 0u) { P3 |= mask; } else { P3 &= (uint8_t)~mask; }
        break;
#if STC_CORE_HAS_PORT4
    case 4u:
        if (high != 0u) { P4 |= mask; } else { P4 &= (uint8_t)~mask; }
        break;
#endif
#if STC_CORE_HAS_PORT5
    case 5u:
        if (high != 0u) { P5 |= mask; } else { P5 &= (uint8_t)~mask; }
        break;
#endif
#if STC_CORE_HAS_PORT6
    case 6u:
        if (high != 0u) { P6 |= mask; } else { P6 &= (uint8_t)~mask; }
        break;
#endif
#if STC_CORE_HAS_PORT7
    case 7u:
        if (high != 0u) { P7 |= mask; } else { P7 &= (uint8_t)~mask; }
        break;
#endif
#if STC_CORE_HAS_PORT8
    case 8u:
        if (high != 0u) { P8SETB = mask; } else { P8CLRB = mask; }
        break;
#endif
#if STC_CORE_HAS_PORT9
    case 9u:
        if (high != 0u) { P9SETB = mask; } else { P9CLRB = mask; }
        break;
#endif
#if STC_CORE_HAS_PORTA
    case 10u:
        if (high != 0u) { PASETB = mask; } else { PACLRB = mask; }
        break;
#endif
#if STC_CORE_HAS_PORTB
    case 11u:
        if (high != 0u) { PBSETB = mask; } else { PBCLRB = mask; }
        break;
#endif
    default:
        break;
    }
    if (saved_ea != 0u) {
        IE |= STC_IE_EA;
    }
}

static uint8_t software_serial_read_logical(void)
{
    uint8_t physical_high =
        ((software_serial_port_read(software_serial_rx_port) &
          software_serial_rx_mask) != 0u) ? 1u : 0u;

    return (uint8_t)(physical_high ^ software_serial_inverse);
}

static void software_serial_write_logical(uint8_t logical_high)
{
    software_serial_port_write(
        software_serial_tx_port,
        software_serial_tx_mask,
        (uint8_t)((logical_high != 0u) ^ software_serial_inverse));
}

static uint8_t software_serial_timer_ready(void)
{
    return (((IE & (STC_IE_EA | STC_IE_ET0)) ==
             (STC_IE_EA | STC_IE_ET0)) &&
            ((TCON & STC_TCON_TR0) != 0u)) ? 1u : 0u;
}

static uint8_t software_serial_wait_until(unsigned long deadline)
{
    unsigned int spins = 0u;

    while ((long)(micros() - deadline) < 0L) {
        if (software_serial_timer_ready() == 0u) {
            return 0u;
        }
        ++spins;
        if (spins >= (unsigned int)SOFTWARE_SERIAL_WAIT_SPIN_LIMIT) {
            return 0u;
        }
    }
    return 1u;
}

static void software_serial_reset_receive(void)
{
    software_serial_rx_head = 0u;
    software_serial_rx_tail = 0u;
    software_serial_rx_overflow = 0u;
    software_serial_framing_error = 0u;
    software_serial_timing_error = 0u;
    software_serial_waiting_for_idle = 0u;
}

static uint8_t software_serial_pins_are_valid(uint8_t receive_pin,
                                               uint8_t transmit_pin)
{
    if ((digitalPinIsValid(receive_pin) == 0u) ||
        (digitalPinIsValid(transmit_pin) == 0u)) {
        return 0u;
    }
    if ((receive_pin == transmit_pin) ||
        digitalPinsSharePhysicalPad(receive_pin, transmit_pin)) {
        return 0u;
    }
    return 1u;
}

static uint8_t software_serial_period_for_baud(unsigned long baud,
                                                unsigned int *period)
{
    unsigned long rounded_period;
    unsigned long remainder;

    if ((baud == 0UL) || (period == NULL)) {
        return 0u;
    }

    /* Round 1,000,000 / baud without overflowing for a large baud value. */
    rounded_period = 1000000UL / baud;
    remainder = 1000000UL % baud;
    if (remainder >= baud - remainder) {
        ++rounded_period;
    }
    if ((rounded_period < (unsigned long)SOFTWARE_SERIAL_MIN_BIT_PERIOD_US) ||
        (rounded_period > (unsigned long)SOFTWARE_SERIAL_MAX_BIT_PERIOD_US) ||
        (rounded_period > 65535UL)) {
        return 0u;
    }
    *period = (unsigned int)rounded_period;
    return 1u;
}

static void software_serial_start_validated(unsigned int period)
{
    if (software_serial_started != 0u) {
        SoftwareSerial_end();
    }

    software_serial_bit_period_us = period;
    software_serial_half_period_us = (unsigned int)((period + 1u) / 2u);
    software_serial_rx_port = digitalPinToPort(software_serial_rx_pin);
    software_serial_tx_port = digitalPinToPort(software_serial_tx_pin);
    software_serial_rx_mask = digitalPinToBitMask(software_serial_rx_pin);
    software_serial_tx_mask = digitalPinToBitMask(software_serial_tx_pin);

    /* Preload the latch before enabling push-pull output to avoid a false
     * start-bit pulse when the old latch held the opposite level. */
    software_serial_port_write(software_serial_tx_port,
                               software_serial_tx_mask,
                               (software_serial_inverse != 0u) ? LOW : HIGH);
    pinMode(software_serial_tx_pin, OUTPUT);
    pinMode(software_serial_rx_pin,
            (software_serial_inverse != 0u) ? INPUT : INPUT_PULLUP);

    software_serial_reset_receive();
    software_serial_started = 1u;
    software_serial_listening = 1u;
}

bool SoftwareSerial_beginOnPins(uint8_t receive_pin, uint8_t transmit_pin,
                                bool inverse_logic, unsigned long baud)
                                STC_SOFTWARE_SERIAL_REENTRANT
{
    unsigned int period;

    /* Do every fallible check before ending or reconfiguring the current
     * owner.  Once these checks pass, the remaining transition cannot fail. */
    if ((software_serial_pins_are_valid(receive_pin, transmit_pin) == 0u) ||
        (software_serial_timer_ready() == 0u) ||
        (software_serial_period_for_baud(baud, &period) == 0u)) {
        return false;
    }
    if (software_serial_started != 0u) {
        SoftwareSerial_end();
    }
    software_serial_rx_pin = receive_pin;
    software_serial_tx_pin = transmit_pin;
    software_serial_inverse = inverse_logic ? 1u : 0u;
    software_serial_start_validated(period);
    return true;
}

void SoftwareSerial_end(void)
{
    if (software_serial_started != 0u) {
        software_serial_write_logical(HIGH);
        pinMode(software_serial_tx_pin, INPUT);
        pinMode(software_serial_rx_pin, INPUT);
    }
    software_serial_started = 0u;
    software_serial_listening = 0u;
    software_serial_bit_period_us = 0u;
    software_serial_half_period_us = 0u;
    software_serial_reset_receive();
}

bool SoftwareSerial_listen(void)
{
    if ((software_serial_started == 0u) ||
        (software_serial_listening != 0u)) {
        return false;
    }
    software_serial_reset_receive();
    software_serial_listening = 1u;
    return true;
}

bool SoftwareSerial_stopListening(void)
{
    uint8_t was_listening = software_serial_listening;

    software_serial_listening = 0u;
    return (was_listening != 0u);
}

bool SoftwareSerial_isListening(void)
{
    return (software_serial_started != 0u) &&
           (software_serial_listening != 0u);
}

size_t SoftwareSerial_poll(void)
{
    unsigned long deadline;
    uint8_t value = 0u;
    uint8_t mask;
    uint8_t next;

    if (!SoftwareSerial_isListening()) {
        return 0u;
    }
    /* Timer0 must be running and serviceable during a blocking frame. */
    if (software_serial_timer_ready() == 0u) {
        software_serial_timing_error = 1u;
        return 0u;
    }
    if (software_serial_waiting_for_idle != 0u) {
        if (software_serial_read_logical() == HIGH) {
            software_serial_waiting_for_idle = 0u;
        }
        return 0u;
    }
    if (software_serial_read_logical() != LOW) {
        return 0u;
    }

    deadline = micros() + (unsigned long)software_serial_half_period_us;
    if (software_serial_wait_until(deadline) == 0u) {
        software_serial_timing_error = 1u;
        return 0u;
    }
    if (software_serial_read_logical() != LOW) {
        return 0u;
    }

    for (mask = 0x01u; mask != 0u; mask <<= 1) {
        deadline += (unsigned long)software_serial_bit_period_us;
        if (software_serial_wait_until(deadline) == 0u) {
            software_serial_timing_error = 1u;
            return 0u;
        }
        if (software_serial_read_logical() != LOW) {
            value |= mask;
        }
    }

    deadline += (unsigned long)software_serial_bit_period_us;
    if (software_serial_wait_until(deadline) == 0u) {
        software_serial_timing_error = 1u;
        return 0u;
    }
    if (software_serial_read_logical() != HIGH) {
        software_serial_framing_error = 1u;
        software_serial_waiting_for_idle = 1u;
        return 0u;
    }

    next = software_serial_next_index(software_serial_rx_head);
    if (next == software_serial_rx_tail) {
        software_serial_rx_overflow = 1u;
        return 0u;
    }
    software_serial_rx_buffer[software_serial_rx_head] = value;
    software_serial_rx_head = next;
    return 1u;
}

int SoftwareSerial_available(void)
{
    uint8_t count;

    /* Service reception from the conventional Stream API too. This remains
     * polling (not background IRQ reception), but an ordinary available/read
     * loop no longer requires a nonstandard explicit poll() call. */
    (void)SoftwareSerial_poll();

    if (software_serial_rx_head >= software_serial_rx_tail) {
        count = (uint8_t)(software_serial_rx_head - software_serial_rx_tail);
    } else {
        count = (uint8_t)(SOFTWARE_SERIAL_RX_BUFFER_SIZE -
                          software_serial_rx_tail + software_serial_rx_head);
    }
    return (int)count;
}

int SoftwareSerial_availableForWrite(void)
{
    return ((software_serial_started != 0u) &&
            (software_serial_timer_ready() != 0u)) ? 1 : 0;
}

int SoftwareSerial_peek(void)
{
    if (SoftwareSerial_available() == 0) {
        return -1;
    }
    return (int)software_serial_rx_buffer[software_serial_rx_tail];
}

int SoftwareSerial_read(void)
{
    uint8_t value;

    if (SoftwareSerial_available() == 0) {
        return -1;
    }
    value = software_serial_rx_buffer[software_serial_rx_tail];
    software_serial_rx_tail =
        software_serial_next_index(software_serial_rx_tail);
    return (int)value;
}

size_t SoftwareSerial_write(uint8_t value)
{
    unsigned long deadline;
    uint8_t mask;

    if ((software_serial_started == 0u) ||
        (software_serial_timer_ready() == 0u)) {
        software_serial_timing_error = 1u;
        return 0u;
    }

    deadline = micros() + (unsigned long)software_serial_bit_period_us;
    software_serial_write_logical(LOW);
    if (software_serial_wait_until(deadline) == 0u) {
        software_serial_write_logical(HIGH);
        software_serial_timing_error = 1u;
        return 0u;
    }

    for (mask = 0x01u; mask != 0u; mask <<= 1) {
        software_serial_write_logical((value & mask) != 0u ? HIGH : LOW);
        deadline += (unsigned long)software_serial_bit_period_us;
        if (software_serial_wait_until(deadline) == 0u) {
            software_serial_write_logical(HIGH);
            software_serial_timing_error = 1u;
            return 0u;
        }
    }

    software_serial_write_logical(HIGH);
    deadline += (unsigned long)software_serial_bit_period_us;
    if (software_serial_wait_until(deadline) == 0u) {
        software_serial_timing_error = 1u;
        return 0u;
    }
    return 1u;
}

void SoftwareSerial_flush(void)
{
    /* Transmission is synchronous, so there is nothing pending to flush. */
}

bool SoftwareSerial_overflow(void)
{
    uint8_t result = software_serial_rx_overflow;

    software_serial_rx_overflow = 0u;
    return (result != 0u);
}

bool SoftwareSerial_framingError(void)
{
    uint8_t result = software_serial_framing_error;

    software_serial_framing_error = 0u;
    return (result != 0u);
}

bool SoftwareSerial_timingError(void)
{
    uint8_t result = software_serial_timing_error;

    software_serial_timing_error = 0u;
    return (result != 0u);
}
